#!/bin/bash

#******************************************************************************
# 正确的UVM内核调试输出启用方法
# 基于UVM源码分析的正确启用方式
#******************************************************************************

echo "UVM内核调试输出正确启用方法"
echo "=========================="
echo ""

if [[ $EUID -ne 0 ]]; then
   echo "错误: 需要root权限"
   echo "请使用: sudo $0"
   exit 1
fi

echo "🔍 根据UVM源码分析，找到了正确的调试启用方法！"
echo ""

echo "1. 检查UVM调试参数..."
UVM_DEBUG_PARAM="/sys/module/nvidia_uvm/parameters/uvm_debug_prints"

if [[ -f "$UVM_DEBUG_PARAM" ]]; then
    current_value=$(cat "$UVM_DEBUG_PARAM")
    echo "✅ 找到UVM调试参数"
    echo "   当前值: $current_value"
    
    if [[ "$current_value" != "1" ]]; then
        echo "   正在启用调试打印..."
        echo 1 > "$UVM_DEBUG_PARAM"
        new_value=$(cat "$UVM_DEBUG_PARAM")
        echo "   新值: $new_value"
        
        if [[ "$new_value" == "1" ]]; then
            echo "✅ UVM调试打印已成功启用！"
        else
            echo "❌ 启用失败"
        fi
    else
        echo "✅ UVM调试打印已经启用"
    fi
else
    echo "❌ 未找到UVM调试参数"
    echo "   可能需要重新加载模块:"
    echo "   modprobe -r nvidia_uvm"
    echo "   modprobe nvidia-uvm uvm_enable_builtin_tests=1 uvm_debug_prints=1"
fi

echo ""
echo "2. 设置内核日志级别..."
echo 8 > /proc/sys/kernel/printk
echo "✅ 内核日志级别已设置为最高"

echo ""
echo "3. 检查UVM测试打印机制..."
echo "根据源码分析:"
echo "- UVM_TEST_PRINT 使用 pr_info 输出"
echo "- UVM_ERR_PRINT, UVM_DBG_PRINT, UVM_INFO_PRINT 需要 uvm_debug_prints=1"
echo "- 测试失败时会输出详细错误信息"

echo ""
echo "4. 重新加载模块以确保所有参数生效..."
read -p "是否重新加载UVM模块? (y/N): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "重新加载nvidia_uvm模块..."
    
    # 卸载模块
    modprobe -r nvidia_uvm 2>/dev/null || echo "模块卸载可能失败（正常）"
    
    # 重新加载模块，同时启用测试和调试
    modprobe nvidia-uvm uvm_enable_builtin_tests=1 uvm_debug_prints=1
    
    echo "✅ 模块重新加载完成"
    
    # 验证参数
    echo "验证参数设置:"
    echo "  测试启用: $(cat /sys/module/nvidia_uvm/parameters/uvm_enable_builtin_tests)"
    echo "  调试打印: $(cat /sys/module/nvidia_uvm/parameters/uvm_debug_prints)"
fi

echo ""
echo "5. 测试内核输出..."
echo "现在运行一个测试来验证内核输出..."

# 记录测试前的dmesg行数
before_lines=$(dmesg | wc -l)

# 执行一个简单的测试
echo "执行RNG_SANITY测试..."
python3 << 'EOF'
import os
import fcntl
import array

try:
    fd = os.open('/dev/nvidia-uvm', os.O_RDWR)
    try:
        params = array.array('B', [0] * 1024)
        result = fcntl.ioctl(fd, 201, params)  # RNG_SANITY
        print(f"✅ 测试完成，返回值: {result}")
    finally:
        os.close(fd)
except Exception as e:
    print(f"❌ 测试失败: {e}")
EOF

# 等待一下让内核消息出现
sleep 1

# 检查新的内核消息
after_lines=$(dmesg | wc -l)
new_lines=$((after_lines - before_lines))

echo ""
echo "6. 检查内核输出结果..."
echo "新增内核消息行数: $new_lines"

if [[ $new_lines -gt 0 ]]; then
    echo "✅ 发现新的内核消息:"
    echo "最近的内核消息:"
    dmesg | tail -$new_lines
else
    echo "❌ 没有新的内核消息"
    echo ""
    echo "可能的原因:"
    echo "1. UVM测试可能不会产生内核打印（某些测试是静默的）"
    echo "2. 需要测试失败才会产生错误输出"
    echo "3. 需要特定的编译选项"
    echo ""
    echo "尝试运行一个会失败的测试:"
    echo "python3 << 'EOF'"
    echo "import os, fcntl, array"
    echo "fd = os.open('/dev/nvidia-uvm', os.O_RDWR)"
    echo "params = array.array('B', [0] * 1024)"
    echo "fcntl.ioctl(fd, 999, params)  # 无效命令，应该会产生错误输出"
    echo "os.close(fd)"
    echo "EOF"
fi

echo ""
echo "7. 实时监控设置完成!"
echo "==================="
echo ""
echo "现在可以实时监控UVM内核输出:"
echo ""
echo "# 在终端1中运行（监控）:"
echo "dmesg -w | grep -E '(nvidia_uvm|UVM)' --line-buffered"
echo ""
echo "# 在终端2中运行（测试）:"
echo "./run_uvm_tests_final.sh --test RNG_SANITY --verbose"
echo ""
echo "# 或者运行所有测试:"
echo "./run_uvm_tests_final.sh --continue --verbose"
echo ""
echo "如果仍然没有输出，说明UVM测试程序可能设计为静默执行，"
echo "只在出现错误时才输出信息。"