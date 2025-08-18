#!/usr/bin/env python3
"""
修复版最小化UVM测试 - 解决字符设备打开模式问题
"""

import os
import sys
import fcntl
import errno
import struct

def test_minimal_ioctl():
    """执行最基本的UVM ioctl测试"""
    
    print("修复版UVM ioctl测试")
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
        # 修复：使用正确的模式打开字符设备
        # 对于字符设备，应该使用 os.open() 而不是 open()
        fd = os.open(device_path, os.O_RDWR)
        print("✅ 成功打开设备")
        
        try:
            # 测试最简单的ioctl调用
            # GET_USER_SPACE_END_ADDRESS (290) 应该是最基本的调用
            cmd_id = 290
            print(f"\n正在测试ioctl调用 (命令ID: {cmd_id})...")
            
            # 准备参数缓冲区
            import array
            params = array.array('B', [0] * 1024)  # 使用array而不是bytearray
            
            try:
                result = fcntl.ioctl(fd, cmd_id, params)
                print(f"✅ ioctl调用成功!")
                print(f"   返回值: {result}")
                
                # 检查返回的数据
                if any(params[:8]):  # 检查前8个字节
                    # 尝试解析为64位整数
                    try:
                        # 将array转换为bytes再解析
                        data_bytes = params[:8].tobytes()
                        value = struct.unpack('<Q', data_bytes)[0]  # 小端序64位
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
                
        finally:
            os.close(fd)
            
    except PermissionError:
        print("❌ 权限错误 - 需要root权限")
        return False
    except Exception as e:
        print(f"❌ 意外错误: {e}")
        print(f"   错误类型: {type(e).__name__}")
        return False

def test_alternative_method():
    """使用替代方法测试UVM设备"""
    print("\n" + "="*50)
    print("替代方法测试")
    print("="*50)
    
    device_path = "/dev/nvidia-uvm"
    
    try:
        # 方法2：使用更简单的文件操作
        print("尝试简单的文件操作...")
        with open(device_path, "r+b", buffering=0) as f:  # 无缓冲模式
            print("✅ 文件打开成功 (无缓冲模式)")
            
            # 尝试基本的ioctl
            import array
            params = array.array('B', [0] * 1024)
            
            try:
                result = fcntl.ioctl(f.fileno(), 290, params)
                print(f"✅ ioctl成功 (返回值: {result})")
                return True
            except Exception as e:
                print(f"❌ ioctl失败: {e}")
                return False
                
    except Exception as e:
        print(f"❌ 替代方法也失败: {e}")
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
                    return False
        except Exception as e:
            print(f"⚠️ 无法读取参数: {e}")
    else:
        print("⚠️ 参数文件不存在")
    
    return True

def main():
    print("UVM修复版测试工具")
    print("================")
    print()
    print("修复了字符设备打开模式的问题")
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
    
    # 执行修复后的测试
    success = test_minimal_ioctl()
    
    if not success:
        print("\n尝试替代方法...")
        success = test_alternative_method()
    
    if success:
        print("\n🎉 UVM基本功能正常!")
        print("\n现在可以修复原始测试脚本了")
        return 0
    else:
        print("\n❌ UVM功能仍然有问题")
        print("可能需要更深入的系统级诊断")
        return 1

if __name__ == "__main__":
    sys.exit(main())