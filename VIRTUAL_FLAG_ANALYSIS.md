# NVOS32_ALLOC_FLAGS_VIRTUAL 标志的详细分析

## 核心问题

**只要有 `NVOS32_ALLOC_FLAGS_VIRTUAL` 就够了吗？还是必须配合 `NVOS32_ALLOC_FLAGS_LAZY`？**

## 答案：只要有 NVOS32_ALLOC_FLAGS_VIRTUAL 就足够！

### 关键代码证据

#### 1. 类型选择逻辑

**文件**: `src/nvidia/interface/deprecated/rmapi_deprecated_vidheapctrl.c`
**行号**: 138-145

```c
if (pUserParams->flags & NVOS32_ALLOC_FLAGS_VIRTUAL)
    externalClassId = NV50_MEMORY_VIRTUAL;  // 使用虚拟内存类
else if (FLD_TEST_DRF(OS32, _ATTR2, _USE_EGM, _TRUE, pUserParams->attr2))
    externalClassId = NV_MEMORY_EXTENDED_USER;
else if (FLD_TEST_DRF(OS32, _ATTR, _LOCATION, _VIDMEM, pUserParams->attr))
    externalClassId = NV01_MEMORY_LOCAL_USER;  // 物理显存类
else
    externalClassId = NV01_MEMORY_SYSTEM;
```

**关键点**:
- ✅ 只检查 `NVOS32_ALLOC_FLAGS_VIRTUAL` 标志
- ✅ 不需要 LAZY 标志
- ✅ 只要有 VIRTUAL，就使用 `NV50_MEMORY_VIRTUAL` 类

#### 2. 虚拟内存分配流程

**文件**: `src/nvidia/src/kernel/mem_mgr/virtual_mem.c`
**函数**: `virtmemAllocResources`
**行号**: 858-860

```c
status = vaspaceAlloc(pVAS, pFbAllocInfo->size, align,
                      pVidHeapAlloc->rangeLo, pVidHeapAlloc->rangeHi,
                      pageSizeLockMask, flags, &pFbAllocInfo->offset);
```

**关键点**:
- ✅ `vaspaceAlloc` 从 **VA space 的 eheap** 中分配虚拟地址
- ✅ **不涉及**物理显存的 heap (`pHeap->free`)
- ✅ 因此 nvidia-smi 看到的显存使用量**不会因为数据分配而增加**

#### 3. 物理显存 heap vs 虚拟地址 heap

这是两个**完全独立**的 heap：

| 特性 | 物理显存 Heap | 虚拟地址 Heap |
|-----|-------------|--------------|
| 数据结构 | `Heap` (heap.c) | `OBJEHEAP` (eheap_old.c) |
| 分配函数 | `heapAlloc` | `eheapAlloc` |
| 查询函数 | `heapGetFree` | `eheapGetFree` |
| nvidia-smi 查询 | ✅ 是 | ❌ 否 |
| 用途 | 物理显存页 | 虚拟地址范围 |

**关键**:
- `NV50_MEMORY_VIRTUAL` 只操作 **VA space heap**
- **不会**修改物理显存 heap 的 `pHeap->free`
- nvidia-smi 查询的是**物理显存 heap**

#### 4. LAZY 标志的真正作用

**文件**: `src/nvidia/src/kernel/mem_mgr/virtual_mem.c`
**行号**: 375-415

```c
//
// Reserve memory for page tables in case of non lazy page table
// allocations.
//
if (memmgrIsPmaInitialized(pMemoryManager) &&
    !(pAllocData->flags & NVOS32_ALLOC_FLAGS_LAZY) &&
    !(pAllocData->flags & NVOS32_ALLOC_FLAGS_EXTERNALLY_MANAGED))
{
    // 为页表预留内存
    status = vaspaceReserveMempool(pVAS, pGpu, pDevice,
                                   size, pageSizeLockMask,
                                   VASPACE_RESERVE_FLAGS_NONE);
}
```

**LAZY 标志的作用**:
- ✅ 控制**页表（page table）内存**是否预先分配
- ❌ **不影响**数据的物理显存分配
- ⚠️ 页表内存占用远小于数据大小（通常 < 1%）

#### 5. 页表内存大小估算

**文件**: `src/nvidia/src/kernel/mem_mgr/gpu_vaspace.c`
**行号**: 5329-5330

```c
poolSize = kgmmuGetSizeOfPageDirs(pGpu, pKernelGmmu, pFmt, 0, size - 1, pageSizeLockMask) +
           kgmmuGetSizeOfPageTables(pGpu, pKernelGmmu, pFmt, 0, size - 1, pageSizeLockMask);
```

**示例计算** (假设 4KB 页):
- 1GB 数据 = 1024 MB
- 页表内存 ≈ 2 MB (1GB / 4KB * 8 bytes per PTE)
- 占比 ≈ **0.2%**

## 实际测试验证

### 测试场景 1: 只有 VIRTUAL 标志

```c
NV_MEMORY_ALLOCATION_PARAMS params = {
    .flags = NVOS32_ALLOC_FLAGS_VIRTUAL,  // 只有 VIRTUAL
    .attr = NVOS32_ATTR_LOCATION_VIDMEM,
    .size = 1024 * 1024 * 1024,  // 1GB
};
```

**预期结果**:
- ✅ 分配成功
- ✅ 只分配虚拟地址范围
- ✅ nvidia-smi 显示的数据显存使用量 **不变**
- ⚠️ 可能有 ~2MB 页表内存增加（通常看不出来）

### 测试场景 2: VIRTUAL + LAZY

```c
NV_MEMORY_ALLOCATION_PARAMS params = {
    .flags = NVOS32_ALLOC_FLAGS_VIRTUAL | 
             NVOS32_ALLOC_FLAGS_LAZY,
    .attr = NVOS32_ATTR_LOCATION_VIDMEM,
    .size = 1024 * 1024 * 1024,  // 1GB
};
```

**预期结果**:
- ✅ 分配成功
- ✅ 只分配虚拟地址范围
- ✅ nvidia-smi 显示的显存使用量 **完全不变**
- ✅ 连页表内存也不预分配

### 对比：物理内存分配

```c
NVOS32_PARAMETERS params = {
    .function = NVOS32_FUNCTION_ALLOC_SIZE,
    .data.AllocSize.flags = 0,  // 无 VIRTUAL 标志
    .data.AllocSize.attr = NVOS32_ATTR_LOCATION_VIDMEM,
    .data.AllocSize.size = 1024 * 1024 * 1024,
};
```

**结果**:
- ❌ 使用 `NV01_MEMORY_LOCAL_USER` 类
- ❌ 调用 `heapAlloc` 分配物理显存
- ❌ `pHeap->free` **立即减少 1GB**
- ❌ nvidia-smi **立即显示增加 1GB**

## 代码调用链对比

### 有 VIRTUAL 标志的调用链

```
NV_ESC_RM_VID_HEAP_CONTROL
  └─> _rmVidHeapControlAllocCommon
      └─> RmAlloc(NV50_MEMORY_VIRTUAL)  ← 注意这里
          └─> virtmemConstruct_IMPL
              ├─> vaspaceReserveMempool (if !LAZY)  ← 页表内存
              └─> virtmemAllocResources
                  └─> vaspaceAlloc
                      └─> eheapAlloc  ← VA space heap
                          ✅ 不涉及物理显存 heap
```

### 无 VIRTUAL 标志的调用链

```
NV_ESC_RM_VID_HEAP_CONTROL
  └─> _rmVidHeapControlAllocCommon
      └─> RmAlloc(NV01_MEMORY_LOCAL_USER)  ← 注意这里
          └─> vidmemConstruct_IMPL
              └─> vidmemAllocResources
                  └─> heapAlloc  ← 物理显存 heap
                      ├─> _heapProcessFreeBlock
                      └─> _heapAdjustFree
                          └─> pHeap->free -= size  ❌ 减少物理显存
```

## nvidia-smi 数据来源

**文件**: `src/nvidia/src/kernel/gpu/mem_sys/kern_mem_sys_ctrl.c`
**行号**: 575-589

```c
case NV2080_CTRL_FB_INFO_INDEX_HEAP_FREE:
{
    if (bIsPmaEnabled)
    {
        pmaGetFreeMemory(pHeap->pPmaObject, &bytesFree);  // PMA 管理
    }
    else
    {
        heapGetFree(pHeap, &size);  // 直接查询物理 heap
    }
    
    data = NvU64_LO32(size >> 10);  // 返回给 nvidia-smi
}
```

**关键点**:
- nvidia-smi 查询的是**物理显存 heap** (`pHeap->free`)
- 虚拟内存分配**不修改**这个值
- 因此 nvidia-smi 看不到虚拟内存分配

## 结论

### ✅ 只要有 NVOS32_ALLOC_FLAGS_VIRTUAL 就够了

1. **类型选择**: 只检查 VIRTUAL 标志，不检查 LAZY
2. **分配位置**: VA space heap，不涉及物理显存 heap
3. **nvidia-smi**: 显示的是物理显存，虚拟内存不影响

### 📌 LAZY 标志的补充作用

1. **页表内存**: 控制是否预先分配页表内存
2. **影响很小**: 页表内存 << 数据大小（通常 < 1%）
3. **可选优化**: 
   - 不带 LAZY: 预分配页表，map 时更快
   - 带 LAZY: 不预分配，节省少量内存

### 🎯 实际使用建议

#### 场景 1: 只需要虚拟地址，不关心性能
```c
flags = NVOS32_ALLOC_FLAGS_VIRTUAL;
```
- ✅ 最简单
- ⚠️ nvidia-smi 可能显示很小的增加（页表）

#### 场景 2: 完全不分配任何内存
```c
flags = NVOS32_ALLOC_FLAGS_VIRTUAL | 
        NVOS32_ALLOC_FLAGS_LAZY;
```
- ✅ 连页表也不分配
- ✅ nvidia-smi 完全不变

#### 场景 3: 外部管理
```c
flags = NVOS32_ALLOC_FLAGS_VIRTUAL | 
        NVOS32_ALLOC_FLAGS_EXTERNALLY_MANAGED;
```
- ✅ 应用层自己管理
- ✅ nvidia-smi 不变

## 修正后的测试程序

原测试程序使用了 `VIRTUAL | LAZY`，其实**只用 VIRTUAL 也足够**：

```c
// 方案 A: 只有 VIRTUAL（推荐）
virtualAllocParams.flags = NVOS32_ALLOC_FLAGS_VIRTUAL;

// 方案 B: VIRTUAL + LAZY（更彻底）
virtualAllocParams.flags = NVOS32_ALLOC_FLAGS_VIRTUAL | 
                           NVOS32_ALLOC_FLAGS_LAZY;
```

两者都不会在 nvidia-smi 中显示数据显存的增加，差别只是页表内存（通常可忽略）。

## 参考代码位置汇总

| 功能 | 文件 | 行号 |
|-----|------|-----|
| 类型选择（检查 VIRTUAL） | rmapi_deprecated_vidheapctrl.c | 138-145 |
| 虚拟内存分配 | virtual_mem.c | 720-945 |
| VA space alloc | virtual_mem.c | 858-860 |
| 页表预留（LAZY 控制） | virtual_mem.c | 382-415 |
| 物理显存分配 | video_mem.c | 533-1630 |
| heap 空闲查询 | heap.c | 2499-2508 |
| nvidia-smi 查询 | kern_mem_sys_ctrl.c | 521-591 |
