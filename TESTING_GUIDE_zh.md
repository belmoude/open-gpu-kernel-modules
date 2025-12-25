# NVIDIA 虚拟内存延迟分配测试指南

## 问题背景

在处理 `NV_ESC_RM_VID_HEAP_CONTROL` + `NVOS32_FUNCTION_ALLOC_SIZE` 时，**是否可能出现不立即分配显存的情况**？

**答案：是的！** 当使用特定标志组合时，确实会出现"只保留虚拟地址，不立即分配物理显存"的情况。

## 两种分配模式对比

### 模式 1：立即分配物理显存（默认）

```c
NVOS32_PARAMETERS params = {
    .function = NVOS32_FUNCTION_ALLOC_SIZE,
    .data.AllocSize.flags = 0,  // 无特殊标志
    .data.AllocSize.attr = NVOS32_ATTR_LOCATION_VIDMEM,
    .data.AllocSize.size = 256 * 1024 * 1024,  // 256MB
};
```

**行为**:
- ✅ 立即从 heap 分配物理显存
- ✅ `pHeap->free` 立即减少
- ✅ nvidia-smi 立即显示显存使用增加

### 模式 2：延迟分配（只需要 VIRTUAL 标志）

```c
NV_MEMORY_ALLOCATION_PARAMS params = {
    .flags = NVOS32_ALLOC_FLAGS_VIRTUAL,  // 只要有 VIRTUAL 就够了！
    .attr = NVOS32_ATTR_LOCATION_VIDMEM,
    .size = 1024 * 1024 * 1024,  // 1GB 虚拟地址
};

// 使用 NV50_MEMORY_VIRTUAL 类
```

**行为**:
- ✅ 只在 VA space 中保留地址范围
- ❌ **不**从 heap 分配物理显存
- ❌ `pHeap->free` **不**变化
- ❌ nvidia-smi **不**显示数据显存增加
- ⚠️ 可能有约 2MB 页表内存（通常 < 1%，可忽略）
- ⏰ 物理显存在后续 **map 操作时才分配**

**可选优化**：加上 LAZY 标志
```c
.flags = NVOS32_ALLOC_FLAGS_VIRTUAL | NVOS32_ALLOC_FLAGS_LAZY,
```
- 连页表内存也不预分配
- nvidia-smi **完全**不变

## 关键代码证据

### 1. 延迟分配判断逻辑

**文件**: `src/nvidia/src/kernel/mem_mgr/virtual_mem.c`
**行号**: 382-415

```c
if (memmgrIsPmaInitialized(pMemoryManager) &&
    !(pAllocData->flags & NVOS32_ALLOC_FLAGS_LAZY) &&
    !(pAllocData->flags & NVOS32_ALLOC_FLAGS_EXTERNALLY_MANAGED))
{
    // 只有不带 LAZY/EXTERNALLY_MANAGED 时才预分配物理内存
    // 如果带这些标志，会跳过这个代码块，不分配物理内存
    
    status = vaspaceReserveMempool(pVAS, pGpu, pDevice,
                                   size, pageSizeLockMask,
                                   VASPACE_RESERVE_FLAGS_NONE);
}
```

**关键点**: 
- 条件是 `!(flags & LAZY) && !(flags & EXTERNALLY_MANAGED)`
- 当有 LAZY 或 EXTERNALLY_MANAGED 标志时，跳过物理内存预分配
- 只保留虚拟地址空间

### 2. nvidia-smi 查询的数据源

**文件**: `src/nvidia/src/kernel/gpu/mem_mgr/heap.c`
**行号**: 2499-2508

```c
NV_STATUS heapGetFree_IMPL(Heap *pHeap, NvU64 *free)
{
    *free = pHeap->free;  // nvidia-smi 显示的空闲显存来自这里
    HEAP_VALIDATE(pHeap);
    return (NV_OK);
}
```

**关键点**:
- nvidia-smi 查询的是 `pHeap->free`
- 虚拟内存分配（LAZY）不会修改这个值
- 只有真正的物理内存分配才会减少 `pHeap->free`

### 3. 物理内存分配时的更新

**文件**: `src/nvidia/src/kernel/gpu/mem_mgr/heap.c`
**行号**: 3120

```c
// 在 _heapProcessFreeBlock 函数中
// 减少空闲显存量
_heapAdjustFree(pHeap, -((NvS64) (pBlockNew->end - pBlockNew->begin + 1)));
```

**关键点**:
- 只有实际分配物理内存时才调用 `_heapAdjustFree`
- 虚拟内存分配不会走到这里

## 测试环境准备

### 1. 编译测试程序

```bash
cd /workspace

# 方法 1: 使用 Makefile（推荐）
make -f Makefile.test

# 方法 2: 手动编译
gcc -o test_virtual_alloc test_virtual_alloc.c \
    -std=c99 \
    -I./src/common/sdk/nvidia/inc \
    -I./src/nvidia/arch/nvalloc/unix/include
```

**注意**: 使用 `-std=c99` 确保编译器支持 C99 标准。

### 2. 准备 Python 监控脚本

```bash
# 安装依赖（可选，用于更详细的监控）
pip3 install pynvml

# 给脚本添加执行权限
chmod +x monitor_memory.py
```

## 测试步骤

### 方案 A: 使用 Makefile（推荐）

**终端 1 - 监控显存**:
```bash
make -f Makefile.test monitor
```

**终端 2 - 运行测试**:
```bash
make -f Makefile.test test
```

### 方案 B: 手动执行

**终端 1 - 监控显存**:
```bash
# 方式 1: 使用 Python 脚本（推荐）
python3 monitor_memory.py

# 方式 2: 使用 watch + nvidia-smi
watch -n 1 nvidia-smi
```

**终端 2 - 运行测试**:
```bash
sudo ./test_virtual_alloc
```

## 预期测试结果

### 阶段 1: 初始状态
```
请在另一个终端运行 'nvidia-smi' 查看显存使用情况
按回车继续...
```

**nvidia-smi 显示**: 基线显存使用量（记录下来）

---

### 阶段 2: 分配虚拟内存（只有 VIRTUAL）
```
>>> 场景1：分配虚拟内存（只有 VIRTUAL 标志）- 不应立即占用数据显存 <<<
成功分配虚拟内存对象: 0x87654321 (大小: 1024 MB)
虚拟地址偏移: 0x...

按回车继续...
```

**nvidia-smi 显示**: 
- ❌ **显存使用量基本不变**（可能 +1~2MB 页表）
- ✅ **这证明了只要有 VIRTUAL 标志，数据显存就不会分配**

---

### 阶段 3: 分配虚拟内存（VIRTUAL + LAZY）
```
>>> 场景2：分配虚拟内存（VIRTUAL + LAZY）- 完全不占用显存 <<<
成功分配虚拟内存对象: 0x98765432 (大小: 1024 MB)
虚拟地址偏移: 0x...

按回车继续...
```

**nvidia-smi 显示**: 
- ❌ **显存使用量完全不变**（连页表也没有）
- ✅ **这证明了 LAZY 只是进一步避免页表内存**

---

### 阶段 4: 分配物理显存（无 VIRTUAL 标志）
```
>>> 场景3：分配物理显存（不带 VIRTUAL/LAZY 标志）- 应立即占用物理显存 <<<
成功分配物理显存: 0x12345678 (大小: 256 MB)

按回车继续...
```

**nvidia-smi 显示**:
- ✅ **显存使用量增加约 256MB**
- ✅ **这证明了物理显存分配立即占用显存**
- ✅ **对比验证了 VIRTUAL 标志的作用**

---

### 阶段 5: 清理
```
已释放物理显存
测试完成，所有资源已清理
```

**nvidia-smi 显示**: 显存使用量恢复到基线

## 关键观察点

### ✅ 成功标志

1. **场景1（只有 VIRTUAL）**:
   - `nvidia-smi` 显示的已用显存 **≈** 基线 + 0~2MB
   - 页表内存占用（通常可忽略）

2. **场景2（VIRTUAL + LAZY）**:
   - `nvidia-smi` 显示的已用显存 **≈** 基线 + 0MB
   - 完全没有增加

3. **场景3（物理显存）**:
   - `nvidia-smi` 显示的已用显存 **≈** 基线 + 256MB
   - 有明显增加

4. **清理后**:
   - `nvidia-smi` 显示的已用显存 **≈** 基线
   - 恢复到初始状态

### ❌ 失败情况

如果虚拟内存分配后 `nvidia-smi` 显示显存明显增加（> 10MB），可能原因：
1. 标志设置错误（**没有 VIRTUAL 标志**）
2. 驱动版本不支持虚拟内存分配
3. 系统配置问题

**注意**：
- 场景1 有 1~2MB 的页表内存是**正常的**
- 只有场景2（VIRTUAL + LAZY）才完全不增加

## 标志详解

### ⭐ NVOS32_ALLOC_FLAGS_VIRTUAL (0x00080000) **← 关键标志**
- **这是最关键的标志**
- 指示这是虚拟内存分配
- 使用 `NV50_MEMORY_VIRTUAL` 类而不是 `NV01_MEMORY_LOCAL_USER`
- 从 VA space heap 分配，**不涉及物理显存 heap**
- nvidia-smi 看不到数据显存增加

### NVOS32_ALLOC_FLAGS_LAZY (0x00000400) **← 辅助标志**
- **这不是必需的**，只是锦上添花
- 控制页表内存是否预先分配
- 页表内存通常 < 数据大小的 1%
- 有 LAZY: 连页表也不分配（nvidia-smi 完全不变）
- 无 LAZY: 预分配页表（nvidia-smi 可能 +1~2MB）

### NVOS32_ALLOC_FLAGS_EXTERNALLY_MANAGED (0x00400000)
- 外部管理模式
- 物理页的分配由外部（如应用层）管理
- 与 LAZY 类似，也会跳过页表内存预分配

### NVOS32_ALLOC_FLAGS_VIRTUAL_ONLY
组合标志，定义在 `nvos.h:1491-1497`:
```c
#define NVOS32_ALLOC_FLAGS_VIRTUAL_ONLY ( \
    NVOS32_ALLOC_FLAGS_VIRTUAL          | \
    NVOS32_ALLOC_FLAGS_LAZY             | \
    NVOS32_ALLOC_FLAGS_EXTERNALLY_MANAGED | \
    NVOS32_ALLOC_FLAGS_SPARSE           | \
    NVOS32_ALLOC_FLAGS_MAXIMIZE_ADDRESS_SPACE | \
    NVOS32_ALLOC_FLAGS_PREFER_PTES_IN_SYSMEMORY )
```

## 实际应用场景

### 1. UVM (Unified Virtual Memory)
- 预留大地址空间
- 按需将数据在 CPU/GPU 间迁移
- 物理页延迟分配

### 2. 稀疏矩阵/稀疏张量
- 分配大虚拟地址范围（如 100GB）
- 实际只使用少量物理内存（如 1GB）
- 节省物理显存

### 3. 按需分页（Demand Paging）
- 类似操作系统虚拟内存
- 首次访问时触发缺页
- 动态分配物理页

### 4. 外部内存管理
- 应用层自己管理物理页池
- RM 只提供虚拟地址映射
- 灵活的内存分配策略

## 调试技巧

### 1. 内核日志
```bash
# 查看 NVIDIA 驱动日志
sudo dmesg | grep -i nvidia | tail -50
```

### 2. 详细的 nvidia-smi
```bash
# 查看详细内存信息
nvidia-smi --query-gpu=index,memory.total,memory.used,memory.free --format=csv

# 持续监控
nvidia-smi dmon -s m
```

### 3. 进程级显存监控
```bash
# 查看特定进程的显存使用
nvidia-smi pmon -s m
```

### 4. 增加测试程序的调试输出
在 `test_virtual_alloc.c` 中添加：
```c
printf("DEBUG: flags=0x%x, attr=0x%x\n", params.flags, params.attr);
```

## 常见问题

### Q1: 为什么需要 root 权限？
**A**: `/dev/nvidiactl` 设备节点默认只有 root 或 video 组成员可访问。

**解决方案**:
```bash
# 方法 1: 使用 sudo
sudo ./test_virtual_alloc

# 方法 2: 添加用户到 video 组
sudo usermod -a -G video $USER
# 需要重新登录生效
```

### Q2: 编译失败，找不到头文件
**A**: 确保在正确的目录执行，且代码仓库完整。

```bash
# 检查头文件是否存在
ls src/common/sdk/nvidia/inc/nvos.h
ls src/nvidia/arch/nvalloc/unix/include/nv_escape.h
```

### Q3: 运行时返回错误码 0x32
**A**: 错误码 0x32 = `NV_ERR_INVALID_ARGUMENT`，可能原因：
1. VA space 未正确初始化
2. 参数设置错误
3. 驱动版本不匹配

### Q4: nvidia-smi 显示的变化很小或不明显
**A**: 可能原因：
1. 测试分配的内存太小（建议 >= 256MB）
2. 系统有其他进程在使用显存
3. 驱动有内存缓存机制

**解决方案**:
- 增大测试分配的内存大小
- 关闭其他 GPU 应用
- 多次运行观察趋势

## 总结

通过本测试，您可以验证：

1. ✅ **使用 `NVOS32_ALLOC_FLAGS_VIRTUAL + LAZY`**:
   - 只保留虚拟地址
   - 不立即分配物理显存
   - nvidia-smi 不显示显存增加

2. ✅ **普通的 `NVOS32_FUNCTION_ALLOC_SIZE`**:
   - 立即分配物理显存
   - heap->free 立即减少
   - nvidia-smi 立即显示显存增加

3. ✅ **物理显存分配时机**:
   - 虚拟内存：在后续 map 操作时
   - 物理内存：在 alloc 时立即分配

## 相关资源

- 测试程序源码: `test_virtual_alloc.c`
- 监控脚本: `monitor_memory.py`
- 编译脚本: `Makefile.test`
- 详细说明: `test_virtual_alloc_README.md`

## 下一步实验

1. **测试 Map 操作**: 在虚拟内存分配后调用 `NV_ESC_RM_MAP_MEMORY_DMA`，观察此时 nvidia-smi 的变化
2. **测试不同标志组合**: 对比 LAZY、EXTERNALLY_MANAGED、SPARSE 等标志的效果
3. **压力测试**: 分配超大虚拟地址空间（如 32GB），验证系统行为
4. **性能测试**: 对比延迟分配和立即分配的性能差异
