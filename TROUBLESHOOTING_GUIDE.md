# UVM测试程序故障排除指南

## 问题：程序直接退出，没有执行测试

### 症状
```bash
./run_uvm_tests.sh
# 输出：
Error: UVM device /dev/nvidia-uvm not found.
Make sure the nvidia-uvm module is loaded.
```

### 解决方案

#### 1. 检查UVM设备是否存在
```bash
ls -la /dev/nvidia-uvm
```

#### 2. 如果设备存在但权限不足
```bash
# 检查当前用户权限
whoami
ls -la /dev/nvidia-uvm

# 解决方案A: 以root身份运行
sudo ./run_uvm_tests.sh

# 解决方案B: 临时修改权限
sudo chmod 666 /dev/nvidia-uvm
./run_uvm_tests.sh
```

#### 3. 如果设备不存在，加载UVM模块
```bash
# 加载UVM模块并启用测试功能
sudo modprobe nvidia-uvm uvm_enable_builtin_tests=1

# 验证模块已加载
lsmod | grep nvidia_uvm

# 验证设备文件已创建
ls -la /dev/nvidia-uvm
```

#### 4. 检查NVIDIA驱动状态
```bash
# 检查NVIDIA驱动
nvidia-smi

# 检查驱动版本
cat /proc/driver/nvidia/version

# 检查GPU设备
ls -la /dev/nvidia*
```

## 测试执行示例

### 成功的执行流程应该是：
```bash
$ sudo ./run_uvm_tests.sh --verbose --continue

UVM Test Runner - NVIDIA UVM Driver Test Suite
==============================================

GPU detected. All tests can be executed.

Starting UVM test execution...
Total tests to run: 97

Running test: GET_GPU_REF_COUNT                [PASS]
  Description: Get GPU reference count
  Command ID: 200
  Requires GPU: Yes
  Executing... 
  Result: Test completed successfully

Running test: RNG_SANITY                       [PASS]
  Description: Random number generator sanity test
  Command ID: 201
  Requires GPU: No
  Executing... 
  Result: Test completed successfully

... (继续执行其他测试)

Test Execution Summary
=====================
Total tests:     97
Passed:          85
Failed:          12
Skipped:         0
Success rate:    87.6%
Execution time:  45 seconds
```

## 常见错误和解决方案

### 错误1: "Permission denied"
```bash
# 问题：权限不足
# 解决：以root身份运行或修改设备权限
sudo ./run_uvm_tests.sh
```

### 错误2: "No such device"
```bash
# 问题：UVM模块未加载或测试未启用
# 解决：重新加载模块
sudo modprobe -r nvidia_uvm
sudo modprobe nvidia-uvm uvm_enable_builtin_tests=1
```

### 错误3: GPU相关测试失败
```bash
# 问题：系统没有NVIDIA GPU
# 解决：这是正常现象，GPU测试需要实际的NVIDIA硬件
# 可以只运行非GPU测试：
./run_uvm_tests.sh --filter "SANITY|KVMALLOC|MEM_SANITY|LOCK_SANITY"
```

## 验证测试程序本身

如果您想验证测试程序的功能（不需要实际硬件）：

```bash
# 1. 验证程序基本功能
./validate_test_runner.sh

# 2. 查看所有可用测试
./run_uvm_tests.sh --list

# 3. 查看帮助信息
./run_uvm_tests.sh --help

# 4. 运行演示模式（模拟执行）
./simple_uvm_test_demo.sh
```

## 系统要求检查清单

- [ ] NVIDIA GPU硬件已安装
- [ ] NVIDIA驱动程序已安装
- [ ] nvidia-uvm内核模块已加载
- [ ] uvm_enable_builtin_tests=1 参数已设置
- [ ] /dev/nvidia-uvm设备文件存在
- [ ] 用户有适当的设备访问权限
- [ ] Python 3已安装（用于ioctl调用）

## 联系信息

如果问题持续存在，请提供以下信息：
1. 操作系统版本
2. NVIDIA驱动版本
3. GPU型号
4. 错误消息的完整输出
5. dmesg中的相关错误信息