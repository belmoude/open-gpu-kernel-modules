#ifndef _LINUX_CONFIDENTIAL_VM_MEMORY_H
#define _LINUX_CONFIDENTIAL_VM_MEMORY_H

#include <linux/mm.h>
#include <linux/mmu_notifier.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <asm/pgtable.h>

/*
 * 机密计算页面状态定义
 * 借鉴页面迁移机制的思路，使用特殊的PTE条目来处理状态切换
 */

/* 页面加密状态 */
enum cvm_page_state {
    CVM_PAGE_PRIVATE = 0,    /* 私有/加密状态 */
    CVM_PAGE_SHARED = 1,     /* 共享/解密状态 */
    CVM_PAGE_TRANSITIONING = 2, /* 状态切换中 */
};

/* 状态切换条目类型 */
enum cvm_transition_type {
    CVM_TRANSITION_TO_PRIVATE = 0,
    CVM_TRANSITION_TO_SHARED = 1,
};

/*
 * CVM状态切换条目结构
 * 类似于migration entry，用于在状态切换期间阻塞并发访问
 */
struct cvm_transition_entry {
    enum cvm_transition_type type;
    struct page *page;
    wait_queue_head_t wait_queue;
    atomic_t ref_count;
    spinlock_t lock;
    unsigned long start_time;
};

/* 页面状态切换上下文 */
struct cvm_state_transition {
    struct mm_struct *mm;
    unsigned long start_addr;
    unsigned long end_addr;
    enum cvm_page_state target_state;
    struct list_head entry_list;
    struct list_head global_list;
    struct mutex transition_mutex;
    atomic_t pending_count;
    struct completion completion;
    struct work_struct completion_work;
    unsigned long start_time;
    bool cancelled;
    bool timeout;
};

/*
 * PTE条目操作宏定义
 * 利用PTE的unused bits来标识CVM状态切换条目
 */
#define _PAGE_CVM_TRANSITION    (_AT(pteval_t, 1) << 9)  /* 使用bit 9标识状态切换 */
#define _PAGE_CVM_TYPE_MASK     (_AT(pteval_t, 1) << 10) /* 使用bit 10标识切换类型 */

static inline int is_cvm_transition_pte(pte_t pte)
{
    return pte_flags(pte) & _PAGE_CVM_TRANSITION;
}

static inline pte_t make_cvm_transition_pte(struct cvm_transition_entry *entry)
{
    pteval_t val = _PAGE_CVM_TRANSITION;
    
    if (entry->type == CVM_TRANSITION_TO_SHARED)
        val |= _PAGE_CVM_TYPE_MASK;
    
    /* 将entry指针编码到PTE中 */
    val |= ((unsigned long)entry) & ~(_PAGE_CVM_TRANSITION | _PAGE_CVM_TYPE_MASK);
    
    return __pte(val);
}

static inline struct cvm_transition_entry *cvm_transition_pte_to_entry(pte_t pte)
{
    unsigned long ptr = pte_val(pte) & ~(_PAGE_CVM_TRANSITION | _PAGE_CVM_TYPE_MASK);
    return (struct cvm_transition_entry *)ptr;
}

/* 函数声明 */
extern int cvm_begin_state_transition(struct vm_area_struct *vma,
                                     unsigned long start, unsigned long end,
                                     enum cvm_page_state target_state);

extern int cvm_complete_state_transition(struct cvm_state_transition *transition);

extern int cvm_schedule_transition(struct cvm_state_transition *transition);

extern vm_fault_t cvm_handle_transition_fault(struct vm_fault *vmf);

extern int cvm_set_page_state(struct page *page, enum cvm_page_state state);

extern enum cvm_page_state cvm_get_page_state(struct page *page);

extern void cvm_transition_entry_free(struct cvm_transition_entry *entry);

extern struct cvm_transition_entry *cvm_transition_entry_alloc(enum cvm_transition_type type,
                                                              struct page *page);

/* 内存屏障和同步操作 */
static inline void cvm_memory_barrier(void)
{
    /* 确保内存操作的顺序性 */
    smp_mb();
}

static inline void cvm_flush_tlb_range(struct vm_area_struct *vma,
                                      unsigned long start, unsigned long end)
{
    flush_tlb_range(vma, start, end);
}

/* 调试和统计接口 */
extern void cvm_dump_transition_state(struct mm_struct *mm);
extern unsigned long cvm_get_transition_count(void);

#endif /* _LINUX_CONFIDENTIAL_VM_MEMORY_H */