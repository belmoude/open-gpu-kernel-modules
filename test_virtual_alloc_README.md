# 虚拟内存延迟分配测试说明

## 核心原理

在 NVIDIA 驱动中，使用 `NV_ESC_RM_VID_HEAP_CONTROL` + `NVOS32_FUNCTION_ALLOC_SIZE` 时存在两种分配模式：

### 1. 立即分配物理显存（默认行为）
- **条件**: 不带 `NVOS32_ALLOC_FLAGS_VIRTUAL/LAZY/EXTERNALLY_MANAGED` 标志
- **行为**: 立即从 heap 中分配物理显存，`pHeap->free` 立即减少
- **nvidia-smi**: 立即显示显存使用量增加

### 2. 延迟分配（虚拟内存模式）
- **条件**: 带 `NVOS32_ALLOC_FLAGS_VIRTUAL` + (`NVOS32_ALLOC_FLAGS_LAZY` 或 `NVOS32_ALLOC_FLAGS_EXTERNALLY_MANAGED`)
- **行为**: 
  - 只在虚拟地址空间中保留（reserve）地址范围
  - 不从物理显存 heap 中分配，`pHeap->free` 不变
  - 物理显存在后续 map 操作（如 `NV_ESC_RM_MAP_MEMORY_DMA`）时才真正分配
- **nvidia-smi**: 分配后不显示显存增加，map 后才显示

## 关键代码位置

### 延迟分配的判断逻辑
`src/nvidia/src/kernel/mem_mgr/virtual_mem.c:382-415`

```c
if (memmgrIsPmaInitialized(pMemoryManager) &&
    !(pAllocData->flags & NVOS32_ALLOC_FLAGS_LAZY) &&
    !(pAllocData->flags & NVOS32_ALLOC_FLAGS_EXTERNALLY_MANAGED))
{
    // 只有不带 LAZY 和 EXTERNALLY_MANAGED 时才预分配
    // 带这些标志时，跳过物理内存分配
}
```

### 物理显存统计
`src/nvidia/src/kernel/gpu/mem_mgr/heap.c:2499-2508`

```c
NV_STATUS heapGetFree_IMPL(Heap *pHeap, NvU64 *free)
{
    *free = pHeap->free;  // nvidia-smi 查询的就是这个值
    HEAP_VALIDATE(pHeap);
    return (NV_OK);
}
```

### nvidia-smi 查询接口
`src/nvidia/src/kernel/gpu/mem_sys/kern_mem_sys_ctrl.c:521-591`

```c
case NV2080_CTRL_FB_INFO_INDEX_HEAP_FREE:
{
    if (bIsPmaEnabled)
        pmaGetFreeMemory(pHeap->pPmaObject, &bytesFree);
    else
        heapGetFree(pHeap, &size);  // 返回 pHeap->free
}
```

## 编译和运行

### 编译
```bash
cd /workspace
gcc -o test_virtual_alloc test_virtual_alloc.c \
    -I./src/common/sdk/nvidia/inc \
    -I./src/nvidia/arch/nvalloc/unix/include
```

### 运行步骤

1. **准备两个终端**
   - 终端1：运行测试程序
   - 终端2：监控 nvidia-smi

2. **终端2 - 持续监控显存**
   ```bash
   watch -n 1 nvidia-smi
   ```

3. **终端1 - 运行测试**
   ```bash
   sudo ./test_virtual_alloc
   ```

4. **观察现象**
   - 程序会在关键步骤暂停，提示按回车继续
   - 每次暂停时查看终端2的 nvidia-smi 输出
   - 对比虚拟内存分配和物理内存分配的显存使用差异

## 预期结果

### 场景1：虚拟内存分配（VIRTUAL + LAZY）
```
成功分配虚拟内存对象: 0x87654321 (大小: 1024 MB)
```
**nvidia-smi 显示**: 显存使用量**不变**

### 场景2：物理显存分配（无 VIRTUAL/LAZY）
```
成功分配物理显存: 0x12345678 (大小: 256 MB)
```
**nvidia-smi 显示**: 显存使用量**增加约 256MB**

## 标志组合说明

### NVOS32_ALLOC_FLAGS_VIRTUAL_ONLY
定义在 `src/common/sdk/nvidia/inc/nvos.h:1491-1497`

```c
#define NVOS32_ALLOC_FLAGS_VIRTUAL_ONLY ( \
    NVOS32_ALLOC_FLAGS_VIRTUAL          | \
    NVOS32_ALLOC_FLAGS_LAZY             | \
    NVOS32_ALLOC_FLAGS_EXTERNALLY_MANAGED | \
    NVOS32_ALLOC_FLAGS_SPARSE           | \
    NVOS32_ALLOC_FLAGS_MAXIMIZE_ADDRESS_SPACE | \
    NVOS32_ALLOC_FLAGS_PREFER_PTES_IN_SYSMEMORY )
```

这个组合就是专门用于"只保留虚拟地址，不分配物理内存"的场景。

## 典型应用场景

1. **UVM (Unified Virtual Memory)**: 延迟分配，按需调入
2. **大型稀疏矩阵**: 预留大地址空间，实际只使用一小部分
3. **外部管理的内存**: 由应用层自己管理物理页的分配和释放
4. **按需分页 (demand paging)**: 类似操作系统的虚拟内存

## 故障排查

### 问题1：权限不足
```
无法打开 /dev/nvidiactl: Permission denied
```
**解决**: 使用 sudo 运行

### 问题2：分配失败
```
分配虚拟内存失败: status=0x32
```
**可能原因**:
- VA space 未正确初始化
- 系统不支持该功能
- 驱动版本过旧

### 问题3：观察不到效果
**检查项**:
1. 确认显卡有足够空闲显存
2. 确认没有其他进程在使用显卡
3. 增大分配大小（建议 >= 256MB）

## 延伸实验

### 实验1：测试 map 后的显存占用
在虚拟内存分配后，执行 `NV_ESC_RM_MAP_MEMORY_DMA`，观察此时 nvidia-smi 的变化。

### 实验2：对比不同标志组合
- `VIRTUAL` 单独使用
- `VIRTUAL + LAZY`
- `VIRTUAL + EXTERNALLY_MANAGED`
- `VIRTUAL + LAZY + EXTERNALLY_MANAGED`

### 实验3：大规模虚拟地址空间
分配远超物理显存的虚拟地址空间（如 16GB），验证是否成功且不占用物理显存。

## 参考代码位置

| 功能 | 文件路径 | 行号 |
|-----|---------|------|
| 虚拟内存构造 | `src/nvidia/src/kernel/mem_mgr/virtual_mem.c` | 292-680 |
| 虚拟内存资源分配 | `src/nvidia/src/kernel/mem_mgr/virtual_mem.c` | 720-945 |
| heap 空闲内存查询 | `src/nvidia/src/kernel/gpu/mem_mgr/heap.c` | 2499-2508 |
| 物理内存分配 | `src/nvidia/src/kernel/mem_mgr/video_mem.c` | 533-1630 |
| 标志定义 | `src/common/sdk/nvidia/inc/nvos.h` | 1444-1497 |
