#include "confidential_vm_memory.h"
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/cpu.h>

/* 工作队列和同步机制 */
static struct workqueue_struct *cvm_transition_wq;
static DECLARE_WAIT_QUEUE_HEAD(cvm_transition_wait);

/* 状态切换工作结构 */
struct cvm_transition_work {
    struct work_struct work;
    struct cvm_state_transition *transition;
};

/* 全局状态切换管理 */
static DEFINE_SPINLOCK(cvm_global_transition_lock);
static LIST_HEAD(cvm_active_transitions);
static atomic_t cvm_global_transition_count = ATOMIC_INIT(0);

/**
 * cvm_hardware_set_encryption - 硬件级别的加密状态设置
 * @page: 目标页面
 * @encrypted: 是否加密
 * 
 * 这个函数需要根据具体的硬件平台实现（如AMD SEV, Intel TDX等）
 */
static int cvm_hardware_set_encryption(struct page *page, bool encrypted)
{
    unsigned long pfn = page_to_pfn(page);
    int ret = 0;
    
    /*
     * 这里需要调用具体的硬件接口
     * 
     * 对于AMD SEV：
     * - 调用 sev_set_memory_encrypted/sev_set_memory_decrypted
     * - 使用 CLFLUSH 指令清除缓存
     * - 调用 WBINVD 确保缓存一致性
     * 
     * 对于Intel TDX：
     * - 调用 tdx_accept_memory/tdx_unaccept_memory
     * - 使用相应的 TDX 指令
     */
    
#ifdef CONFIG_AMD_MEM_ENCRYPT
    if (encrypted) {
        ret = sev_set_memory_encrypted(pfn, 1);
    } else {
        ret = sev_set_memory_decrypted(pfn, 1);
    }
#endif

#ifdef CONFIG_INTEL_TDX_GUEST
    if (encrypted) {
        ret = tdx_accept_memory(pfn << PAGE_SHIFT, PAGE_SIZE);
    } else {
        ret = tdx_unaccept_memory(pfn << PAGE_SHIFT, PAGE_SIZE);
    }
#endif

    if (ret) {
        pr_err("CVM: Failed to set encryption state for page %lx: %d\n", pfn, ret);
        return ret;
    }
    
    /* 确保缓存一致性 */
    cvm_memory_barrier();
    flush_cache_page(page_mapping(page), page_index(page));
    
    return 0;
}

/**
 * cvm_transition_worker - 状态切换工作函数
 * @work: 工作结构
 */
static void cvm_transition_worker(struct work_struct *work)
{
    struct cvm_transition_work *tw = container_of(work, struct cvm_transition_work, work);
    struct cvm_state_transition *transition = tw->transition;
    struct mm_struct *mm = transition->mm;
    unsigned long addr;
    int ret;
    bool encrypted = (transition->target_state == CVM_PAGE_PRIVATE);
    
    pr_debug("CVM: Starting transition worker for range 0x%lx-0x%lx\n",
             transition->start_addr, transition->end_addr);
    
    /* 遍历所有需要切换的页面 */
    for (addr = transition->start_addr; 
         addr < transition->end_addr; 
         addr += PAGE_SIZE) {
        
        pgd_t *pgd;
        p4d_t *p4d;
        pud_t *pud;
        pmd_t *pmd;
        pte_t *pte;
        struct page *page;
        struct cvm_transition_entry *entry;
        spinlock_t *ptl;
        
        /* 检查是否需要停止 */
        if (kthread_should_stop() || freezing(current)) {
            pr_info("CVM: Transition worker interrupted\n");
            break;
        }
        
        /* 可能需要让出CPU */
        if (need_resched())
            schedule();
        
        mmap_read_lock(mm);
        
        pgd = pgd_offset(mm, addr);
        if (pgd_none(*pgd) || pgd_bad(*pgd)) {
            mmap_read_unlock(mm);
            continue;
        }
        
        p4d = p4d_offset(pgd, addr);
        if (p4d_none(*p4d) || p4d_bad(*p4d)) {
            mmap_read_unlock(mm);
            continue;
        }
        
        pud = pud_offset(p4d, addr);
        if (pud_none(*pud) || pud_bad(*pud)) {
            mmap_read_unlock(mm);
            continue;
        }
        
        pmd = pmd_offset(pud, addr);
        if (pmd_none(*pmd) || pmd_bad(*pmd)) {
            mmap_read_unlock(mm);
            continue;
        }
        
        pte = pte_offset_map_lock(mm, pmd, addr, &ptl);
        if (!pte) {
            mmap_read_unlock(mm);
            continue;
        }
        
        /* 检查是否是状态切换条目 */
        if (!is_cvm_transition_pte(*pte)) {
            pte_unmap_unlock(pte, ptl);
            mmap_read_unlock(mm);
            continue;
        }
        
        entry = cvm_transition_pte_to_entry(*pte);
        page = entry->page;
        
        if (!page || !PageLocked(page)) {
            pte_unmap_unlock(pte, ptl);
            mmap_read_unlock(mm);
            continue;
        }
        
        pte_unmap_unlock(pte, ptl);
        mmap_read_unlock(mm);
        
        /* 执行硬件级别的状态切换 */
        ret = cvm_hardware_set_encryption(page, encrypted);
        if (ret) {
            pr_err("CVM: Hardware encryption failed for page at 0x%lx\n", addr);
            /* 继续处理其他页面 */
            continue;
        }
        
        /* 更新页面状态 */
        cvm_set_page_state(page, transition->target_state);
        
        pr_debug("CVM: Completed encryption state change for page at 0x%lx\n", addr);
    }
    
    /* 完成状态切换 */
    schedule_work(&transition->completion_work);
    
    kfree(tw);
}

/**
 * cvm_transition_completion_worker - 状态切换完成工作函数
 * @work: 工作结构
 */
static void cvm_transition_completion_worker(struct work_struct *work)
{
    struct cvm_state_transition *transition = 
        container_of(work, struct cvm_state_transition, completion_work);
    
    pr_debug("CVM: Completing state transition\n");
    
    /* 调用完成函数 */
    cvm_complete_state_transition(transition);
    
    /* 从活跃列表中移除 */
    spin_lock(&cvm_global_transition_lock);
    list_del(&transition->global_list);
    atomic_dec(&cvm_global_transition_count);
    spin_unlock(&cvm_global_transition_lock);
    
    /* 唤醒等待的进程 */
    wake_up_all(&cvm_transition_wait);
}

/**
 * cvm_schedule_transition - 调度状态切换任务
 * @transition: 状态切换上下文
 */
int cvm_schedule_transition(struct cvm_state_transition *transition)
{
    struct cvm_transition_work *work;
    
    work = kmalloc(sizeof(*work), GFP_KERNEL);
    if (!work)
        return -ENOMEM;
    
    INIT_WORK(&work->work, cvm_transition_worker);
    INIT_WORK(&transition->completion_work, cvm_transition_completion_worker);
    work->transition = transition;
    
    /* 添加到全局活跃列表 */
    spin_lock(&cvm_global_transition_lock);
    list_add(&transition->global_list, &cvm_active_transitions);
    atomic_inc(&cvm_global_transition_count);
    spin_unlock(&cvm_global_transition_lock);
    
    /* 提交到工作队列 */
    queue_work(cvm_transition_wq, &work->work);
    
    return 0;
}

/**
 * cvm_wait_all_transitions - 等待所有状态切换完成
 */
void cvm_wait_all_transitions(void)
{
    wait_event(cvm_transition_wait, 
               atomic_read(&cvm_global_transition_count) == 0);
}

/**
 * cvm_cancel_transitions - 取消指定内存映射的所有状态切换
 * @mm: 内存映射结构
 */
int cvm_cancel_transitions(struct mm_struct *mm)
{
    struct cvm_state_transition *transition, *tmp;
    int cancelled = 0;
    
    spin_lock(&cvm_global_transition_lock);
    
    list_for_each_entry_safe(transition, tmp, &cvm_active_transitions, global_list) {
        if (transition->mm == mm) {
            /* 标记为取消 */
            transition->cancelled = true;
            cancelled++;
        }
    }
    
    spin_unlock(&cvm_global_transition_lock);
    
    if (cancelled > 0) {
        /* 刷新工作队列确保所有任务处理完成 */
        flush_workqueue(cvm_transition_wq);
    }
    
    return cancelled;
}

/**
 * cvm_transition_timeout_worker - 超时处理工作函数
 * @work: 延迟工作结构
 */
static void cvm_transition_timeout_worker(struct delayed_work *work)
{
    struct cvm_state_transition *transition, *tmp;
    unsigned long timeout_jiffies = msecs_to_jiffies(30000); /* 30秒超时 */
    
    spin_lock(&cvm_global_transition_lock);
    
    list_for_each_entry_safe(transition, tmp, &cvm_active_transitions, global_list) {
        if (time_after(jiffies, transition->start_time + timeout_jiffies)) {
            pr_warn("CVM: Transition timeout for range 0x%lx-0x%lx\n",
                   transition->start_addr, transition->end_addr);
            
            /* 强制完成超时的切换 */
            transition->timeout = true;
            schedule_work(&transition->completion_work);
        }
    }
    
    spin_unlock(&cvm_global_transition_lock);
    
    /* 重新调度超时检查 */
    schedule_delayed_work(&cvm_timeout_work, timeout_jiffies);
}

static DECLARE_DELAYED_WORK(cvm_timeout_work, cvm_transition_timeout_worker);

/**
 * cvm_memory_hotplug_notifier - 内存热插拔通知处理
 * @nb: 通知块
 * @action: 动作
 * @data: 数据
 */
static int cvm_memory_hotplug_notifier(struct notifier_block *nb,
                                      unsigned long action, void *data)
{
    struct memory_notify *mn = data;
    
    switch (action) {
    case MEM_GOING_OFFLINE:
        /* 内存即将下线，等待所有相关的状态切换完成 */
        pr_info("CVM: Memory going offline, waiting for transitions\n");
        cvm_wait_all_transitions();
        break;
        
    case MEM_CANCEL_OFFLINE:
        /* 内存下线取消 */
        pr_info("CVM: Memory offline cancelled\n");
        break;
        
    case MEM_ONLINE:
        /* 内存上线，可能需要初始化CVM状态 */
        pr_info("CVM: Memory online\n");
        break;
    }
    
    return NOTIFY_OK;
}

static struct notifier_block cvm_memory_nb = {
    .notifier_call = cvm_memory_hotplug_notifier,
    .priority = 0,
};

/**
 * cvm_workqueue_init - 初始化CVM工作队列系统
 */
int __init cvm_workqueue_init(void)
{
    /* 创建专用的工作队列 */
    cvm_transition_wq = alloc_workqueue("cvm_transition", 
                                       WQ_MEM_RECLAIM | WQ_HIGHPRI, 0);
    if (!cvm_transition_wq) {
        pr_err("CVM: Failed to create transition workqueue\n");
        return -ENOMEM;
    }
    
    /* 注册内存热插拔通知 */
    register_memory_notifier(&cvm_memory_nb);
    
    /* 启动超时检查 */
    schedule_delayed_work(&cvm_timeout_work, msecs_to_jiffies(30000));
    
    pr_info("CVM: Workqueue system initialized\n");
    return 0;
}

/**
 * cvm_workqueue_exit - 清理CVM工作队列系统
 */
void __exit cvm_workqueue_exit(void)
{
    /* 取消超时检查 */
    cancel_delayed_work_sync(&cvm_timeout_work);
    
    /* 等待所有状态切换完成 */
    cvm_wait_all_transitions();
    
    /* 销毁工作队列 */
    if (cvm_transition_wq) {
        destroy_workqueue(cvm_transition_wq);
        cvm_transition_wq = NULL;
    }
    
    /* 注销内存热插拔通知 */
    unregister_memory_notifier(&cvm_memory_nb);
    
    pr_info("CVM: Workqueue system cleaned up\n");
}

module_init(cvm_workqueue_init);
module_exit(cvm_workqueue_exit);