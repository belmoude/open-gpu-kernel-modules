#!/bin/bash
# fix_uvm_device.sh - 修复UVM设备文件问题

set -e

echo "=== NVIDIA UVM设备文件修复工具 ==="

# 检查是否以root权限运行
if [ "$EUID" -ne 0 ]; then
    echo "❌ 此脚本必须以root权限运行"
    echo "   运行: sudo $0"
    exit 1
fi

# 检查nvidia-uvm模块状态
check_uvm_module() {
    echo ""
    echo "=== 检查UVM模块状态 ==="
    
    if lsmod | grep -q "nvidia_uvm"; then
        echo "✅ nvidia-uvm模块已加载"
        
        # 显示模块信息
        echo "模块信息:"
        lsmod | grep nvidia_uvm
        
        # 检查模块参数
        if [ -d "/sys/module/nvidia_uvm" ]; then
            echo "✅ 模块参数目录存在"
            if [ -f "/sys/module/nvidia_uvm/parameters/uvm_enable_builtin_tests" ]; then
                test_enabled=$(cat /sys/module/nvidia_uvm/parameters/uvm_enable_builtin_tests)
                echo "当前测试状态: $test_enabled"
            fi
        fi
    else
        echo "❌ nvidia-uvm模块未加载"
        return 1
    fi
}

# 检查设备文件
check_device_files() {
    echo ""
    echo "=== 检查NVIDIA设备文件 ==="
    
    echo "当前NVIDIA设备文件:"
    ls -la /dev/nvidia* 2>/dev/null || echo "  没有找到NVIDIA设备文件"
    
    echo ""
    echo "检查设备节点:"
    if [ -c "/dev/nvidia-uvm" ]; then
        echo "✅ /dev/nvidia-uvm 存在"
        ls -la /dev/nvidia-uvm
    else
        echo "❌ /dev/nvidia-uvm 不存在"
    fi
    
    if [ -c "/dev/nvidiactl" ]; then
        echo "✅ /dev/nvidiactl 存在"
    else
        echo "❌ /dev/nvidiactl 不存在"
    fi
}

# 查找UVM设备的主设备号
find_uvm_major_number() {
    echo ""
    echo "=== 查找UVM设备主设备号 ==="
    
    # 从/proc/devices查找
    if grep -q "nvidia-uvm" /proc/devices; then
        major=$(grep "nvidia-uvm" /proc/devices | awk '{print $1}')
        echo "✅ 找到UVM主设备号: $major"
        echo "$major"
        return 0
    fi
    
    # 从内核日志查找
    uvm_major=$(dmesg | grep -i "uvm.*major" | tail -1 | grep -o "major device number [0-9]*" | grep -o "[0-9]*" || echo "")
    if [ -n "$uvm_major" ]; then
        echo "✅ 从dmesg找到UVM主设备号: $uvm_major"
        echo "$uvm_major"
        return 0
    fi
    
    echo "❌ 无法找到UVM主设备号"
    return 1
}

# 手动创建UVM设备文件
create_uvm_device() {
    echo ""
    echo "=== 创建UVM设备文件 ==="
    
    major=$(find_uvm_major_number)
    if [ $? -ne 0 ]; then
        echo "❌ 无法获取主设备号，无法创建设备文件"
        return 1
    fi
    
    echo "使用主设备号 $major 创建设备文件..."
    
    # 删除可能存在的旧设备文件
    rm -f /dev/nvidia-uvm
    
    # 创建新的设备文件
    mknod /dev/nvidia-uvm c "$major" 0
    
    # 设置权限
    chmod 666 /dev/nvidia-uvm
    
    if [ -c "/dev/nvidia-uvm" ]; then
        echo "✅ UVM设备文件创建成功"
        ls -la /dev/nvidia-uvm
        return 0
    else
        echo "❌ UVM设备文件创建失败"
        return 1
    fi
}

# 重新加载UVM模块并启用测试
reload_uvm_with_tests() {
    echo ""
    echo "=== 重新加载UVM模块并启用测试 ==="
    
    echo "卸载现有的UVM模块..."
    modprobe -r nvidia-uvm 2>/dev/null || true
    
    sleep 2
    
    echo "重新加载UVM模块并启用测试..."
    if modprobe nvidia-uvm uvm_enable_builtin_tests=1; then
        echo "✅ UVM模块重新加载成功"
        
        # 等待设备文件创建
        sleep 2
        
        if [ -c "/dev/nvidia-uvm" ]; then
            echo "✅ UVM设备文件自动创建"
        else
            echo "⚠️  设备文件未自动创建，尝试手动创建..."
            create_uvm_device
        fi
    else
        echo "❌ UVM模块加载失败"
        return 1
    fi
}

# 验证修复结果
verify_fix() {
    echo ""
    echo "=== 验证修复结果 ==="
    
    # 检查模块
    if lsmod | grep -q "nvidia_uvm"; then
        echo "✅ UVM模块已加载"
    else
        echo "❌ UVM模块未加载"
        return 1
    fi
    
    # 检查设备文件
    if [ -c "/dev/nvidia-uvm" ]; then
        echo "✅ UVM设备文件存在"
        ls -la /dev/nvidia-uvm
    else
        echo "❌ UVM设备文件仍然不存在"
        return 1
    fi
    
    # 检查测试启用状态
    if [ -f "/sys/module/nvidia_uvm/parameters/uvm_enable_builtin_tests" ]; then
        test_status=$(cat /sys/module/nvidia_uvm/parameters/uvm_enable_builtin_tests)
        if [ "$test_status" = "1" ]; then
            echo "✅ UVM测试已启用"
        else
            echo "⚠️  UVM测试未启用 (当前值: $test_status)"
        fi
    fi
    
    echo ""
    echo "🎉 UVM环境修复完成！现在可以运行测试了："
    echo "   sudo python3 uvm_test_suite.py"
    echo "   或"
    echo "   sudo ./build_and_test.sh"
}

# 显示详细的诊断信息
show_diagnostic_info() {
    echo ""
    echo "=== 详细诊断信息 ==="
    
    echo "1. 内核版本:"
    uname -r
    
    echo ""
    echo "2. NVIDIA驱动版本:"
    if command -v nvidia-smi >/dev/null 2>&1; then
        nvidia-smi --query-gpu=driver_version --format=csv,noheader,nounits | head -1
    else
        echo "   nvidia-smi 不可用"
    fi
    
    echo ""
    echo "3. 加载的NVIDIA模块:"
    lsmod | grep nvidia
    
    echo ""
    echo "4. /proc/devices中的nvidia设备:"
    grep nvidia /proc/devices || echo "   没有找到nvidia设备"
    
    echo ""
    echo "5. dmesg中的UVM相关信息:"
    dmesg | grep -i uvm | tail -5 || echo "   没有找到UVM相关信息"
    
    echo ""
    echo "6. /dev目录中的nvidia设备:"
    ls -la /dev/nvidia* 2>/dev/null || echo "   没有找到nvidia设备文件"
}

# 主修复流程
main() {
    echo "开始诊断和修复UVM设备问题..."
    
    check_uvm_module
    check_device_files
    
    if [ ! -c "/dev/nvidia-uvm" ]; then
        echo ""
        echo "🔧 检测到UVM设备文件缺失，开始修复..."
        
        # 方法1: 尝试重新加载模块
        reload_uvm_with_tests
        
        # 方法2: 如果还是没有，手动创建
        if [ ! -c "/dev/nvidia-uvm" ]; then
            echo "⚠️  自动创建失败，尝试手动创建设备文件..."
            create_uvm_device
        fi
    fi
    
    verify_fix
}

# 解析命令行参数
case "${1:-}" in
    -h|--help)
        echo "用法: $0 [选项]"
        echo ""
        echo "选项:"
        echo "  -h, --help      显示帮助信息"
        echo "  -d, --diag      只显示诊断信息"
        echo "  -c, --create    只创建设备文件"
        echo "  -r, --reload    只重新加载模块"
        echo ""
        echo "默认行为: 执行完整的诊断和修复流程"
        exit 0
        ;;
    -d|--diag)
        show_diagnostic_info
        exit 0
        ;;
    -c|--create)
        create_uvm_device
        exit 0
        ;;
    -r|--reload)
        reload_uvm_with_tests
        exit 0
        ;;
    "")
        main
        ;;
    *)
        echo "未知选项: $1"
        echo "使用 --help 查看帮助信息"
        exit 1
        ;;
esac