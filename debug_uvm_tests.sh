#!/bin/bash

#******************************************************************************
# UVM测试程序 - 调试版本
# 用于诊断为什么测试程序直接退出的问题
#******************************************************************************

set -x  # 启用调试输出

echo "=== UVM测试程序调试版本 ==="

# 检查UVM设备
echo "1. 检查UVM设备文件..."
if [[ -e "/dev/nvidia-uvm" ]]; then
    echo "✓ /dev/nvidia-uvm 存在"
    ls -la /dev/nvidia-uvm
    
    # 检查设备类型
    if [[ -c "/dev/nvidia-uvm" ]]; then
        echo "✓ 是字符设备文件"
    else
        echo "✗ 不是字符设备文件"
    fi
    
    # 检查权限
    if [[ -r "/dev/nvidia-uvm" && -w "/dev/nvidia-uvm" ]]; then
        echo "✓ 有读写权限"
    else
        echo "⚠ 缺少读写权限"
        echo "当前用户: $(whoami)"
        echo "设备权限: $(ls -la /dev/nvidia-uvm)"
    fi
else
    echo "✗ /dev/nvidia-uvm 不存在"
fi

echo ""
echo "2. 检查Python..."
if command -v python3 >/dev/null 2>&1; then
    echo "✓ Python3 可用"
    python3 --version
else
    echo "✗ Python3 不可用"
fi

echo ""
echo "3. 尝试简单的测试执行..."

# 如果设备存在，尝试简单的ioctl调用
if [[ -c "/dev/nvidia-uvm" ]]; then
    echo "尝试打开UVM设备..."
    
    cat > /tmp/test_uvm.py << 'EOF'
#!/usr/bin/env python3
import os
import sys
import fcntl

try:
    print("尝试打开 /dev/nvidia-uvm...")
    with open('/dev/nvidia-uvm', 'rb+') as f:
        print("✓ 成功打开设备")
        
        # 尝试一个简单的ioctl调用
        print("尝试ioctl调用...")
        params = bytearray(1024)
        
        # 使用一个简单的测试命令 (GET_USER_SPACE_END_ADDRESS = 290)
        result = fcntl.ioctl(f, 290, params)
        print("✓ ioctl调用成功")
        
except PermissionError:
    print("✗ 权限不足 - 需要root权限或适当的设备权限")
    sys.exit(1)
except FileNotFoundError:
    print("✗ 设备文件不存在")
    sys.exit(1)
except Exception as e:
    print(f"✗ 其他错误: {e}")
    sys.exit(1)

print("✓ 基本UVM设备访问测试通过")
EOF

    python3 /tmp/test_uvm.py
    TEST_RESULT=$?
    
    if [[ $TEST_RESULT -eq 0 ]]; then
        echo "✓ 基本UVM访问测试成功"
        echo ""
        echo "现在尝试运行实际的测试程序..."
        
        # 运行原始测试程序的一个简单版本
        echo "运行测试: RNG_SANITY"
        cat > /tmp/run_single_test.py << 'EOF'
#!/usr/bin/env python3
import os
import sys
import fcntl

try:
    with open('/dev/nvidia-uvm', 'rb+') as f:
        params = bytearray(1024)
        # RNG_SANITY test command = 201
        result = fcntl.ioctl(f, 201, params)
        print("RNG_SANITY: [通过]")
except Exception as e:
    print(f"RNG_SANITY: [失败] - {e}")
EOF
        
        python3 /tmp/run_single_test.py
        
    else
        echo "✗ 基本UVM访问测试失败"
        echo "可能的原因："
        echo "- 需要root权限"
        echo "- UVM模块未正确加载"
        echo "- 测试功能未启用"
    fi
    
    # 清理临时文件
    rm -f /tmp/test_uvm.py /tmp/run_single_test.py
    
else
    echo "跳过设备测试 - 设备不存在"
fi

echo ""
echo "4. 检查内核模块..."
if [[ -f /proc/modules ]]; then
    if grep -q nvidia_uvm /proc/modules; then
        echo "✓ nvidia-uvm 模块已加载"
        grep nvidia_uvm /proc/modules
    else
        echo "✗ nvidia-uvm 模块未加载"
    fi
else
    echo "⚠ 无法检查内核模块状态"
fi

echo ""
echo "=== 调试总结 ==="
echo "如果设备存在但测试失败，请尝试："
echo "1. 以root身份运行: sudo ./run_uvm_tests.sh"
echo "2. 加载模块并启用测试: sudo modprobe nvidia-uvm uvm_enable_builtin_tests=1"
echo "3. 检查设备权限: sudo chmod 666 /dev/nvidia-uvm"