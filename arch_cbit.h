#ifndef _ARCH_CBIT_H
#define _ARCH_CBIT_H

#include <linux/types.h>
#include <asm/pgtable.h>

/*
 * 架构相关的C-bit操作函数
 * 支持AMD SEV、Intel TDX等机密计算平台
 */

#ifdef CONFIG_AMD_MEM_ENCRYPT
/* AMD SEV C-bit定义 */
#define _PAGE_ENC_MASK  _PAGE_ENCRYPTED  /* AMD SEV的C-bit */

static inline pte_t pte_mk_encrypted(pte_t pte)
{
    return __pte(pte_val(pte) | _PAGE_ENC_MASK);
}

static inline pte_t pte_mk_decrypted(pte_t pte)
{
    return __pte(pte_val(pte) & ~_PAGE_ENC_MASK);
}

static inline bool pte_encrypted(pte_t pte)
{
    return !!(pte_flags(pte) & _PAGE_ENC_MASK);
}

/* AMD SEV内存加密/解密函数声明 */
extern int sev_set_memory_encrypted(unsigned long pfn, int npages);
extern int sev_set_memory_decrypted(unsigned long pfn, int npages);

#elif defined(CONFIG_INTEL_TDX_GUEST)
/* Intel TDX相关定义 */
#define _PAGE_TDX_SHARED_MASK  (1UL << 51)  /* TDX shared bit */

static inline pte_t pte_mk_encrypted(pte_t pte)
{
    return __pte(pte_val(pte) & ~_PAGE_TDX_SHARED_MASK);
}

static inline pte_t pte_mk_decrypted(pte_t pte)
{
    return __pte(pte_val(pte) | _PAGE_TDX_SHARED_MASK);
}

static inline bool pte_encrypted(pte_t pte)
{
    return !(pte_flags(pte) & _PAGE_TDX_SHARED_MASK);
}

/* Intel TDX内存接受/取消接受函数声明 */
extern int tdx_accept_memory(unsigned long addr, unsigned long size);
extern int tdx_unaccept_memory(unsigned long addr, unsigned long size);

#else
/* 通用实现（无硬件加密支持） */
static inline pte_t pte_mk_encrypted(pte_t pte)
{
    return pte;  /* 无操作 */
}

static inline pte_t pte_mk_decrypted(pte_t pte)
{
    return pte;  /* 无操作 */
}

static inline bool pte_encrypted(pte_t pte)
{
    return false;  /* 总是返回未加密 */
}
#endif

/*
 * 通用的C-bit操作接口
 */
static inline pte_t pte_set_encryption(pte_t pte, bool encrypted)
{
    if (encrypted)
        return pte_mk_encrypted(pte);
    else
        return pte_mk_decrypted(pte);
}

/*
 * 页面级别的加密状态操作
 */
static inline int arch_set_page_encryption(struct page *page, bool encrypted)
{
    unsigned long pfn = page_to_pfn(page);
    int ret = 0;

#ifdef CONFIG_AMD_MEM_ENCRYPT
    if (encrypted) {
        ret = sev_set_memory_encrypted(pfn, 1);
    } else {
        ret = sev_set_memory_decrypted(pfn, 1);
    }
#elif defined(CONFIG_INTEL_TDX_GUEST)
    if (encrypted) {
        ret = tdx_accept_memory(pfn << PAGE_SHIFT, PAGE_SIZE);
    } else {
        ret = tdx_unaccept_memory(pfn << PAGE_SHIFT, PAGE_SIZE);
    }
#endif

    return ret;
}

/*
 * 检查当前架构是否支持机密计算
 */
static inline bool arch_supports_confidential_computing(void)
{
#if defined(CONFIG_AMD_MEM_ENCRYPT) || defined(CONFIG_INTEL_TDX_GUEST)
    return true;
#else
    return false;
#endif
}

/*
 * 缓存一致性操作
 */
static inline void arch_flush_encrypted_page(struct page *page)
{
    /* 确保加密状态变更后的缓存一致性 */
    unsigned long addr = (unsigned long)page_address(page);
    
    if (addr) {
        /* 刷新CPU缓存行 */
        clflush_cache_range((void *)addr, PAGE_SIZE);
        
        /* 内存屏障确保操作完成 */
        mb();
    }
}

/*
 * 调试和统计接口
 */
#ifdef CONFIG_DEBUG_VM
extern void arch_dump_encryption_state(struct mm_struct *mm, 
                                      unsigned long start, unsigned long end);
#else
static inline void arch_dump_encryption_state(struct mm_struct *mm,
                                             unsigned long start, unsigned long end)
{
    /* 空实现 */
}
#endif

#endif /* _ARCH_CBIT_H */