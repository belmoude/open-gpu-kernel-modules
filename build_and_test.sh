#!/bin/bash
# build_and_test.sh - UVM测试构建和运行脚本

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_PROGRAM="$SCRIPT_DIR/simple_uvm_test"
UVM_HEADERS="$SCRIPT_DIR/kernel-open/nvidia-uvm"

echo "=== NVIDIA UVM Test Builder and Runner ==="
echo "Script directory: $SCRIPT_DIR"

# 检查是否以root权限运行
check_root() {
    if [ "$EUID" -ne 0 ]; then
        echo "❌ This script must be run as root"
        echo "   Run: sudo $0"
        exit 1
    fi
}

# 编译测试程序
build_test_program() {
    echo ""
    echo "=== Building Test Program ==="
    
    # 优先编译智能测试程序
    if [ -f "$SCRIPT_DIR/smart_uvm_test.c" ]; then
        echo "编译智能UVM测试程序 (smart_uvm_test.c)..."
        TEST_PROGRAM="$SCRIPT_DIR/smart_uvm_test"
        
        if gcc -Wall -o "$TEST_PROGRAM" "$SCRIPT_DIR/smart_uvm_test.c"; then
            echo "✅ 智能测试程序编译成功"
            return 0
        else
            echo "⚠️  智能测试程序编译失败，尝试简单版本..."
        fi
    fi
    
    # 备选：编译简单测试程序
    if [ -f "$SCRIPT_DIR/simple_uvm_test.c" ]; then
        echo "编译简单UVM测试程序 (simple_uvm_test.c)..."
        TEST_PROGRAM="$SCRIPT_DIR/simple_uvm_test"
        
        if gcc -Wall -o "$TEST_PROGRAM" "$SCRIPT_DIR/simple_uvm_test.c"; then
            echo "✅ 简单测试程序编译成功"
            return 0
        else
            echo "❌ 简单测试程序编译也失败"
        fi
    fi
    
    # 最后备选：编译基础IOCTL测试
    if [ -f "$SCRIPT_DIR/test_ioctl_simple.c" ]; then
        echo "编译基础IOCTL测试程序 (test_ioctl_simple.c)..."
        TEST_PROGRAM="$SCRIPT_DIR/test_ioctl_simple"
        
        if gcc -Wall -o "$TEST_PROGRAM" "$SCRIPT_DIR/test_ioctl_simple.c"; then
            echo "✅ 基础测试程序编译成功"
            return 0
        fi
    fi
    
    echo "❌ 所有测试程序编译失败"
    echo "   请检查编译环境或安装build-essential"
    exit 1
}

# 检查和设置UVM环境
setup_uvm_environment() {
    echo ""
    echo "=== Setting Up UVM Environment ==="
    
    # 检查nvidia-uvm模块是否已加载
    if lsmod | grep -q "nvidia_uvm"; then
        echo "✅ nvidia-uvm module is loaded"
        
        # 检查测试是否已启用
        if [ -f "/sys/module/nvidia_uvm/parameters/uvm_enable_builtin_tests" ]; then
            current_value=$(cat /sys/module/nvidia_uvm/parameters/uvm_enable_builtin_tests)
            if [ "$current_value" = "1" ]; then
                echo "✅ UVM builtin tests are already enabled"
                return 0
            else
                echo "⚠️  UVM builtin tests are disabled, enabling..."
            fi
        fi
        
        # 重新加载模块并启用测试
        echo "Reloading UVM module with tests enabled..."
        modprobe -r nvidia-uvm 2>/dev/null || true
        sleep 1
    else
        echo "⚠️  nvidia-uvm module not loaded"
    fi
    
    # 加载UVM模块并启用测试
    echo "Loading nvidia-uvm with tests enabled..."
    if modprobe nvidia-uvm uvm_enable_builtin_tests=1; then
        echo "✅ UVM module loaded with tests enabled"
    else
        echo "❌ Failed to load UVM module"
        echo "   Make sure NVIDIA drivers are properly installed"
        exit 1
    fi
    
    # 验证设备文件
    if [ -c "/dev/nvidia-uvm" ]; then
        echo "✅ UVM device file created"
    else
        echo "❌ UVM device file not found"
        exit 1
    fi
}

# 运行测试
run_tests() {
    echo ""
    echo "=== Running UVM Tests ==="
    
    if [ ! -x "$TEST_PROGRAM" ]; then
        echo "❌ Test program not found or not executable: $TEST_PROGRAM"
        exit 1
    fi
    
    echo "Executing test program..."
    "$TEST_PROGRAM"
}

# 清理环境（可选）
cleanup() {
    echo ""
    echo "=== Cleanup (Optional) ==="
    echo "To disable UVM tests after testing:"
    echo "  sudo modprobe -r nvidia-uvm"
    echo "  sudo modprobe nvidia-uvm"
    echo ""
    echo "To keep tests enabled, do nothing."
}

# 主函数
main() {
    echo "Starting UVM test process..."
    
    check_root
    build_test_program
    setup_uvm_environment
    run_tests
    cleanup
    
    echo ""
    echo "🎉 UVM test process completed!"
}

# 显示帮助信息
show_help() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -h, --help     Show this help message"
    echo "  -b, --build    Only build the test program"
    echo "  -r, --run      Only run tests (assumes program is built)"
    echo "  -s, --setup    Only setup UVM environment"
    echo ""
    echo "Examples:"
    echo "  $0                  # Full process: build, setup, and run"
    echo "  $0 --build          # Only compile the test program"
    echo "  $0 --setup          # Only setup UVM with tests enabled"
    echo "  $0 --run            # Only run tests"
    echo ""
    echo "Requirements:"
    echo "  - Root privileges (sudo)"
    echo "  - NVIDIA drivers installed"
    echo "  - GCC compiler available"
}

# 解析命令行参数
case "${1:-}" in
    -h|--help)
        show_help
        exit 0
        ;;
    -b|--build)
        build_test_program
        exit 0
        ;;
    -s|--setup)
        check_root
        setup_uvm_environment
        exit 0
        ;;
    -r|--run)
        check_root
        run_tests
        exit 0
        ;;
    "")
        main
        ;;
    *)
        echo "Unknown option: $1"
        echo "Use --help for usage information"
        exit 1
        ;;
esac