# UVM测试全部失败 - 快速修复指南

## 🚨 问题症状
- 所有97个UVM测试都失败
- 成功率0%
- `modprobe nvidia-uvm uvm_enable_builtin_tests=1` 已执行
- `/dev/nvidia-uvm` 设备存在

## 🔧 立即执行的诊断步骤

### 步骤1: 运行最小化测试
```bash
sudo python3 minimal_uvm_test.py
```

### 步骤2: 检查UVM模块参数
```bash
cat /sys/module/nvidia_uvm/parameters/uvm_enable_builtin_tests
```
**预期结果**: 应该显示 `1` 或 `Y`

### 步骤3: 检查设备权限
```bash
ls -la /dev/nvidia-uvm
sudo chmod 666 /dev/nvidia-uvm  # 如果权限不足
```

### 步骤4: 以root身份测试单个用例
```bash
sudo ./run_uvm_tests.sh --test RNG_SANITY --verbose --debug
```

## 🎯 常见问题和解决方案

### 问题1: EINVAL错误 (最常见)
**症状**: 所有ioctl调用返回"Invalid argument"
**原因**: 测试功能未真正启用
**解决方案**:
```bash
# 完全重新加载模块
sudo modprobe -r nvidia_uvm
sudo modprobe -r nvidia_drm  # 可能需要
sudo modprobe -r nvidia
sudo modprobe nvidia
sudo modprobe nvidia_drm
sudo modprobe nvidia-uvm uvm_enable_builtin_tests=1

# 验证
cat /sys/module/nvidia_uvm/parameters/uvm_enable_builtin_tests
```

### 问题2: ENOTTY错误
**症状**: "Inappropriate ioctl for device"
**原因**: 驱动版本不支持测试接口
**解决方案**:
```bash
# 检查驱动版本
cat /proc/driver/nvidia/version
nvidia-smi

# 可能需要升级或降级NVIDIA驱动
```

### 问题3: 权限问题
**症状**: Permission denied
**解决方案**:
```bash
# 方案A: 以root运行
sudo ./run_uvm_tests.sh

# 方案B: 修改设备权限
sudo chmod 666 /dev/nvidia-uvm
sudo chown $(whoami):$(whoami) /dev/nvidia-uvm
```

### 问题4: 容器环境限制
**症状**: 在Docker/容器中运行失败
**解决方案**:
```bash
# 运行容器时添加特权模式
docker run --privileged --device=/dev/nvidia-uvm ...

# 或在宿主机上运行测试
```

## 🔍 深度诊断工具

如果上述快速修复无效，运行深度诊断：

```bash
# 详细诊断
sudo python3 deep_uvm_diagnosis.py

# 检查内核日志
dmesg | grep -i uvm | tail -20
dmesg | grep -i nvidia | tail -20

# 检查SELinux (如果适用)
getenforce
sudo setenforce 0  # 临时禁用测试

# 检查AppArmor (如果适用)
sudo aa-status
```

## ⚡ 一键修复脚本

```bash
#!/bin/bash
# 一键修复UVM测试问题

echo "UVM测试修复脚本"
echo "=============="

# 1. 重新加载所有NVIDIA模块
echo "1. 重新加载NVIDIA模块..."
sudo modprobe -r nvidia_uvm nvidia_drm nvidia 2>/dev/null || true
sleep 2
sudo modprobe nvidia
sudo modprobe nvidia_drm  
sudo modprobe nvidia-uvm uvm_enable_builtin_tests=1

# 2. 验证模块加载
echo "2. 验证模块状态..."
lsmod | grep nvidia
cat /sys/module/nvidia_uvm/parameters/uvm_enable_builtin_tests

# 3. 修复设备权限
echo "3. 修复设备权限..."
sudo chmod 666 /dev/nvidia-uvm
ls -la /dev/nvidia-uvm

# 4. 运行测试验证
echo "4. 运行验证测试..."
sudo python3 minimal_uvm_test.py

echo "修复完成！现在可以运行 sudo ./run_uvm_tests.sh"
```

## 📞 如果问题仍然存在

请收集以下信息:
1. `sudo python3 deep_uvm_diagnosis.py` 的完整输出
2. `dmesg | grep -i nvidia` 的输出
3. 操作系统版本: `uname -a`
4. NVIDIA驱动版本: `cat /proc/driver/nvidia/version`
5. GPU型号: `nvidia-smi -L`

## ✅ 成功标志

修复成功后，您应该看到:
- `minimal_uvm_test.py` 显示 "✅ ioctl调用成功!"
- 至少一些非GPU测试通过 (RNG_SANITY, LOCK_SANITY等)
- 成功率从0%提升到30-90% (取决于GPU硬件)

记住：**即使在完美配置的系统上，某些GPU相关测试在没有对应硬件时也会失败，这是正常现象。**