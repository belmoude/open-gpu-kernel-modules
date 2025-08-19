#!/bin/bash

#******************************************************************************
# UVM测试失败诊断脚本
# 用于诊断为什么所有UVM测试都失败的问题
#******************************************************************************

echo "UVM测试失败诊断工具"
echo "==================="
echo ""

echo "1. 检查UVM模块状态..."
if lsmod 2>/dev/null | grep -q nvidia_uvm; then
    echo "✓ nvidia-uvm模块已加载"
    lsmod | grep nvidia
else
    echo "✗ nvidia-uvm模块未加载"
    echo "请运行: sudo modprobe nvidia-uvm uvm_enable_builtin_tests=1"
fi

echo ""
echo "2. 检查UVM模块参数..."
if [[ -f /sys/module/nvidia_uvm/parameters/uvm_enable_builtin_tests ]]; then
    TESTS_ENABLED=$(cat /sys/module/nvidia_uvm/parameters/uvm_enable_builtin_tests)
    if [[ "$TESTS_ENABLED" == "1" ]]; then
        echo "✓ UVM内置测试已启用"
    else
        echo "✗ UVM内置测试未启用 (当前值: $TESTS_ENABLED)"
        echo "请重新加载模块: sudo modprobe -r nvidia_uvm && sudo modprobe nvidia-uvm uvm_enable_builtin_tests=1"
    fi
else
    echo "⚠ 无法检查UVM测试参数"
fi

echo ""
echo "3. 检查UVM设备状态..."
if [[ -c /dev/nvidia-uvm ]]; then
    echo "✓ UVM设备文件存在"
    ls -la /dev/nvidia-uvm
    
    # 检查设备是否可以打开
    echo ""
    echo "4. 测试UVM设备访问..."
    cat > /tmp/test_uvm_access.py << 'EOF'
#!/usr/bin/env python3
import os
import sys
import fcntl
import errno

def test_uvm_device():
    try:
        print("尝试打开UVM设备...")
        with open('/dev/nvidia-uvm', 'rb+') as f:
            print("✓ 成功打开UVM设备")
            
            # 尝试一个简单的ioctl调用
            print("测试基本ioctl调用...")
            params = bytearray(1024)
            
            # 尝试GET_USER_SPACE_END_ADDRESS (应该总是成功)
            try:
                result = fcntl.ioctl(f, 290, params)  # UVM_TEST_GET_USER_SPACE_END_ADDRESS
                print("✓ 基本ioctl调用成功")
                return True
            except OSError as e:
                if e.errno == errno.EINVAL:
                    print("✗ ioctl返回EINVAL - 可能是测试未启用")
                    print("  错误码:", e.errno)
                    print("  错误信息:", e.strerror)
                    return False
                else:
                    print(f"✗ ioctl失败: {e}")
                    return False
                    
    except PermissionError:
        print("✗ 权限不足")
        print("请以root身份运行或修改设备权限")
        return False
    except Exception as e:
        print(f"✗ 其他错误: {e}")
        return False

if __name__ == "__main__":
    success = test_uvm_device()
    sys.exit(0 if success else 1)
EOF

    python3 /tmp/test_uvm_access.py
    TEST_RESULT=$?
    rm -f /tmp/test_uvm_access.py
    
    if [[ $TEST_RESULT -ne 0 ]]; then
        echo ""
        echo "5. 尝试重新配置UVM模块..."
        echo "建议执行以下命令:"
        echo ""
        echo "sudo modprobe -r nvidia_uvm"
        echo "sudo modprobe nvidia-uvm uvm_enable_builtin_tests=1"
        echo ""
        echo "然后重新运行测试"
    fi
else
    echo "✗ UVM设备文件不存在"
fi

echo ""
echo "6. 检查NVIDIA驱动状态..."
if command -v nvidia-smi >/dev/null 2>&1; then
    echo "✓ nvidia-smi可用"
    nvidia-smi -L 2>/dev/null || echo "⚠ nvidia-smi执行失败"
else
    echo "⚠ nvidia-smi不可用"
fi

echo ""
echo "7. 检查内核日志中的UVM相关信息..."
if command -v dmesg >/dev/null 2>&1; then
    echo "最近的UVM相关内核消息:"
    dmesg | grep -i uvm | tail -10 || echo "无UVM相关消息"
else
    echo "⚠ 无法访问内核日志"
fi

echo ""
echo "=== 诊断总结 ==="
echo ""
echo "基于测试结果(所有97个测试都失败)，最可能的原因是:"
echo ""
echo "1. UVM模块测试功能未启用"
echo "   解决方案: sudo modprobe -r nvidia_uvm && sudo modprobe nvidia-uvm uvm_enable_builtin_tests=1"
echo ""
echo "2. 权限问题"
echo "   解决方案: sudo ./run_uvm_tests.sh"
echo ""
echo "3. 驱动版本不兼容"
echo "   解决方案: 检查NVIDIA驱动版本是否支持UVM测试"
echo ""
echo "建议按以下顺序尝试修复:"
echo "Step 1: sudo modprobe -r nvidia_uvm"
echo "Step 2: sudo modprobe nvidia-uvm uvm_enable_builtin_tests=1"
echo "Step 3: sudo ./run_uvm_tests.sh --test RNG_SANITY --verbose"
echo "Step 4: 如果单个测试通过，再运行完整测试套件"