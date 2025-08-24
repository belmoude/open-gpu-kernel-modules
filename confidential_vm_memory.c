#include "confidential_vm_memory.h"
#include <linux/slab.h>
#include <linux/rmap.h>
#include <linux/pagemap.h>
#include <linux/mmu_context.h>
#include <linux/sched/mm.h>
#include <linux/delay.h>

/* 全局统计和管理 */
static DEFINE_SPINLOCK(cvm_global_lock);
static atomic_long_t cvm_transition_count = ATOMIC_LONG_INIT(0);
static struct kmem_cache *cvm_entry_cache;

/* 初始化函数 */
static int __init cvm_memory_init(void)
{
    cvm_entry_cache = kmem_cache_create("cvm_transition_entry",
                                       sizeof(struct cvm_transition_entry),
                                       0, SLAB_HWCACHE_ALIGN, NULL);
    if (!cvm_entry_cache)
        return -ENOMEM;
    
    pr_info("CVM: Confidential VM memory management initialized\n");
    return 0;
}

static void __exit cvm_memory_exit(void)
{
    if (cvm_entry_cache)
        kmem_cache_destroy(cvm_entry_cache);
}

module_init(cvm_memory_init);
module_exit(cvm_memory_exit);

/**
 * cvm_transition_entry_alloc - 分配状态切换条目
 * @type: 切换类型
 * @page: 目标页面
 */
struct cvm_transition_entry *cvm_transition_entry_alloc(enum cvm_transition_type type,
                                                       struct page *page)
{
    struct cvm_transition_entry *entry;
    
    entry = kmem_cache_alloc(cvm_entry_cache, GFP_KERNEL);
    if (!entry)
        return NULL;
    
    entry->type = type;
    entry->page = page;
    init_waitqueue_head(&entry->wait_queue);
    atomic_set(&entry->ref_count, 1);
    spin_lock_init(&entry->lock);
    entry->start_time = jiffies;
    
    if (page)
        get_page(page);
    
    atomic_long_inc(&cvm_transition_count);
    
    return entry;
}

/**
 * cvm_transition_entry_free - 释放状态切换条目
 * @entry: 要释放的条目
 */
void cvm_transition_entry_free(struct cvm_transition_entry *entry)
{
    if (!entry)
        return;
    
    if (!atomic_dec_and_test(&entry->ref_count))
        return;
    
    if (entry->page)
        put_page(entry->page);
    
    kmem_cache_free(cvm_entry_cache, entry);
    atomic_long_dec(&cvm_transition_count);
}

/**
 * cvm_get_page_state - 获取页面当前的加密状态
 * @page: 目标页面
 */
enum cvm_page_state cvm_get_page_state(struct page *page)
{
    unsigned long flags = page->flags;
    
    /* 从页面flags中提取状态信息 */
    if (flags & (1UL << PG_private))
        return CVM_PAGE_PRIVATE;
    else if (flags & (1UL << PG_private_2))
        return CVM_PAGE_SHARED;
    else
        return CVM_PAGE_TRANSITIONING;
}

/**
 * cvm_set_page_state - 设置页面的加密状态
 * @page: 目标页面
 * @state: 新的状态
 */
int cvm_set_page_state(struct page *page, enum cvm_page_state state)
{
    unsigned long flags;
    
    lock_page(page);
    
    /* 清除旧状态 */
    ClearPagePrivate(page);
    ClearPagePrivate2(page);
    
    /* 设置新状态 */
    switch (state) {
    case CVM_PAGE_PRIVATE:
        SetPagePrivate(page);
        break;
    case CVM_PAGE_SHARED:
        SetPagePrivate2(page);
        break;
    case CVM_PAGE_TRANSITIONING:
        /* 不设置任何标志，表示过渡状态 */
        break;
    default:
        unlock_page(page);
        return -EINVAL;
    }
    
    unlock_page(page);
    return 0;
}

/**
 * cvm_replace_page_table_entries - 替换页表条目为状态切换条目
 * @mm: 内存管理结构
 * @start: 起始地址
 * @end: 结束地址
 * @transition: 状态切换上下文
 */
static int cvm_replace_page_table_entries(struct mm_struct *mm,
                                         unsigned long start,
                                         unsigned long end,
                                         struct cvm_state_transition *transition)
{
    struct vm_area_struct *vma;
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte, *start_pte;
    spinlock_t *ptl;
    unsigned long addr;
    struct page *page;
    struct cvm_transition_entry *entry;
    pte_t old_pte, new_pte;
    int replaced = 0;
    
    mmap_read_lock(mm);
    
    for (addr = start; addr < end; addr += PAGE_SIZE) {
        vma = find_vma(mm, addr);
        if (!vma || addr < vma->vm_start)
            continue;
        
        pgd = pgd_offset(mm, addr);
        if (pgd_none(*pgd) || pgd_bad(*pgd))
            continue;
        
        p4d = p4d_offset(pgd, addr);
        if (p4d_none(*p4d) || p4d_bad(*p4d))
            continue;
        
        pud = pud_offset(p4d, addr);
        if (pud_none(*pud) || pud_bad(*pud))
            continue;
        
        pmd = pmd_offset(pud, addr);
        if (pmd_none(*pmd) || pmd_bad(*pmd))
            continue;
        
        start_pte = pte_offset_map_lock(mm, pmd, addr, &ptl);
        if (!start_pte)
            continue;
        
        pte = start_pte;
        old_pte = *pte;
        
        if (pte_none(old_pte) || !pte_present(old_pte)) {
            pte_unmap_unlock(start_pte, ptl);
            continue;
        }
        
        page = pte_page(old_pte);
        if (!page) {
            pte_unmap_unlock(start_pte, ptl);
            continue;
        }
        
        /* 创建状态切换条目 */
        entry = cvm_transition_entry_alloc(
            (transition->target_state == CVM_PAGE_SHARED) ? 
            CVM_TRANSITION_TO_SHARED : CVM_TRANSITION_TO_PRIVATE, page);
        
        if (!entry) {
            pte_unmap_unlock(start_pte, ptl);
            continue;
        }
        
        /* 创建新的PTE */
        new_pte = make_cvm_transition_pte(entry);
        
        /* 原子替换PTE */
        set_pte_at(mm, addr, pte, new_pte);
        
        /* 更新页面状态 */
        cvm_set_page_state(page, CVM_PAGE_TRANSITIONING);
        
        pte_unmap_unlock(start_pte, ptl);
        
        atomic_inc(&transition->pending_count);
        replaced++;
    }
    
    mmap_read_unlock(mm);
    
    /* 刷新TLB确保修改生效 */
    if (replaced > 0) {
        cvm_memory_barrier();
        flush_tlb_range(find_vma(mm, start), start, end);
    }
    
    return replaced;
}

/**
 * cvm_begin_state_transition - 开始页面状态切换
 * @vma: 虚拟内存区域
 * @start: 起始地址
 * @end: 结束地址
 * @target_state: 目标状态
 */
int cvm_begin_state_transition(struct vm_area_struct *vma,
                              unsigned long start, unsigned long end,
                              enum cvm_page_state target_state)
{
    struct cvm_state_transition *transition;
    struct mm_struct *mm = vma->vm_mm;
    int ret;
    
    if (!mm || start >= end)
        return -EINVAL;
    
    /* 分配状态切换上下文 */
    transition = kzalloc(sizeof(*transition), GFP_KERNEL);
    if (!transition)
        return -ENOMEM;
    
    transition->mm = mm;
    transition->start_addr = start;
    transition->end_addr = end;
    transition->target_state = target_state;
    INIT_LIST_HEAD(&transition->entry_list);
    INIT_LIST_HEAD(&transition->global_list);
    mutex_init(&transition->transition_mutex);
    atomic_set(&transition->pending_count, 0);
    init_completion(&transition->completion);
    transition->start_time = jiffies;
    transition->cancelled = false;
    transition->timeout = false;
    
    mmget(mm);
    
    mutex_lock(&transition->transition_mutex);
    
    /* 替换所有相关的页表条目 */
    ret = cvm_replace_page_table_entries(mm, start, end, transition);
    if (ret < 0) {
        mutex_unlock(&transition->transition_mutex);
        mmput(mm);
        kfree(transition);
        return ret;
    }
    
    /* 启动后台任务完成状态切换 */
    ret = cvm_schedule_transition(transition);
    if (ret < 0) {
        mutex_unlock(&transition->transition_mutex);
        mmput(mm);
        kfree(transition);
        return ret;
    }
    
    mutex_unlock(&transition->transition_mutex);
    
    pr_debug("CVM: Started state transition for range 0x%lx-0x%lx, %d pages affected\n",
             start, end, ret);
    
    return 0;
}

/**
 * cvm_complete_state_transition - 完成页面状态切换
 * @transition: 状态切换上下文
 */
int cvm_complete_state_transition(struct cvm_state_transition *transition)
{
    struct mm_struct *mm = transition->mm;
    unsigned long addr;
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte, *start_pte;
    spinlock_t *ptl;
    struct cvm_transition_entry *entry;
    struct page *page;
    pte_t old_pte, new_pte;
    int completed = 0;
    
    mutex_lock(&transition->transition_mutex);
    mmap_read_lock(mm);
    
    for (addr = transition->start_addr; 
         addr < transition->end_addr; 
         addr += PAGE_SIZE) {
        
        pgd = pgd_offset(mm, addr);
        if (pgd_none(*pgd) || pgd_bad(*pgd))
            continue;
        
        p4d = p4d_offset(pgd, addr);
        if (p4d_none(*p4d) || p4d_bad(*p4d))
            continue;
        
        pud = pud_offset(p4d, addr);
        if (pud_none(*pud) || pud_bad(*pud))
            continue;
        
        pmd = pmd_offset(pud, addr);
        if (pmd_none(*pmd) || pmd_bad(*pmd))
            continue;
        
        start_pte = pte_offset_map_lock(mm, pmd, addr, &ptl);
        if (!start_pte)
            continue;
        
        pte = start_pte;
        old_pte = *pte;
        
        if (!is_cvm_transition_pte(old_pte)) {
            pte_unmap_unlock(start_pte, ptl);
            continue;
        }
        
        entry = cvm_transition_pte_to_entry(old_pte);
        if (!entry) {
            pte_unmap_unlock(start_pte, ptl);
            continue;
        }
        
        page = entry->page;
        
        /* 执行实际的状态切换操作 */
        /* 这里需要调用具体的硬件接口来设置C-bit */
        cvm_set_page_state(page, transition->target_state);
        
        /* 恢复正常的PTE */
        new_pte = mk_pte(page, find_vma(mm, addr)->vm_page_prot);
        if (transition->target_state == CVM_PAGE_PRIVATE)
            new_pte = pte_mkwrite(new_pte);
        
        set_pte_at(mm, addr, pte, new_pte);
        
        /* 唤醒等待的进程 */
        wake_up_all(&entry->wait_queue);
        
        /* 释放状态切换条目 */
        cvm_transition_entry_free(entry);
        
        pte_unmap_unlock(start_pte, ptl);
        
        completed++;
        atomic_dec(&transition->pending_count);
    }
    
    mmap_read_unlock(mm);
    
    /* 再次刷新TLB确保所有修改生效 */
    if (completed > 0) {
        cvm_memory_barrier();
        flush_tlb_range(find_vma(mm, transition->start_addr), 
                       transition->start_addr, transition->end_addr);
    }
    
    complete(&transition->completion);
    mutex_unlock(&transition->transition_mutex);
    
    pr_debug("CVM: Completed state transition, %d pages processed\n", completed);
    
    /* 清理资源 */
    mmput(mm);
    kfree(transition);
    
    return 0;
}

unsigned long cvm_get_transition_count(void)
{
    return atomic_long_read(&cvm_transition_count);
}

void cvm_dump_transition_state(struct mm_struct *mm)
{
    pr_info("CVM: Active transitions: %lu\n", cvm_get_transition_count());
    /* 可以添加更详细的调试信息 */
}