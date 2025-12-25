# 快速参考卡片

## 用户问题

> 一定需要 NVOS32_ALLOC_FLAGS_LAZY 标记吗，还是只要有 NVOS32_ALLOC_FLAGS_VIRTUAL 就可以了？

## 答案

**只要有 `NVOS32_ALLOC_FLAGS_VIRTUAL` 就够了！**

## 一句话总结

`VIRTUAL` 标志决定是否分配物理显存（nvidia-smi 是否变化），`LAZY` 只是进一步控制页表内存（通常可忽略）。

## 快速对比表

| 标志组合 | 类型 | 虚拟地址 | 页表内存 | 数据显存 | nvidia-smi |
|---------|------|---------|---------|---------|-----------|
| 只有 `VIRTUAL` | `NV50_MEMORY_VIRTUAL` | ✅ | ~2MB (0.2%) | ❌ | +0~2MB |
| `VIRTUAL + LAZY` | `NV50_MEMORY_VIRTUAL` | ✅ | ❌ | ❌ | +0MB |
| 无 `VIRTUAL` | `NV01_MEMORY_LOCAL_USER` | ❌ | N/A | ✅ 1GB | +1GB |

## 关键代码

```c
// rmapi_deprecated_vidheapctrl.c:138-145
if (pUserParams->flags & NVOS32_ALLOC_FLAGS_VIRTUAL)
    externalClassId = NV50_MEMORY_VIRTUAL;  // ← 只检查 VIRTUAL！
else if (FLD_TEST_DRF(OS32, _ATTR, _LOCATION, _VIDMEM, pUserParams->attr))
    externalClassId = NV01_MEMORY_LOCAL_USER;
```

## 两个独立的 Heap

```
物理显存 Heap              虚拟地址 Heap
    ↓                          ↓
NV01_MEMORY_LOCAL_USER    NV50_MEMORY_VIRTUAL
    ↓                          ↓
heapAlloc()                eheapAlloc()
    ↓                          ↓
pHeap->free -= 1GB         不影响物理 heap
    ↓                          ↓
nvidia-smi +1GB            nvidia-smi 不变
```

## 推荐用法

### 最简单（足够用）
```c
params.flags = NVOS32_ALLOC_FLAGS_VIRTUAL;
```
- nvidia-smi: +0~2MB（页表）
- 数据显存: 不分配

### 更彻底（完全零占用）
```c
params.flags = NVOS32_ALLOC_FLAGS_VIRTUAL | 
               NVOS32_ALLOC_FLAGS_LAZY;
```
- nvidia-smi: +0MB
- 连页表也不分配

## 类比理解

图书馆类比：
- **书架**（物理显存）：存放实体书
- **目录**（虚拟地址）：地址索引
- **卡片**（页表）：指向书的位置

**有 VIRTUAL**:
- 在目录预留（虚拟地址）✅
- 准备卡片（页表，~2MB）⚠️ LAZY 控制这个
- **不占书架**（物理显存）❌ ← 关键！

**无 VIRTUAL**:
- 占用书架（物理显存）✅
- nvidia-smi 立即看到

## 测试验证

```bash
# 编译
cd /workspace
make -f Makefile.test

# 终端1: 监控
make -f Makefile.test monitor

# 终端2: 测试
make -f Makefile.test test
```

观察三个场景：
1. 只有 VIRTUAL → nvidia-smi +0~2MB
2. VIRTUAL + LAZY → nvidia-smi +0MB
3. 物理分配 → nvidia-smi +256MB

## 关键文件

| 文档 | 说明 |
|-----|------|
| `ANSWER_SUMMARY.md` | 详细技术分析 |
| `VIRTUAL_FLAG_ANALYSIS.md` | 代码证据汇总 |
| `TESTING_GUIDE_zh.md` | 测试指南 |
| `test_virtual_alloc.c` | 测试程序 |

## 常见误解

### ❌ 误解1: 必须有 LAZY 才不分配显存
**✅ 正确**: 只要有 VIRTUAL 就不分配**数据**显存，LAZY 只影响页表

### ❌ 误解2: nvidia-smi 查询虚拟地址使用量
**✅ 正确**: nvidia-smi 只查询**物理显存 heap**，虚拟地址不被统计

### ❌ 误解3: 虚拟内存和物理内存用同一个 heap
**✅ 正确**: 两个**完全独立**的 heap，不互相影响

## 底线

**记住这一条就够了**：
> 只要有 `NVOS32_ALLOC_FLAGS_VIRTUAL`，nvidia-smi 就看不到**数据显存**的增加。LAZY 只是让它更彻底（连页表也没有）。
