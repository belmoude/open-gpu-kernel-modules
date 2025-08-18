# UVM整合测试程序使用指南

我已经创建了一个智能的UVM测试程序，它能自动检测哪些测试需要VA space，并在需要时自动创建和管理。

## 智能测试程序特点

### 🧠 智能VA Space管理
- **自动检测**: 程序知道哪些测试需要VA space
- **按需创建**: 只在需要时创建VA space
- **自动清理**: 测试完成后自动清理资源
- **错误恢复**: 出错时自动清理，避免资源泄露

### 📊 测试分类执行
```
基础测试 (无需VA Space)
├── RNG_SANITY                 # 随机数生成器
├── RANGE_TREE_DIRECTED        # 范围树
├── LOCK_SANITY               # 锁机制
├── KVMALLOC                  # 内核内存分配
└── RANGE_ALLOCATOR_SANITY    # 范围分配器

高级测试 (需要VA Space)
├── RM_MEM_SANITY             # RM内存管理
├── GPU_SEMAPHORE_SANITY      # GPU信号量
├── TRACKER_SANITY            # 跟踪器
├── PUSH_SANITY               # Push机制
├── CHANNEL_SANITY            # 通道管理
├── CE_SANITY                 # 拷贝引擎
├── PERF_UTILS_SANITY         # 性能工具
├── PERF_EVENTS_SANITY        # 性能事件
├── PERF_MODULE_SANITY        # 性能模块
└── FAULT_BUFFER_FLUSH        # 故障缓冲区

机密计算测试 (需要VA Space + 特殊硬件)
├── SEC2_SANITY               # SEC2引擎
└── SEC2_CPU_GPU_ROUNDTRIP    # SEC2往返测试
```

## 使用方法

### 方法1: 一键运行（推荐）

```bash
# 自动编译、设置环境、运行测试
sudo ./build_and_test.sh
```

**程序会自动**：
1. 编译智能测试程序
2. 设置UVM环境
3. 运行所有测试
4. 自动管理VA space
5. 清理资源

### 方法2: 手动步骤

```bash
# 1. 编译智能测试程序
gcc -o smart_uvm_test smart_uvm_test.c

# 2. 确保UVM测试已启用
sudo modprobe nvidia-uvm uvm_enable_builtin_tests=1

# 3. 运行测试
sudo ./smart_uvm_test
```

### 方法3: 分步调试

```bash
# 1. 先运行最简单的测试验证IOCTL
gcc -o test_ioctl_simple test_ioctl_simple.c
sudo ./test_ioctl_simple

# 2. 如果基础IOCTL工作，运行智能测试
sudo ./smart_uvm_test

# 3. 如果需要调试，运行调试脚本
sudo ./debug_uvm_ioctl.sh
```

## 预期输出

### 成功的输出示例
```
=== 智能NVIDIA UVM测试程序 ===
版本: 2.0 (支持自动VA Space管理)

=== 检查UVM测试环境 ===
✅ UVM设备文件存在
✅ UVM内置测试已启用
✅ UVM设备打开成功
  初始化UVM... ✅

=== 运行UVM测试套件 ===
总测试数: 17

--- 基础测试（无需VA Space） ---
  RNG_SANITY                ... ✅ PASSED
  RANGE_TREE_DIRECTED       ... ✅ PASSED
  LOCK_SANITY               ... ✅ PASSED
  KVMALLOC                  ... ✅ PASSED
  RANGE_ALLOCATOR_SANITY    ... ✅ PASSED

--- 高级测试（需要VA Space） ---
  创建VA Space... ✅ (handle=0x7f8b4c000000)
  RM_MEM_SANITY             ... ✅ PASSED
  GPU_SEMAPHORE_SANITY      ... ✅ PASSED
  TRACKER_SANITY            ... ✅ PASSED
  PUSH_SANITY               ... ✅ PASSED
  CHANNEL_SANITY            ... ✅ PASSED
  CE_SANITY                 ... ✅ PASSED
  PERF_UTILS_SANITY         ... ✅ PASSED
  PERF_EVENTS_SANITY        ... ✅ PASSED
  PERF_MODULE_SANITY        ... ✅ PASSED
  FAULT_BUFFER_FLUSH        ... ✅ PASSED

--- 机密计算测试（需要VA Space + 特殊硬件） ---
  SEC2_SANITY               ... ❌ FAILED (硬件不支持)
  SEC2_CPU_GPU_ROUNDTRIP    ... ❌ FAILED (硬件不支持)

=== 测试结果总结 ===
总测试数:   17
通过测试:   15
失败测试:   2
成功率:     88.2%

✅ 部分测试通过，这通常是正常的
失败原因可能包括:
  - 硬件功能不支持（如机密计算）
  - 虚拟环境限制
  - 特定GPU功能缺失

=== 清理资源 ===
  销毁VA Space... ✅
  反初始化UVM... ✅
```

## 程序优势

### 🎯 智能化
- **自动检测**: 知道哪些测试需要什么前提条件
- **按需分配**: 只在需要时创建VA space
- **智能分类**: 按功能类别组织测试执行

### 🛡️ 健壮性
- **错误处理**: 完善的错误检测和报告
- **资源管理**: 自动清理，防止资源泄露
- **容错能力**: 单个测试失败不影响其他测试

### 📋 可维护性
- **清晰输出**: 分类显示测试结果
- **详细诊断**: 失败时提供详细错误信息
- **易于扩展**: 添加新测试只需修改测试列表

## 立即行动

现在你可以直接运行：

```bash
# 最简单的方式 - 一键测试
sudo ./build_and_test.sh
```

这个程序会：
1. ✅ 自动处理VA space的创建和销毁
2. ✅ 智能地知道哪些测试需要什么前提条件
3. ✅ 提供清晰的测试结果分类
4. ✅ 在出错时自动清理资源
5. ✅ 给出失败测试的可能原因

**你不再需要手动管理VA space或担心资源清理问题！**