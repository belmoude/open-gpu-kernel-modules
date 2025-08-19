#!/bin/bash

#******************************************************************************
# UVM调试输出分析工具
# 分析为什么没有内核日志输出
#******************************************************************************

echo "UVM调试输出分析工具"
echo "=================="
echo ""

if [[ $EUID -ne 0 ]]; then
   echo "请以root身份运行: sudo $0"
   exit 1
fi

echo "1. 检查UVM模块编译信息..."
modinfo nvidia_uvm 2>/dev/null | head -20 || echo "无法获取模块信息"

echo ""
echo "2. 检查UVM模块参数..."
echo "可用参数:"
if [[ -d /sys/module/nvidia_uvm/parameters ]]; then
    for param in /sys/module/nvidia_uvm/parameters/*; do
        param_name=$(basename "$param")
        param_value=$(cat "$param" 2>/dev/null || echo "无法读取")
        echo "  $param_name = $param_value"
    done
else
    echo "  参数目录不存在"
fi

echo ""
echo "3. 检查内核配置..."
echo "当前printk级别: $(cat /proc/sys/kernel/printk)"

# 检查内核是否支持动态调试
if [[ -f /sys/kernel/debug/dynamic_debug/control ]]; then
    echo "动态调试: 支持"
    echo "UVM动态调试状态:"
    grep nvidia_uvm /sys/kernel/debug/dynamic_debug/control | head -5 || echo "  无UVM动态调试条目"
else
    echo "动态调试: 不支持"
fi

echo ""
echo "4. 测试内核日志输出..."

# 记录当前dmesg行数
before_lines=$(dmesg | wc -l)
echo "测试前dmesg行数: $before_lines"

# 执行一个简单的UVM操作
echo "执行UVM测试操作..."
python3 << 'EOF'
import os
import fcntl
import array

try:
    fd = os.open('/dev/nvidia-uvm', os.O_RDWR)
    try:
        params = array.array('B', [0] * 1024)
        result = fcntl.ioctl(fd, 201, params)  # RNG_SANITY
        print(f"UVM测试执行完成，返回值: {result}")
    finally:
        os.close(fd)
except Exception as e:
    print(f"UVM测试失败: {e}")
EOF

sleep 1

# 检查是否有新的dmesg输出
after_lines=$(dmesg | wc -l)
echo "测试后dmesg行数: $after_lines"
echo "新增行数: $((after_lines - before_lines))"

if [[ $((after_lines - before_lines)) -gt 0 ]]; then
    echo ""
    echo "新的内核消息:"
    dmesg | tail -$((after_lines - before_lines))
else
    echo "❌ 没有新的内核消息产生"
fi

echo ""
echo "5. 分析可能的原因..."

# 检查UVM源码中的打印语句
echo "UVM模块可能的调试输出情况:"
echo "1. 编译时禁用了调试打印"
echo "2. 需要特定的内核配置选项"
echo "3. 使用了条件编译的调试代码"
echo "4. 调试输出被重定向到其他地方"

echo ""
echo "6. 检查其他可能的输出位置..."

# 检查trace系统
if [[ -d /sys/kernel/debug/tracing ]]; then
    echo "Trace系统可用，检查UVM trace..."
    if [[ -f /sys/kernel/debug/tracing/available_events ]]; then
        grep -i uvm /sys/kernel/debug/tracing/available_events || echo "  无UVM trace事件"
    fi
fi

# 检查perf事件
if command -v perf >/dev/null 2>&1; then
    echo "检查perf事件..."
    perf list | grep -i uvm || echo "  无UVM perf事件"
fi

echo ""
echo "7. 建议的解决方案..."
echo "==================="
echo ""
echo "如果UVM模块没有内置调试输出，可以考虑:"
echo ""
echo "A. 使用用户态日志记录:"
echo "   修改测试脚本，添加详细的用户态日志"
echo ""
echo "B. 使用strace跟踪系统调用:"
echo "   strace -e ioctl ./run_uvm_tests_final.sh --test RNG_SANITY"
echo ""
echo "C. 使用ftrace跟踪内核函数:"
echo "   echo uvm_* > /sys/kernel/debug/tracing/set_ftrace_filter"
echo "   echo function > /sys/kernel/debug/tracing/current_tracer"
echo ""
echo "D. 检查NVIDIA驱动是否有专门的调试版本"
echo ""
echo "E. 查看UVM源码中的UVM_TEST_PRINT宏使用情况"