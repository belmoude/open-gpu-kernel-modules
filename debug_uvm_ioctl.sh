#!/bin/bash
# debug_uvm_ioctl.sh - 调试UVM IOCTL问题

set -e

echo "=== UVM IOCTL调试工具 ==="

# 检查是否以root权限运行
if [ "$EUID" -ne 0 ]; then
    echo "❌ 此脚本必须以root权限运行"
    exit 1
fi

# 编译简单测试程序
echo "编译简单测试程序..."
gcc -o test_ioctl_simple test_ioctl_simple.c
if [ $? -eq 0 ]; then
    echo "✅ 编译成功"
else
    echo "❌ 编译失败"
    exit 1
fi

# 检查UVM模块状态
echo ""
echo "=== UVM模块状态 ==="
echo "模块加载状态:"
lsmod | grep nvidia_uvm

echo ""
echo "模块参数:"
if [ -f "/sys/module/nvidia_uvm/parameters/uvm_enable_builtin_tests" ]; then
    echo "uvm_enable_builtin_tests = $(cat /sys/module/nvidia_uvm/parameters/uvm_enable_builtin_tests)"
else
    echo "❌ 无法读取模块参数"
fi

# 检查设备文件
echo ""
echo "=== 设备文件状态 ==="
if [ -c "/dev/nvidia-uvm" ]; then
    echo "✅ /dev/nvidia-uvm 存在"
    ls -la /dev/nvidia-uvm
    
    # 检查设备的主从设备号
    major=$(stat -c "%t" /dev/nvidia-uvm)
    minor=$(stat -c "%T" /dev/nvidia-uvm)
    echo "主设备号: 0x$major ($(printf "%d" 0x$major))"
    echo "从设备号: 0x$minor ($(printf "%d" 0x$minor))"
else
    echo "❌ /dev/nvidia-uvm 不存在"
    exit 1
fi

# 检查/proc/devices
echo ""
echo "=== /proc/devices中的nvidia设备 ==="
grep nvidia /proc/devices || echo "没有找到nvidia设备"

# 运行简单测试
echo ""
echo "=== 运行简单IOCTL测试 ==="
./test_ioctl_simple

# 检查内核日志
echo ""
echo "=== 最近的UVM内核日志 ==="
echo "最近10条UVM相关日志:"
dmesg | grep -i uvm | tail -10 || echo "没有找到UVM相关日志"

echo ""
echo "最近10条nvidia相关日志:"
dmesg | grep -i nvidia | tail -10 || echo "没有找到nvidia相关日志"

# 显示调试建议
echo ""
echo "=== 调试建议 ==="
echo "如果IOCTL仍然失败，可能的原因:"
echo "1. IOCTL命令号不正确"
echo "2. 参数结构与内核期望不匹配"
echo "3. UVM测试功能实际未启用"
echo "4. 需要特定的VA space上下文"
echo ""
echo "下一步调试:"
echo "1. 检查内核日志中的错误信息"
echo "2. 尝试不同的IOCTL命令号"
echo "3. 检查是否需要先创建VA space"