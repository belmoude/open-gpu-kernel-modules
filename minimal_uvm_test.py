#!/usr/bin/env python3
"""
最小化UVM测试 - 用于确定确切的失败原因
"""

import os
import sys
import fcntl
import errno
import struct

def test_minimal_ioctl():
    """执行最基本的UVM ioctl测试"""
    
    print("最小化UVM ioctl测试")
    print("==================")
    print()
    
    device_path = "/dev/nvidia-uvm"
    
    # 检查设备文件
    if not os.path.exists(device_path):
        print(f"❌ 设备文件不存在: {device_path}")
        return False
    
    print(f"✅ 设备文件存在: {device_path}")
    
    # 检查权限
    if not (os.access(device_path, os.R_OK) and os.access(device_path, os.W_OK)):
        print(f"❌ 权限不足，需要读写权限")
        print(f"   当前用户: {os.getenv('USER', 'unknown')}")
        print(f"   建议: sudo python3 {sys.argv[0]}")
        return False
    
    print("✅ 权限检查通过")
    
    try:
        print("\n正在打开UVM设备...")
        with open(device_path, "rb+") as f:
            print("✅ 成功打开设备")
            
            # 测试最简单的ioctl调用
            # GET_USER_SPACE_END_ADDRESS (290) 应该是最基本的调用
            cmd_id = 290
            print(f"\n正在测试ioctl调用 (命令ID: {cmd_id})...")
            
            # 准备参数缓冲区
            params = bytearray(1024)  # 1KB缓冲区
            
            try:
                result = fcntl.ioctl(f, cmd_id, params)
                print(f"✅ ioctl调用成功!")
                print(f"   返回值: {result}")
                
                # 检查返回的数据
                if any(params[:8]):  # 检查前8个字节
                    # 尝试解析为64位整数
                    try:
                        value = struct.unpack('<Q', params[:8])[0]  # 小端序64位
                        print(f"   返回数据 (64位): 0x{value:016x}")
                    except:
                        hex_data = ' '.join(f'{b:02x}' for b in params[:16])
                        print(f"   返回数据 (hex): {hex_data}")
                else:
                    print("   无返回数据")
                
                return True
                
            except OSError as e:
                print(f"❌ ioctl调用失败")
                print(f"   错误码: {e.errno}")
                print(f"   错误名: {errno.errorcode.get(e.errno, 'UNKNOWN')}")
                print(f"   错误信息: {e.strerror}")
                
                # 详细分析
                if e.errno == errno.EINVAL:  # 22
                    print("\n🔍 错误分析:")
                    print("   EINVAL (22) - 参数无效")
                    print("   可能原因:")
                    print("   1. UVM测试功能未正确启用")
                    print("   2. 命令ID不被当前驱动版本支持")
                    print("   3. 参数缓冲区格式不正确")
                    print("\n   建议检查:")
                    print("   - cat /sys/module/nvidia_uvm/parameters/uvm_enable_builtin_tests")
                    print("   - dmesg | grep -i uvm")
                    
                elif e.errno == errno.ENOTTY:  # 25
                    print("\n🔍 错误分析:")
                    print("   ENOTTY (25) - 设备不支持此ioctl")
                    print("   可能原因:")
                    print("   1. 驱动版本太老，不支持测试接口")
                    print("   2. 编译时未包含测试功能")
                    
                elif e.errno == errno.EPERM:  # 1
                    print("\n🔍 错误分析:")
                    print("   EPERM (1) - 操作不被允许")
                    print("   需要更高权限或特殊配置")
                    
                return False
                
    except PermissionError:
        print("❌ 权限错误 - 需要root权限")
        return False
    except Exception as e:
        print(f"❌ 意外错误: {e}")
        return False

def check_uvm_module_info():
    """检查UVM模块的详细信息"""
    print("\n" + "="*50)
    print("UVM模块信息检查")
    print("="*50)
    
    # 检查模块是否加载
    try:
        with open('/proc/modules', 'r') as f:
            modules = f.read()
            if 'nvidia_uvm' in modules:
                print("✅ nvidia_uvm模块已加载")
                for line in modules.split('\n'):
                    if 'nvidia_uvm' in line:
                        print(f"   {line}")
            else:
                print("❌ nvidia_uvm模块未加载")
                return False
    except:
        print("⚠️ 无法检查模块状态")
    
    # 检查模块参数
    param_file = "/sys/module/nvidia_uvm/parameters/uvm_enable_builtin_tests"
    if os.path.exists(param_file):
        try:
            with open(param_file, 'r') as f:
                value = f.read().strip()
                print(f"✅ UVM测试参数: {value}")
                if value not in ['1', 'Y', 'y']:
                    print("❌ 测试功能未启用!")
                    print("   请运行: sudo modprobe -r nvidia_uvm && sudo modprobe nvidia-uvm uvm_enable_builtin_tests=1")
                    return False
        except Exception as e:
            print(f"⚠️ 无法读取参数: {e}")
    else:
        print("⚠️ 参数文件不存在")
    
    return True

def main():
    print("UVM最小化测试工具")
    print("================")
    print()
    print("此工具执行最基本的UVM测试来确定失败的确切原因")
    print()
    
    # 检查运行权限
    if os.geteuid() != 0:
        print("⚠️ 建议以root身份运行以获得完整诊断信息")
        print("   sudo python3", sys.argv[0])
        print()
    
    # 检查UVM模块
    if not check_uvm_module_info():
        print("\n❌ UVM模块配置有问题，请先解决模块问题")
        return 1
    
    # 执行基本测试
    if test_minimal_ioctl():
        print("\n🎉 基本UVM功能正常!")
        print("\n如果完整测试仍然失败，可能的原因:")
        print("1. 特定测试用例的参数格式问题")
        print("2. 某些测试需要特定的硬件配置")
        print("3. 测试脚本中的命令ID映射错误")
        return 0
    else:
        print("\n❌ 基本UVM功能失败")
        print("\n这解释了为什么所有测试都失败")
        print("需要解决上述诊断中发现的问题")
        return 1

if __name__ == "__main__":
    sys.exit(main())