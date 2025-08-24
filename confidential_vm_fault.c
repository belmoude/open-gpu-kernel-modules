#include "confidential_vm_memory.h"
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <linux/rmap.h>
#include <linux/swap.h>
#include <linux/ptrace.h>
#include <linux/security.h>
#include <linux/memcontrol.h>
#include <linux/mmu_notifier.h>
#include <linux/hugetlb.h>
#include <linux/userfaultfd_k.h>
#include <linux/dax.h>
#include <asm/mmu_context.h>
#include <asm/tlb.h>

/**
 * cvm_handle_transition_fault - 处理状态切换条目引起的缺页异常
 * @vmf: 虚拟内存故障结构
 * 
 * 类似于handle_pte_fault中对migration entry的处理
 * 当进程访问处于状态切换中的页面时，会进入这个处理函数
 */
vm_fault_t cvm_handle_transition_fault(struct vm_fault *vmf)
{
    struct vm_area_struct *vma = vmf->vma;
    unsigned long address = vmf->address;
    pte_t *pte = vmf->pte;
    pte_t entry = vmf->orig_pte;
    struct cvm_transition_entry *transition_entry;
    struct page *page;
    vm_fault_t ret = 0;
    int locked = 0;
    
    /* 验证这确实是一个CVM状态切换条目 */
    if (!is_cvm_transition_pte(entry))
        return VM_FAULT_SIGBUS;
    
    /* 提取状态切换条目 */
    transition_entry = cvm_transition_pte_to_entry(entry);
    if (!transition_entry)
        return VM_FAULT_SIGBUS;
    
    page = transition_entry->page;
    if (!page)
        return VM_FAULT_SIGBUS;
    
    /* 增加条目的引用计数 */
    atomic_inc(&transition_entry->ref_count);
    
    pte_unmap_unlock(vmf->pte, vmf->ptl);
    vmf->pte = NULL;
    
    /* 等待状态切换完成 */
    if (vmf->flags & FAULT_FLAG_KILLABLE) {
        /* 可中断等待 */
        ret = wait_event_killable(transition_entry->wait_queue,
                                !is_cvm_transition_pte(*pte_offset_map(vmf->pmd, address)));
        if (ret) {
            ret = VM_FAULT_RETRY;
            goto out;
        }
    } else {
        /* 不可中断等待 */
        wait_event(transition_entry->wait_queue,
                  !is_cvm_transition_pte(*pte_offset_map(vmf->pmd, address)));
    }
    
    /* 重新映射页表条目 */
    vmf->pte = pte_offset_map_lock(vma->vm_mm, vmf->pmd, address, &vmf->ptl);
    if (!vmf->pte) {
        ret = VM_FAULT_RETRY;
        goto out;
    }
    
    /* 检查状态切换是否已经完成 */
    if (!is_cvm_transition_pte(*vmf->pte)) {
        /* 状态切换已完成，返回VM_FAULT_NOPAGE让内核重试 */
        ret = VM_FAULT_NOPAGE;
        goto unlock;
    }
    
    /* 如果状态切换仍在进行中（异常情况），继续等待 */
    pte_unmap_unlock(vmf->pte, vmf->ptl);
    vmf->pte = NULL;
    
    /* 避免忙等待 */
    if (PageLocked(page)) {
        if (vmf->flags & FAULT_FLAG_KILLABLE) {
            ret = __lock_page_killable(page);
            if (ret) {
                ret = VM_FAULT_RETRY;
                goto out;
            }
        } else {
            __lock_page(page);
        }
        locked = 1;
    }
    
    /* 再次检查状态 */
    vmf->pte = pte_offset_map_lock(vma->vm_mm, vmf->pmd, address, &vmf->ptl);
    if (!vmf->pte) {
        ret = VM_FAULT_RETRY;
        goto out_unlock;
    }
    
    if (!is_cvm_transition_pte(*vmf->pte)) {
        ret = VM_FAULT_NOPAGE;
        goto unlock;
    }
    
    /*
     * 如果到这里状态切换还没完成，可能是系统异常
     * 记录错误并返回故障
     */
    pr_err("CVM: Transition timeout for page at 0x%lx, entry type %d\n",
           address, transition_entry->type);
    ret = VM_FAULT_SIGBUS;

unlock:
    pte_unmap_unlock(vmf->pte, vmf->ptl);
    vmf->pte = NULL;

out_unlock:
    if (locked)
        unlock_page(page);

out:
    /* 减少条目的引用计数 */
    cvm_transition_entry_free(transition_entry);
    
    return ret;
}

/**
 * cvm_is_transition_fault - 检查是否为CVM状态切换故障
 * @vmf: 虚拟内存故障结构
 */
static inline bool cvm_is_transition_fault(struct vm_fault *vmf)
{
    return is_cvm_transition_pte(vmf->orig_pte);
}

/**
 * cvm_do_swap_page - 处理交换页面与CVM状态切换的冲突
 * @vmf: 虚拟内存故障结构
 * 
 * 当页面在交换中遇到状态切换时的处理
 */
static vm_fault_t cvm_do_swap_page(struct vm_fault *vmf)
{
    struct vm_area_struct *vma = vmf->vma;
    struct page *page = NULL, *swapcache;
    struct mem_cgroup *memcg;
    swp_entry_t entry;
    pte_t pte;
    int locked;
    vm_fault_t ret = 0;
    void *shadow = NULL;
    
    /* 如果是CVM状态切换条目，直接处理 */
    if (cvm_is_transition_fault(vmf))
        return cvm_handle_transition_fault(vmf);
    
    /* 正常的交换页面处理逻辑 */
    /* ... 这里可以添加与现有swap处理的集成 ... */
    
    return ret;
}

/**
 * cvm_pte_alloc_one_map - 为CVM分配页表条目
 * @vma: 虚拟内存区域
 * @address: 虚拟地址
 * 
 * 确保在CVM环境下正确分配和初始化页表条目
 */
static int cvm_pte_alloc_one_map(struct vm_fault *vmf)
{
    struct vm_area_struct *vma = vmf->vma;
    
    if (pmd_none(*vmf->pmd)) {
        if (unlikely(anon_vma_prepare(vma)))
            return VM_FAULT_OOM;
        if (unlikely(__pte_alloc(vma->vm_mm, vmf->pmd)))
            return VM_FAULT_OOM;
    }
    
    /* 
     * 确保页表条目分配后立即设置适当的CVM属性
     * 这里可以根据VMA的属性来设置默认的加密状态
     */
    
    return 0;
}

/**
 * cvm_wp_page_copy - 处理写时复制与状态切换的冲突
 * @vmf: 虚拟内存故障结构
 * @old_page: 原始页面
 */
static vm_fault_t cvm_wp_page_copy(struct vm_fault *vmf, struct page *old_page)
{
    struct vm_area_struct *vma = vmf->vma;
    struct mm_struct *mm = vma->vm_mm;
    struct page *new_page;
    pte_t entry;
    enum cvm_page_state old_state, new_state;
    
    /* 如果原页面处于状态切换中，等待完成 */
    if (cvm_is_transition_fault(vmf))
        return cvm_handle_transition_fault(vmf);
    
    /* 获取原页面的加密状态 */
    old_state = cvm_get_page_state(old_page);
    
    /* 分配新页面 */
    new_page = alloc_page_vma(GFP_HIGHUSER_MOVABLE, vma, vmf->address);
    if (!new_page)
        return VM_FAULT_OOM;
    
    /* 复制页面内容 */
    copy_user_highpage(new_page, old_page, vmf->address, vma);
    
    /* 继承加密状态 */
    cvm_set_page_state(new_page, old_state);
    
    /* 设置新的PTE */
    entry = mk_pte(new_page, vma->vm_page_prot);
    entry = pte_sw_mkyoung(entry);
    entry = maybe_mkwrite(pte_mkdirty(entry), vma);
    
    /* 
     * 根据页面的加密状态设置PTE的C-bit
     * 这里需要根据具体的硬件架构实现
     */
    if (old_state == CVM_PAGE_PRIVATE) {
        /* 设置C-bit表示加密页面 */
        entry = pte_mk_encrypted(entry);  /* 这个函数需要架构相关实现 */
    }
    
    /* 原子性更新PTE */
    ptep_clear_flush_notify(vma, vmf->address, vmf->pte);
    page_add_new_anon_rmap(new_page, vma, vmf->address, false);
    mem_cgroup_count_vm_event(mm, PGFAULT);
    lru_cache_add_active_or_unevictable(new_page, vma);
    set_pte_at_notify(mm, vmf->address, vmf->pte, entry);
    update_mmu_cache(vma, vmf->address, vmf->pte);
    
    /* 释放原页面 */
    page_remove_rmap(old_page, false);
    put_page(old_page);
    
    return VM_FAULT_WRITE;
}

/**
 * cvm_handle_pte_fault - CVM扩展的PTE故障处理
 * @vmf: 虚拟内存故障结构
 */
vm_fault_t cvm_handle_pte_fault(struct vm_fault *vmf)
{
    pte_t entry = vmf->orig_pte;
    
    /* 检查是否为CVM状态切换条目 */
    if (unlikely(cvm_is_transition_fault(vmf))) {
        return cvm_handle_transition_fault(vmf);
    }
    
    /* 其他正常的故障处理 */
    if (unlikely(pte_none(entry))) {
        /* 处理全新页面分配 */
        if (vma_is_anonymous(vmf->vma))
            return do_anonymous_page(vmf);
        else
            return do_fault(vmf);
    }
    
    if (!pte_present(entry))
        return do_swap_page(vmf);
    
    if (vmf->flags & FAULT_FLAG_WRITE) {
        if (!pte_write(entry))
            return do_wp_page(vmf);
        entry = pte_mkdirty(entry);
    }
    
    entry = pte_mkyoung(entry);
    if (ptep_set_access_flags(vmf->vma, vmf->address, vmf->pte, entry,
                             vmf->flags & FAULT_FLAG_WRITE)) {
        update_mmu_cache(vmf->vma, vmf->address, vmf->pte);
    } else {
        /* 
         * 即使没有TLB更新，也要确保CVM状态一致性
         * 检查页面的加密状态是否与PTE的C-bit一致
         */
        struct page *page = pte_page(entry);
        enum cvm_page_state state = cvm_get_page_state(page);
        
        /* 这里可以添加状态一致性检查和修复逻辑 */
        if (state == CVM_PAGE_TRANSITIONING) {
            /* 如果页面仍在切换中，这不应该发生 */
            pr_warn("CVM: Accessing transitioning page at 0x%lx\n", vmf->address);
        }
    }
    
    return 0;
}

/**
 * cvm_memory_failure_recovery - CVM内存故障恢复
 * @page: 故障页面
 * @flags: 故障标志
 */
int cvm_memory_failure_recovery(struct page *page, unsigned long flags)
{
    enum cvm_page_state state;
    
    if (!page)
        return -EINVAL;
    
    state = cvm_get_page_state(page);
    
    /* 如果页面正在状态切换中，等待完成后再处理 */
    if (state == CVM_PAGE_TRANSITIONING) {
        pr_info("CVM: Memory failure during state transition, waiting...\n");
        /* 这里可以添加等待逻辑或强制完成切换 */
        return -EAGAIN;
    }
    
    /* 正常的内存故障恢复流程 */
    return 0;
}