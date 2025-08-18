#!/usr/bin/env python3
"""
深度UVM诊断工具
用于详细分析UVM测试失败的根本原因
"""

import os
import sys
import fcntl
import errno
import struct
import subprocess

def run_command(cmd):
    """安全地运行系统命令"""
    try:
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
        return result.returncode, result.stdout, result.stderr
    except Exception as e:
        return -1, "", str(e)

def check_uvm_device_details():
    """详细检查UVM设备状态"""
    print("=== UVM设备详细检查 ===")
    
    device_path = "/dev/nvidia-uvm"
    
    if not os.path.exists(device_path):
        print(f"✗ 设备文件 {device_path} 不存在")
        return False
    
    # 获取设备统计信息
    stat = os.stat(device_path)
    print(f"✓ 设备文件存在: {device_path}")
    print(f"  设备类型: {'字符设备' if os.path.isfile(device_path) and stat.st_mode & 0o060000 else '其他'}")
    print(f"  主设备号: {os.major(stat.st_rdev)}")
    print(f"  次设备号: {os.minor(stat.st_rdev)}")
    print(f"  权限: {oct(stat.st_mode)[-3:]}")
    print(f"  所有者: UID {stat.st_uid}, GID {stat.st_gid}")
    
    # 检查当前用户权限
    can_read = os.access(device_path, os.R_OK)
    can_write = os.access(device_path, os.W_OK)
    print(f"  当前用户权限: 读取={'✓' if can_read else '✗'}, 写入={'✓' if can_write else '✗'}")
    
    return can_read and can_write

def test_basic_device_open():
    """测试基本设备打开操作"""
    print("\n=== 基本设备访问测试 ===")
    
    try:
        with open("/dev/nvidia-uvm", "rb+") as f:
            print("✓ 成功打开设备文件")
            
            # 获取文件描述符
            fd = f.fileno()
            print(f"  文件描述符: {fd}")
            
            return True
            
    except PermissionError as e:
        print(f"✗ 权限错误: {e}")
        print("  建议: 以root身份运行或修改设备权限")
        return False
    except Exception as e:
        print(f"✗ 其他错误: {e}")
        return False

def test_ioctl_calls():
    """测试各种ioctl调用"""
    print("\n=== ioctl调用详细测试 ===")
    
    # 测试不同类型的ioctl命令
    test_commands = [
        # (命令ID, 命令名称, 预期结果, 描述)
        (290, "GET_USER_SPACE_END_ADDRESS", "should_pass", "获取用户空间结束地址 - 应该总是成功"),
        (296, "CGROUP_ACCOUNTING_SUPPORTED", "may_fail", "检查CGroup支持 - 可能失败"),
        (201, "RNG_SANITY", "should_pass", "随机数生成器测试 - 基本功能"),
        (218, "LOCK_SANITY", "should_pass", "锁测试 - 基本功能"),
        (200, "GET_GPU_REF_COUNT", "may_fail", "GPU引用计数 - 需要GPU"),
    ]
    
    success_count = 0
    total_count = len(test_commands)
    
    try:
        with open("/dev/nvidia-uvm", "rb+") as f:
            for cmd_id, cmd_name, expected, description in test_commands:
                print(f"\n测试: {cmd_name} (ID: {cmd_id})")
                print(f"  描述: {description}")
                
                try:
                    # 创建参数缓冲区
                    params = bytearray(1024)
                    
                    # 执行ioctl
                    result = fcntl.ioctl(f, cmd_id, params)
                    
                    print(f"  结果: ✓ 成功 (返回值: {result})")
                    success_count += 1
                    
                    # 如果有返回数据，显示前几个字节
                    if any(params[:16]):
                        hex_data = ' '.join(f'{b:02x}' for b in params[:16])
                        print(f"  返回数据: {hex_data}...")
                    
                except OSError as e:
                    print(f"  结果: ✗ 失败")
                    print(f"  错误码: {e.errno} ({errno.errorcode.get(e.errno, 'UNKNOWN')})")
                    print(f"  错误信息: {e.strerror}")
                    
                    # 详细错误分析
                    if e.errno == errno.EINVAL:
                        print("  分析: 参数无效 - 可能是测试未正确启用或命令格式错误")
                    elif e.errno == errno.ENOTTY:
                        print("  分析: 设备不支持此ioctl - 可能是驱动版本问题")
                    elif e.errno == errno.EPERM:
                        print("  分析: 权限不足 - 需要更高权限")
                    elif e.errno == errno.ENODEV:
                        print("  分析: 设备不可用 - 硬件或驱动问题")
                    elif e.errno == errno.ENOSYS:
                        print("  分析: 功能未实现 - 可能是驱动不支持")
                    
                except Exception as e:
                    print(f"  结果: ✗ 异常: {e}")
                    
    except Exception as e:
        print(f"✗ 无法打开设备进行ioctl测试: {e}")
        return 0, total_count
    
    print(f"\n=== ioctl测试总结 ===")
    print(f"成功: {success_count}/{total_count}")
    
    return success_count, total_count

def check_system_info():
    """检查系统相关信息"""
    print("\n=== 系统信息检查 ===")
    
    # 检查内核版本
    ret, stdout, stderr = run_command("uname -r")
    if ret == 0:
        print(f"内核版本: {stdout.strip()}")
    
    # 检查NVIDIA驱动版本
    ret, stdout, stderr = run_command("cat /proc/driver/nvidia/version 2>/dev/null")
    if ret == 0:
        print(f"NVIDIA驱动信息:")
        for line in stdout.strip().split('\n'):
            print(f"  {line}")
    else:
        print("⚠ 无法获取NVIDIA驱动信息")
    
    # 检查加载的NVIDIA模块
    ret, stdout, stderr = run_command("lsmod | grep nvidia")
    if ret == 0:
        print(f"已加载的NVIDIA模块:")
        for line in stdout.strip().split('\n'):
            print(f"  {line}")
    else:
        print("⚠ 未找到NVIDIA模块")
    
    # 检查UVM模块参数
    uvm_params = "/sys/module/nvidia_uvm/parameters/uvm_enable_builtin_tests"
    if os.path.exists(uvm_params):
        with open(uvm_params, 'r') as f:
            param_value = f.read().strip()
            print(f"UVM测试参数: {param_value}")
            if param_value not in ['1', 'Y', 'y']:
                print("  ⚠ 测试功能可能未启用")
    else:
        print("⚠ 无法检查UVM测试参数")

def check_dmesg_logs():
    """检查内核日志中的相关信息"""
    print("\n=== 内核日志检查 ===")
    
    # 查找UVM相关的内核消息
    ret, stdout, stderr = run_command("dmesg | grep -i uvm | tail -10")
    if ret == 0 and stdout.strip():
        print("最近的UVM相关内核消息:")
        for line in stdout.strip().split('\n'):
            print(f"  {line}")
    else:
        print("未找到UVM相关的内核消息")
    
    # 查找NVIDIA相关错误
    ret, stdout, stderr = run_command("dmesg | grep -i nvidia | grep -i error | tail -5")
    if ret == 0 and stdout.strip():
        print("最近的NVIDIA错误消息:")
        for line in stdout.strip().split('\n'):
            print(f"  {line}")

def main():
    print("UVM深度诊断工具")
    print("===============")
    print()
    
    # 检查是否以root身份运行
    if os.geteuid() == 0:
        print("✓ 以root身份运行")
    else:
        print("⚠ 未以root身份运行，某些检查可能失败")
    
    print()
    
    # 执行各项检查
    device_ok = check_uvm_device_details()
    
    if device_ok:
        open_ok = test_basic_device_open()
        
        if open_ok:
            success, total = test_ioctl_calls()
            
            print(f"\n=== 最终诊断 ===")
            if success == 0:
                print("❌ 所有ioctl调用都失败")
                print("\n可能的原因:")
                print("1. UVM驱动版本与测试程序不兼容")
                print("2. 内核编译配置问题")
                print("3. 安全模块(SELinux/AppArmor)阻止")
                print("4. 容器环境限制")
                print("5. 驱动程序bug或损坏")
                
                print("\n建议的解决方案:")
                print("1. 重新安装NVIDIA驱动")
                print("2. 检查SELinux/AppArmor设置")
                print("3. 尝试不同版本的驱动")
                print("4. 在非容器环境中测试")
                
            elif success < total // 2:
                print("⚠ 部分ioctl调用失败")
                print("这可能是正常的，某些功能可能需要特定硬件")
            else:
                print("✓ 大部分ioctl调用成功")
                print("UVM基本功能正常，测试脚本可能有其他问题")
        else:
            print("\n❌ 无法打开UVM设备")
    else:
        print("\n❌ UVM设备访问有问题")
    
    # 系统信息检查
    check_system_info()
    check_dmesg_logs()
    
    print(f"\n=== 诊断完成 ===")
    print("如果问题仍然存在，请将此诊断输出发送给技术支持。")

if __name__ == "__main__":
    main()