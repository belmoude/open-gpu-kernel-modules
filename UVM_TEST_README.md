# NVIDIA UVM 测试指南

由于NVIDIA没有提供现成的官方UVM测试工具，你需要自己编写用户态程序来调用测试。但不用担心，我已经为你准备了现成的解决方案！

## 快速开始

### 方式1: 使用Shell脚本（最简单）

```bash
# 1. 运行一键测试脚本
sudo ./build_and_test.sh

# 2. 或者分步执行
sudo ./build_and_test.sh --setup    # 只设置环境
sudo ./build_and_test.sh --build    # 只编译程序
sudo ./build_and_test.sh --run      # 只运行测试
```

### 方式2: 使用Python脚本（推荐）

```bash
# 1. 列出所有可用测试
sudo python3 uvm_test_suite.py --list

# 2. 运行所有测试
sudo python3 uvm_test_suite.py

# 3. 运行特定类别测试
sudo python3 uvm_test_suite.py --test memory     # 内存测试
sudo python3 uvm_test_suite.py --test gpu        # GPU硬件测试  
sudo python3 uvm_test_suite.py --test conf       # 机密计算测试
sudo python3 uvm_test_suite.py --test basic      # 基础测试
```

### 方式3: 直接使用C程序

```bash
# 1. 编译测试程序
gcc -o simple_uvm_test simple_uvm_test.c

# 2. 启用UVM测试
sudo modprobe nvidia-uvm uvm_enable_builtin_tests=1

# 3. 运行测试
sudo ./simple_uvm_test
```

## 测试类别说明

| 类别 | 包含测试 | 说明 |
|------|----------|------|
| **basic** | RNG, Range Tree, Lock | 基础数据结构和算法 |
| **memory** | RM Memory, KVMalloc | 内存管理功能 |
| **gpu** | Semaphore, Channel, CE | GPU硬件抽象层 |
| **sync** | Tracker, Push | 同步和操作跟踪 |
| **perf** | Perf Utils, Events | 性能监控工具 |
| **conf** | SEC2, Encryption | 机密计算功能 |

## 前提条件

### 1. 硬件要求
- NVIDIA GPU (支持UVM的型号)
- 对于机密计算测试：需要支持Confidential Computing的GPU (如H100)

### 2. 软件要求
```bash
# 安装必要软件
sudo apt update
sudo apt install build-essential linux-headers-$(uname -r)

# 确保NVIDIA驱动已安装
nvidia-smi  # 应该能正常显示GPU信息
```

### 3. 权限要求
- 必须以root权限运行测试
- UVM模块必须以测试模式加载

## 常见问题解决

### Q1: "UVM device not found"
```bash
# 检查NVIDIA驱动
nvidia-smi

# 手动加载UVM模块
sudo modprobe nvidia-uvm uvm_enable_builtin_tests=1

# 验证设备文件
ls -la /dev/nvidia-uvm
```

### Q2: "UVM builtin tests are NOT enabled"
```bash
# 重新加载模块
sudo modprobe -r nvidia-uvm
sudo modprobe nvidia-uvm uvm_enable_builtin_tests=1

# 验证参数
cat /sys/module/nvidia_uvm/parameters/uvm_enable_builtin_tests
```

### Q3: 编译错误
```bash
# 安装编译工具
sudo apt install build-essential

# 检查头文件路径
find /usr/src -name "uvm_types.h" 2>/dev/null
```

### Q4: 某些测试失败是正常的
以下情况下测试失败是正常的：
- 机密计算测试在不支持的硬件上失败
- 某些高级功能在虚拟环境中不可用
- 多GPU测试在单GPU系统上失败

## 测试结果分析

### 成功的测试输出示例
```
=== Running UVM Tests ===
  RNG_SANITY                ... PASSED
  RANGE_TREE_DIRECTED       ... PASSED
  RM_MEM_SANITY            ... PASSED
  
=== Test Results Summary ===
Total tests:  16
Passed tests: 14
Failed tests: 2
Success rate: 87.5%
```

### 失败原因分析
1. **环境问题**: UVM未正确加载或配置
2. **硬件限制**: 某些功能需要特定GPU型号
3. **权限问题**: 测试需要root权限
4. **驱动版本**: 某些测试可能需要特定驱动版本

## 高级用法

### 自定义测试程序
如果需要测试特定功能，可以参考 `simple_uvm_test.c` 的结构：

```c
// 1. 包含必要头文件
#include <sys/ioctl.h>

// 2. 定义IOCTL命令
#define UVM_TEST_YOUR_TEST UVM_TEST_IOCTL_BASE(XX)

// 3. 打开设备
int uvm_fd = open("/dev/nvidia-uvm", O_RDWR);

// 4. 调用测试
ioctl(uvm_fd, UVM_TEST_YOUR_TEST, &params);

// 5. 检查结果
if (params.rmStatus == 0) printf("PASSED\n");
```

### 集成到CI/CD
```bash
# 在CI脚本中使用
if sudo python3 uvm_test_suite.py --test basic; then
    echo "UVM basic tests passed"
else
    echo "UVM basic tests failed"
    exit 1
fi
```

## 总结

虽然NVIDIA没有提供现成的UVM测试工具，但通过我提供的脚本和程序，你可以：

1. **快速验证UVM功能** - 使用提供的现成脚本
2. **自动化测试** - 集成到开发流程中
3. **定制测试** - 根据需要修改和扩展
4. **结果分析** - 获得详细的测试报告

选择最适合你需求的方式即可！