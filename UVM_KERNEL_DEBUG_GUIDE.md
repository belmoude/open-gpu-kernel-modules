# NVIDIA UVM内核调试输出启用指南

## 概述

NVIDIA UVM模块的测试程序在执行时会在内核中产生调试信息和测试结果。要查看这些信息，需要正确配置内核日志系统。

## 🔧 快速启用方法

### 1. 使用提供的脚本（推荐）
```bash
# 运行自动配置脚本
sudo ./enable_uvm_kernel_debug.sh
```

### 2. 手动配置步骤

#### 步骤1: 提升内核日志级别
```bash
# 设置最高日志级别 (显示所有消息)
sudo sh -c 'echo 8 > /proc/sys/kernel/printk'

# 或者设置所有级别
sudo sh -c 'echo "8 8 8 8" > /proc/sys/kernel/printk'
```

#### 步骤2: 启用UVM调试参数
```bash
# 检查可用的UVM参数
ls /sys/module/nvidia_uvm/parameters/

# 启用测试输出 (如果存在)
sudo sh -c 'echo 1 > /sys/module/nvidia_uvm/parameters/uvm_enable_builtin_tests'

# 尝试启用调试打印 (如果存在)
sudo sh -c 'echo 1 > /sys/module/nvidia_uvm/parameters/uvm_debug_prints' 2>/dev/null || true
```

#### 步骤3: 启用动态调试 (如果内核支持)
```bash
# 检查是否支持动态调试
if [[ -f /sys/kernel/debug/dynamic_debug/control ]]; then
    # 启用UVM模块的所有调试输出
    sudo sh -c 'echo "module nvidia_uvm +p" > /sys/kernel/debug/dynamic_debug/control'
    echo "动态调试已启用"
else
    echo "内核不支持动态调试"
fi
```

## 📋 监控UVM内核输出

### 实时监控方法

#### 方法1: 使用dmesg (推荐)
```bash
# 实时查看所有内核消息
dmesg -w

# 只查看UVM相关消息
dmesg -w | grep -i uvm

# 查看测试相关消息
dmesg -w | grep -E "(uvm|test)"
```

#### 方法2: 查看日志文件
```bash
# 查看系统日志
sudo tail -f /var/log/kern.log | grep -i uvm

# 或者查看消息日志
sudo tail -f /var/log/messages | grep -i uvm
```

#### 方法3: 使用journalctl (systemd系统)
```bash
# 实时查看内核消息
sudo journalctl -k -f | grep -i uvm
```

### 历史消息查看
```bash
# 查看最近的UVM消息
dmesg | grep -i uvm | tail -20

# 查看最近的测试消息
dmesg | grep -E "(uvm|test)" | tail -20

# 查看特定时间段的消息
dmesg -T | grep -i uvm
```

## 🧪 测试内核输出

### 完整测试流程

1. **启用调试输出**
   ```bash
   sudo ./enable_uvm_kernel_debug.sh
   ```

2. **在一个终端中监控内核输出**
   ```bash
   dmesg -w | grep -i uvm
   ```

3. **在另一个终端中运行UVM测试**
   ```bash
   # 运行单个测试
   sudo ./run_uvm_tests_final.sh --test RNG_SANITY --verbose
   
   # 或运行所有测试
   sudo ./run_uvm_tests_final.sh --continue --verbose
   ```

### 预期的内核输出示例

正常情况下，您可能会看到类似这样的输出：
```
[12345.678901] nvidia_uvm: UVM test RNG_SANITY starting
[12345.678902] nvidia_uvm: RNG sanity check passed
[12345.678903] nvidia_uvm: UVM test RNG_SANITY completed successfully
```

## 🔍 调试特定问题

### 查看测试失败信息
```bash
# 查看失败的测试信息
dmesg | grep -E "(uvm.*fail|uvm.*error)" -i

# 查看特定测试的输出
dmesg | grep -i "va_residency_info"
```

### 查看内存相关问题
```bash
# 查看内存分配问题
dmesg | grep -E "(uvm.*(alloc|memory|oom))" -i

# 查看GPU相关问题  
dmesg | grep -E "(uvm.*(gpu|device))" -i
```

## 🛠️ 高级调试选项

### 1. UVM DebugFS接口 (如果可用)
```bash
# 检查UVM debugfs
ls /sys/kernel/debug/nvidia_uvm/ 2>/dev/null || echo "DebugFS不可用"

# 查看UVM状态
cat /sys/kernel/debug/nvidia_uvm/status 2>/dev/null || echo "状态文件不存在"
```

### 2. UVM Proc接口
```bash
# 检查UVM proc接口
cat /proc/driver/nvidia/uvm 2>/dev/null || echo "Proc接口不可用"
```

### 3. 模块参数调整
```bash
# 查看所有UVM模块参数
find /sys/module/nvidia_uvm/parameters/ -type f -exec basename {} \; 2>/dev/null

# 查看参数值
for param in /sys/module/nvidia_uvm/parameters/*; do
    echo "$(basename $param): $(cat $param 2>/dev/null)"
done
```

## 📊 输出分析

### UVM测试输出的典型模式

1. **测试开始**: `UVM test [TEST_NAME] starting`
2. **测试进行**: 具体的测试步骤和检查
3. **测试结果**: `UVM test [TEST_NAME] completed` 或 `failed`
4. **错误信息**: 详细的错误原因和堆栈跟踪

### 常见的内核消息类型

- **INFO**: 一般信息，测试进度
- **WARN**: 警告信息，可能的问题
- **ERR**: 错误信息，测试失败原因
- **DEBUG**: 详细的调试信息

## 🚨 故障排除

### 问题1: 没有看到任何UVM消息
**解决方案**:
```bash
# 1. 确认模块已加载
lsmod | grep nvidia_uvm

# 2. 确认测试已启用
cat /sys/module/nvidia_uvm/parameters/uvm_enable_builtin_tests

# 3. 重新加载模块
sudo modprobe -r nvidia_uvm
sudo modprobe nvidia-uvm uvm_enable_builtin_tests=1

# 4. 检查内核日志级别
cat /proc/sys/kernel/printk
```

### 问题2: 消息被淹没在其他日志中
**解决方案**:
```bash
# 使用更精确的过滤
dmesg -w | grep -E "nvidia_uvm|UVM" --line-buffered

# 或者使用时间戳
dmesg -T -w | grep -i uvm
```

### 问题3: 权限问题
**解决方案**:
```bash
# 确保以root身份运行
sudo dmesg -w | grep -i uvm

# 检查日志文件权限
ls -la /var/log/kern.log /var/log/messages
```

## 📝 注意事项

1. **编译选项**: 某些详细的调试输出可能需要UVM模块在编译时启用特定的调试选项
2. **性能影响**: 启用详细调试可能会影响测试性能
3. **日志大小**: 大量的调试输出可能会快速填满日志文件
4. **驱动版本**: 不同版本的NVIDIA驱动可能有不同的调试接口

## 🎯 推荐工作流程

1. 运行 `sudo ./enable_uvm_kernel_debug.sh` 启用调试
2. 在一个终端中运行 `dmesg -w | grep -i uvm`
3. 在另一个终端中运行UVM测试
4. 观察和分析内核输出
5. 根据需要调整调试级别和过滤器