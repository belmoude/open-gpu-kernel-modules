#!/bin/bash

#******************************************************************************
# UVM内核调试输出启用脚本
# 用于开启NVIDIA UVM模块的内核测试程序打印
#******************************************************************************

echo "UVM内核调试输出启用工具"
echo "======================="
echo ""

# 检查是否以root身份运行
if [[ $EUID -ne 0 ]]; then
   echo "错误: 此脚本需要root权限运行"
   echo "请使用: sudo $0"
   exit 1
fi

echo "1. 检查UVM模块状态..."
if ! lsmod | grep -q nvidia_uvm; then
    echo "❌ nvidia_uvm模块未加载"
    echo "请先加载模块: modprobe nvidia-uvm uvm_enable_builtin_tests=1"
    exit 1
else
    echo "✅ nvidia_uvm模块已加载"
fi

echo ""
echo "2. 启用内核消息打印..."

# 方法1: 调整内核日志级别
echo "设置内核日志级别..."
echo 8 > /proc/sys/kernel/printk
echo "✅ 内核日志级别设置为最高 (8)"

# 方法2: 启用UVM调试输出 (如果支持)
echo ""
echo "3. 尝试启用UVM调试输出..."

# 检查UVM模块参数
UVM_DEBUG_PATH="/sys/module/nvidia_uvm/parameters"
if [[ -d "$UVM_DEBUG_PATH" ]]; then
    echo "找到UVM模块参数目录:"
    ls -la "$UVM_DEBUG_PATH/"
    
    # 尝试启用各种调试选项
    for param in uvm_debug_prints uvm_enable_debug uvm_debug_level; do
        if [[ -f "$UVM_DEBUG_PATH/$param" ]]; then
            echo "尝试启用 $param..."
            echo 1 > "$UVM_DEBUG_PATH/$param" 2>/dev/null && echo "✅ $param 已启用" || echo "⚠️ $param 无法设置"
        fi
    done
else
    echo "⚠️ 未找到UVM调试参数目录"
fi

# 方法3: 启用动态调试 (如果内核支持)
echo ""
echo "4. 启用动态调试..."
if [[ -f /sys/kernel/debug/dynamic_debug/control ]]; then
    echo "启用UVM模块的动态调试..."
    echo 'module nvidia_uvm +p' > /sys/kernel/debug/dynamic_debug/control 2>/dev/null && \
        echo "✅ UVM动态调试已启用" || echo "⚠️ 动态调试设置失败"
else
    echo "⚠️ 动态调试不可用 (需要CONFIG_DYNAMIC_DEBUG)"
fi

# 方法4: 调整dmesg缓冲区大小
echo ""
echo "5. 优化dmesg缓冲区..."
# 检查当前缓冲区大小
current_size=$(dmesg -s 2>/dev/null | wc -l)
echo "当前dmesg缓冲区行数: $current_size"

echo ""
echo "6. 实时监控设置..."
echo "现在可以使用以下命令实时监控UVM内核输出:"
echo ""
echo "# 实时查看所有内核消息"
echo "dmesg -w"
echo ""
echo "# 只查看UVM相关消息"
echo "dmesg -w | grep -i uvm"
echo ""
echo "# 查看测试相关消息"
echo "dmesg -w | grep -i 'test\\|uvm'"
echo ""
echo "# 在另一个终端运行UVM测试"
echo "./run_uvm_tests_final.sh --test RNG_SANITY --verbose"

echo ""
echo "7. 高级调试选项..."

# 检查是否有UVM特定的调试接口
UVM_DEBUGFS="/sys/kernel/debug/nvidia_uvm"
if [[ -d "$UVM_DEBUGFS" ]]; then
    echo "找到UVM debugfs接口:"
    find "$UVM_DEBUGFS" -type f 2>/dev/null | head -10
else
    echo "⚠️ 未找到UVM debugfs接口"
fi

# 检查proc接口
UVM_PROC="/proc/driver/nvidia/uvm"
if [[ -f "$UVM_PROC" ]]; then
    echo "找到UVM proc接口: $UVM_PROC"
else
    echo "⚠️ 未找到UVM proc接口"
fi

echo ""
echo "8. 测试内核输出..."
echo "运行一个简单测试来验证输出..."

# 运行一个简单的UVM测试并检查内核输出
if command -v python3 >/dev/null 2>&1; then
    echo "执行简单的UVM ioctl调用..."
    
    cat > /tmp/test_uvm_kernel_output.py << 'EOF'
#!/usr/bin/env python3
import os
import sys
import fcntl
import array

try:
    print("测试UVM内核输出...")
    fd = os.open('/dev/nvidia-uvm', os.O_RDWR)
    try:
        # 执行RNG_SANITY测试 (命令ID: 201)
        params = array.array('B', [0] * 1024)
        result = fcntl.ioctl(fd, 201, params)
        print(f"UVM测试完成，返回值: {result}")
    finally:
        os.close(fd)
except Exception as e:
    print(f"测试失败: {e}")
EOF
    
    python3 /tmp/test_uvm_kernel_output.py
    rm -f /tmp/test_uvm_kernel_output.py
    
    echo ""
    echo "检查最近的内核消息..."
    dmesg | tail -10 | grep -i uvm || echo "未发现UVM相关的新内核消息"
fi

echo ""
echo "9. 配置完成!"
echo "============"
echo ""
echo "内核调试输出已启用。现在运行UVM测试时，相关的内核消息将出现在:"
echo "- dmesg 输出中"
echo "- /var/log/kern.log (如果存在)"
echo "- /var/log/messages (如果存在)"
echo ""
echo "推荐的监控命令:"
echo "# 在一个终端中运行:"
echo "dmesg -w | grep -i uvm"
echo ""
echo "# 在另一个终端中运行测试:"
echo "./run_uvm_tests_final.sh --continue --verbose"
echo ""
echo "注意: 某些UVM测试输出可能需要特定的编译选项才能显示。"