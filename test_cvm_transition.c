#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <linux/atomic.h>

#include "confidential_vm_memory.h"
#include "arch_cbit.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CVM Test Module");
MODULE_DESCRIPTION("Test module for CVM state transition mechanism");

/* 测试参数 */
static int test_pages = 100;
module_param(test_pages, int, 0644);
MODULE_PARM_DESC(test_pages, "Number of pages to test");

static int test_threads = 4;
module_param(test_threads, int, 0644);
MODULE_PARM_DESC(test_threads, "Number of test threads");

static bool stress_test = false;
module_param(stress_test, bool, 0644);
MODULE_PARM_DESC(stress_test, "Enable stress testing");

/* 测试统计 */
static atomic_t test_transitions = ATOMIC_INIT(0);
static atomic_t test_faults = ATOMIC_INIT(0);
static atomic_t test_errors = ATOMIC_INIT(0);
static atomic_t test_success = ATOMIC_INIT(0);

/* 测试结构 */
struct cvm_test_context {
    struct task_struct *thread;
    int thread_id;
    void *test_memory;
    unsigned long test_size;
    struct completion completion;
    int result;
};

static struct cvm_test_context *test_contexts;

/**
 * cvm_test_basic_transition - 基本状态切换测试
 */
static int cvm_test_basic_transition(void)
{
    struct page *page;
    enum cvm_page_state initial_state, final_state;
    int ret = 0;
    
    pr_info("CVM Test: Starting basic transition test\n");
    
    /* 分配测试页面 */
    page = alloc_page(GFP_KERNEL);
    if (!page) {
        pr_err("CVM Test: Failed to allocate test page\n");
        return -ENOMEM;
    }
    
    /* 设置初始状态为私有 */
    ret = cvm_set_page_state(page, CVM_PAGE_PRIVATE);
    if (ret) {
        pr_err("CVM Test: Failed to set initial state\n");
        goto cleanup;
    }
    
    /* 验证状态设置 */
    initial_state = cvm_get_page_state(page);
    if (initial_state != CVM_PAGE_PRIVATE) {
        pr_err("CVM Test: Initial state verification failed\n");
        ret = -EINVAL;
        goto cleanup;
    }
    
    /* 切换到共享状态 */
    ret = cvm_set_page_state(page, CVM_PAGE_SHARED);
    if (ret) {
        pr_err("CVM Test: Failed to transition to shared state\n");
        goto cleanup;
    }
    
    /* 验证状态切换 */
    final_state = cvm_get_page_state(page);
    if (final_state != CVM_PAGE_SHARED) {
        pr_err("CVM Test: State transition verification failed\n");
        ret = -EINVAL;
        goto cleanup;
    }
    
    pr_info("CVM Test: Basic transition test passed\n");
    atomic_inc(&test_success);

cleanup:
    if (page)
        __free_page(page);
    
    return ret;
}

/**
 * cvm_test_concurrent_access - 并发访问测试
 */
static int cvm_test_concurrent_access(void *data)
{
    struct cvm_test_context *ctx = (struct cvm_test_context *)data;
    void *test_addr = ctx->test_memory;
    unsigned long size = ctx->test_size;
    struct vm_area_struct *vma;
    int i, ret = 0;
    char test_pattern = 0xAA + ctx->thread_id;
    
    pr_info("CVM Test: Thread %d starting concurrent access test\n", ctx->thread_id);
    
    for (i = 0; i < 100 && !kthread_should_stop(); i++) {
        struct mm_struct *mm = current->mm;
        unsigned long addr = (unsigned long)test_addr;
        
        if (!mm) {
            pr_err("CVM Test: No mm_struct available\n");
            ret = -EINVAL;
            break;
        }
        
        mmap_read_lock(mm);
        vma = find_vma(mm, addr);
        if (!vma || addr < vma->vm_start || addr >= vma->vm_end) {
            mmap_read_unlock(mm);
            continue;
        }
        mmap_read_unlock(mm);
        
        /* 模拟状态切换 */
        ret = cvm_begin_state_transition(vma, addr, addr + PAGE_SIZE,
                                       (i % 2) ? CVM_PAGE_SHARED : CVM_PAGE_PRIVATE);
        if (ret) {
            pr_warn("CVM Test: Thread %d transition failed: %d\n", ctx->thread_id, ret);
            atomic_inc(&test_errors);
            continue;
        }
        
        atomic_inc(&test_transitions);
        
        /* 等待一段随机时间 */
        msleep(get_random_u32() % 10 + 1);
        
        /* 尝试访问内存 */
        if (copy_to_user((void __user *)test_addr, &test_pattern, 1)) {
            atomic_inc(&test_faults);
        }
        
        /* 让出CPU */
        if (need_resched())
            schedule();
    }
    
    ctx->result = ret;
    complete(&ctx->completion);
    
    pr_info("CVM Test: Thread %d completed\n", ctx->thread_id);
    return ret;
}

/**
 * cvm_test_transition_entry - 状态切换条目测试
 */
static int cvm_test_transition_entry(void)
{
    struct cvm_transition_entry *entry;
    struct page *page;
    pte_t test_pte;
    int ret = 0;
    
    pr_info("CVM Test: Starting transition entry test\n");
    
    /* 分配测试页面 */
    page = alloc_page(GFP_KERNEL);
    if (!page) {
        pr_err("CVM Test: Failed to allocate test page\n");
        return -ENOMEM;
    }
    
    /* 创建状态切换条目 */
    entry = cvm_transition_entry_alloc(CVM_TRANSITION_TO_SHARED, page);
    if (!entry) {
        pr_err("CVM Test: Failed to allocate transition entry\n");
        ret = -ENOMEM;
        goto cleanup_page;
    }
    
    /* 测试PTE编码/解码 */
    test_pte = make_cvm_transition_pte(entry);
    if (!is_cvm_transition_pte(test_pte)) {
        pr_err("CVM Test: PTE encoding failed\n");
        ret = -EINVAL;
        goto cleanup_entry;
    }
    
    /* 测试PTE解码 */
    if (cvm_transition_pte_to_entry(test_pte) != entry) {
        pr_err("CVM Test: PTE decoding failed\n");
        ret = -EINVAL;
        goto cleanup_entry;
    }
    
    pr_info("CVM Test: Transition entry test passed\n");
    atomic_inc(&test_success);

cleanup_entry:
    cvm_transition_entry_free(entry);
cleanup_page:
    __free_page(page);
    
    return ret;
}

/**
 * cvm_test_stress - 压力测试
 */
static int cvm_test_stress(void)
{
    int i, ret = 0;
    struct page **pages;
    int num_pages = test_pages;
    
    if (!stress_test) {
        pr_info("CVM Test: Stress test disabled\n");
        return 0;
    }
    
    pr_info("CVM Test: Starting stress test with %d pages\n", num_pages);
    
    pages = kmalloc_array(num_pages, sizeof(struct page *), GFP_KERNEL);
    if (!pages) {
        pr_err("CVM Test: Failed to allocate page array\n");
        return -ENOMEM;
    }
    
    /* 分配所有页面 */
    for (i = 0; i < num_pages; i++) {
        pages[i] = alloc_page(GFP_KERNEL);
        if (!pages[i]) {
            pr_err("CVM Test: Failed to allocate page %d\n", i);
            ret = -ENOMEM;
            num_pages = i;  /* 调整实际分配的页面数 */
            break;
        }
    }
    
    /* 快速状态切换测试 */
    for (i = 0; i < num_pages; i++) {
        enum cvm_page_state state = (i % 2) ? CVM_PAGE_SHARED : CVM_PAGE_PRIVATE;
        ret = cvm_set_page_state(pages[i], state);
        if (ret) {
            pr_err("CVM Test: Failed to set state for page %d\n", i);
            atomic_inc(&test_errors);
        } else {
            atomic_inc(&test_success);
        }
        
        /* 验证状态 */
        if (cvm_get_page_state(pages[i]) != state) {
            pr_err("CVM Test: State verification failed for page %d\n", i);
            atomic_inc(&test_errors);
        }
    }
    
    /* 清理 */
    for (i = 0; i < num_pages; i++) {
        if (pages[i])
            __free_page(pages[i]);
    }
    
    kfree(pages);
    
    pr_info("CVM Test: Stress test completed\n");
    return ret;
}

/**
 * cvm_test_architecture_support - 架构支持测试
 */
static int cvm_test_architecture_support(void)
{
    pr_info("CVM Test: Testing architecture support\n");
    
    if (arch_supports_confidential_computing()) {
        pr_info("CVM Test: Architecture supports confidential computing\n");
        
        /* 测试PTE加密标志操作 */
        pte_t test_pte = __pte(0);
        test_pte = pte_mk_encrypted(test_pte);
        
        if (!pte_encrypted(test_pte)) {
            pr_err("CVM Test: PTE encryption flag test failed\n");
            return -EINVAL;
        }
        
        test_pte = pte_mk_decrypted(test_pte);
        if (pte_encrypted(test_pte)) {
            pr_err("CVM Test: PTE decryption flag test failed\n");
            return -EINVAL;
        }
        
        pr_info("CVM Test: Architecture support test passed\n");
    } else {
        pr_info("CVM Test: Architecture does not support confidential computing (using fallback)\n");
    }
    
    atomic_inc(&test_success);
    return 0;
}

/**
 * cvm_run_all_tests - 运行所有测试
 */
static int cvm_run_all_tests(void)
{
    int ret = 0;
    
    pr_info("CVM Test: Starting comprehensive test suite\n");
    
    /* 基本功能测试 */
    ret = cvm_test_basic_transition();
    if (ret) {
        pr_err("CVM Test: Basic transition test failed: %d\n", ret);
        return ret;
    }
    
    /* 状态切换条目测试 */
    ret = cvm_test_transition_entry();
    if (ret) {
        pr_err("CVM Test: Transition entry test failed: %d\n", ret);
        return ret;
    }
    
    /* 架构支持测试 */
    ret = cvm_test_architecture_support();
    if (ret) {
        pr_err("CVM Test: Architecture support test failed: %d\n", ret);
        return ret;
    }
    
    /* 压力测试 */
    ret = cvm_test_stress();
    if (ret) {
        pr_err("CVM Test: Stress test failed: %d\n", ret);
        return ret;
    }
    
    return 0;
}

/**
 * 模块初始化
 */
static int __init cvm_test_init(void)
{
    int ret = 0;
    
    pr_info("CVM Test: Module loaded\n");
    pr_info("CVM Test: Parameters - pages=%d, threads=%d, stress=%s\n",
            test_pages, test_threads, stress_test ? "yes" : "no");
    
    /* 重置统计 */
    atomic_set(&test_transitions, 0);
    atomic_set(&test_faults, 0);
    atomic_set(&test_errors, 0);
    atomic_set(&test_success, 0);
    
    /* 运行测试 */
    ret = cvm_run_all_tests();
    
    /* 输出统计信息 */
    pr_info("CVM Test: Results - Success: %d, Errors: %d, Transitions: %d, Faults: %d\n",
            atomic_read(&test_success),
            atomic_read(&test_errors),
            atomic_read(&test_transitions),
            atomic_read(&test_faults));
    
    if (ret == 0) {
        pr_info("CVM Test: All tests passed!\n");
    } else {
        pr_err("CVM Test: Some tests failed!\n");
    }
    
    return ret;
}

/**
 * 模块清理
 */
static void __exit cvm_test_exit(void)
{
    pr_info("CVM Test: Module unloaded\n");
}

module_init(cvm_test_init);
module_exit(cvm_test_exit);