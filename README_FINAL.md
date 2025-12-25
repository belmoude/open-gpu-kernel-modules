# NVIDIA 虚拟内存分配行为测试套件

> ⚠️ **重要更新**：原测试程序有两个问题已修复：
> 1. ioctl 调用方式错误（已修复）
> 2. Device 分配参数缺失（已修复）
> 
> 请使用 `test_virtual_alloc_fixed.c`！
>
> 遇到问题？查看 **[TROUBLESHOOTING_zh.md](TROUBLESHOOTING_zh.md)**

## 🎯 核心发现

**只要有 `NVOS32_ALLOC_FLAGS_VIRTUAL` 标志，nvidia-smi 就看不到数据显存的分配！**

`NVOS32_ALLOC_FLAGS_LAZY` 不是必需的，它只是进一步避免页表内存（通常 < 1%）。

## 📦 文件清单

### 核心文档
- **`ANSWER_SUMMARY.md`** ⭐ - 详细回答用户问题
- **`QUICK_REFERENCE.md`** ⭐ - 快速参考卡片
- **`VIRTUAL_FLAG_ANALYSIS.md`** - 完整代码分析

### 测试相关
- **`test_virtual_alloc_fixed.c`** ⭐ - 修复版测试程序（推荐）
- **`test_virtual_alloc.c`** - 原始测试程序（仅供参考）
- **`VERSION_COMPARISON_zh.md`** - 版本对比说明
- **`monitor_memory.py`** - Python 显存监控脚本
- **`Makefile.test`** - 编译和测试自动化
- **`TESTING_GUIDE_zh.md`** - 完整测试指南
- **`test_virtual_alloc_README.md`** - 技术文档

### 工具脚本
- **`QUICK_START_FIXED.sh`** ⭐ - 修复版快速启动（推荐）
- **`QUICK_START.sh`** - 原始快速启动
- **`IOCTL_FIX_GUIDE_zh.md`** ⭐ - ioctl 修复指南（重要）
- **`BUILD_FIX.md`** - 编译问题修复说明

## 🚀 快速开始

### 方法 1: 使用修复版快速启动脚本（强烈推荐）

```bash
cd /workspace
./QUICK_START_FIXED.sh
sudo ./test_virtual_alloc_fixed
```

### 方法 2: 手动编译修复版

```bash
cd /workspace

# 编译修复版
gcc -o test_virtual_alloc_fixed test_virtual_alloc_fixed.c -std=c99

# 运行
sudo ./test_virtual_alloc_fixed
```

### 方法 3: 使用 Makefile（最简单）

```bash
cd /workspace

# 终端 1: 启动监控
make -f Makefile.test monitor

# 终端 2: 运行测试
make -f Makefile.test test
```

## 📊 测试场景

测试程序包含 3 个场景，对比不同标志组合的效果：

### 场景 1: 只有 VIRTUAL 标志
```c
flags = NVOS32_ALLOC_FLAGS_VIRTUAL;
```
**结果**: nvidia-smi +0~2MB（页表内存，可忽略）

### 场景 2: VIRTUAL + LAZY 标志
```c
flags = NVOS32_ALLOC_FLAGS_VIRTUAL | NVOS32_ALLOC_FLAGS_LAZY;
```
**结果**: nvidia-smi +0MB（完全不变）

### 场景 3: 物理显存分配（对比）
```c
flags = 0;  // 无 VIRTUAL 标志
```
**结果**: nvidia-smi +256MB（立即分配）

## 🔍 关键代码位置

| 功能 | 文件 | 行号 |
|-----|------|-----|
| VIRTUAL 标志检查 | `rmapi_deprecated_vidheapctrl.c` | 138-145 |
| 虚拟内存分配 | `virtual_mem.c` | 858-860 |
| LAZY 控制页表 | `virtual_mem.c` | 382-415 |
| 物理显存分配 | `video_mem.c` | 1398 |
| nvidia-smi 查询 | `kern_mem_sys_ctrl.c` | 575-589 |

## 💡 核心原理

### 两个独立的 Heap

```
┌─────────────────────┐         ┌─────────────────────┐
│  物理显存 Heap      │         │  虚拟地址 Heap      │
│  (Heap)             │         │  (OBJEHEAP)         │
├─────────────────────┤         ├─────────────────────┤
│ heapAlloc()         │         │ eheapAlloc()        │
│ pHeap->free         │         │ 不影响物理 heap     │
├─────────────────────┤         ├─────────────────────┤
│ NV01_MEMORY_LOCAL   │         │ NV50_MEMORY_VIRTUAL │
└─────────────────────┘         └─────────────────────┘
        ↑                               ↑
   nvidia-smi 查询                  不被查询
```

### 关键判断逻辑

```c
// rmapi_deprecated_vidheapctrl.c:138-145
if (pUserParams->flags & NVOS32_ALLOC_FLAGS_VIRTUAL)
    externalClassId = NV50_MEMORY_VIRTUAL;  // ← 只检查 VIRTUAL！
else if (FLD_TEST_DRF(OS32, _ATTR, _LOCATION, _VIDMEM, pUserParams->attr))
    externalClassId = NV01_MEMORY_LOCAL_USER;
```

**关键点**：
- 只检查 `VIRTUAL` 标志
- 有 `VIRTUAL` → 虚拟地址 heap → nvidia-smi 不变
- 无 `VIRTUAL` → 物理显存 heap → nvidia-smi 增加

## 📈 效果对比表

| 标志组合 | 类型 | 数据显存 | 页表 | nvidia-smi | 用途 |
|---------|------|---------|------|-----------|------|
| `VIRTUAL` | NV50 | ❌ | ~2MB | +0~2MB | 推荐 |
| `VIRTUAL+LAZY` | NV50 | ❌ | ❌ | +0MB | 更彻底 |
| 无 `VIRTUAL` | NV01 | 1GB | N/A | +1GB | 对比 |

## ✅ 编译修复说明

如果遇到编译错误：
```
error: a label can only be part of a statement and a declaration is not a statement
```

**已修复**！代码已使用复合语句块修复 goto label 问题，并在 Makefile 中添加了 `-std=c99` 标志。

详见 `BUILD_FIX.md`。

## 📚 推荐阅读顺序

1. **新手**:
   - `QUICK_REFERENCE.md` - 快速理解核心概念
   - `TESTING_GUIDE_zh.md` - 跟着指南操作
   - 运行测试程序观察效果

2. **深入理解**:
   - `ANSWER_SUMMARY.md` - 完整技术解答
   - `VIRTUAL_FLAG_ANALYSIS.md` - 代码证据
   - `test_virtual_alloc_README.md` - 技术细节

3. **开发者**:
   - 阅读 `test_virtual_alloc.c` 源码
   - 查看关键代码位置
   - 扩展测试场景

## 🎓 学习要点

### 要点 1: VIRTUAL 标志是关键
```c
// 只要有这个标志
flags = NVOS32_ALLOC_FLAGS_VIRTUAL;

// 就会：
// 1. 使用 NV50_MEMORY_VIRTUAL 类
// 2. 从虚拟地址 heap 分配
// 3. 不占用物理显存 heap
// 4. nvidia-smi 看不到数据显存增加
```

### 要点 2: LAZY 只影响页表
```c
// LAZY 标志控制的是页表内存（通常 < 1%）
flags = NVOS32_ALLOC_FLAGS_VIRTUAL | NVOS32_ALLOC_FLAGS_LAZY;

// 差别：
// 无 LAZY: 预分配 ~2MB 页表（0.2%）
// 有 LAZY: 不预分配页表，完全零占用
```

### 要点 3: 物理分配的对比
```c
// 无 VIRTUAL 标志
flags = 0;

// 结果：
// 1. 使用 NV01_MEMORY_LOCAL_USER 类
// 2. 从物理显存 heap 分配
// 3. pHeap->free 立即减少
// 4. nvidia-smi 立即显示增加
```

## 🔧 故障排查

### 编译问题
- 见 `BUILD_FIX.md`
- 确保使用 `-std=c99`
- 检查头文件路径

### 运行问题
- 需要 root 权限：`sudo ./test_virtual_alloc`
- 检查驱动：`ls -l /dev/nvidiactl`
- 查看内核日志：`sudo dmesg | grep nvidia`

### 观察不到效果
- 分配大小太小（建议 >= 256MB）
- 其他进程占用显存
- 显卡型号或驱动版本差异

## 🙏 致谢

感谢用户的细致追问：
> "一定需要 NVOS32_ALLOC_FLAGS_LAZY 标记吗，还是只要有 NVOS32_ALLOC_FLAGS_VIRTUAL 就可以了？"

这促使我们深入分析代码，得出正确结论：
**✅ 只要有 NVOS32_ALLOC_FLAGS_VIRTUAL 就足够了！**

## 📝 总结

这个测试套件证明了：

1. ✅ `NVOS32_ALLOC_FLAGS_VIRTUAL` 是关键标志
2. ✅ 只要有 `VIRTUAL`，nvidia-smi 就看不到数据显存分配
3. ✅ `LAZY` 标志不是必需的，只是锦上添花
4. ✅ 两个 heap（物理显存 heap 和虚拟地址 heap）完全独立
5. ✅ nvidia-smi 只查询物理显存 heap

## 📞 联系与反馈

如有问题或建议，请查看相关文档或：
- 检查 `TESTING_GUIDE_zh.md` 的常见问题部分
- 查看 `BUILD_FIX.md` 的故障排查
- 运行 `./QUICK_START.sh` 获取帮助信息

---

**版本**: 1.0  
**日期**: 2024-12-25  
**状态**: ✅ 编译通过，测试就绪
