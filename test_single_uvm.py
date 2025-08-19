#!/usr/bin/env python3
"""
单个UVM测试调试工具
用于诊断UVM ioctl调用失败的具体原因
"""

import os
import sys
import fcntl
import errno
import struct

def test_uvm_ioctl(cmd_id, cmd_name):
    """测试单个UVM ioctl命令"""
    try:
        with open('/dev/nvidia-uvm', 'rb+') as f:
            # 创建参数缓冲区
            params = bytearray(1024)
            
            print(f"测试 {cmd_name} (ID: {cmd_id})... ", end="", flush=True)
            
            # 执行ioctl调用
            result = fcntl.ioctl(f, cmd_id, params)
            print("✓ 通过")
            return True
            
    except OSError as e:
        print(f"✗ 失败")
        print(f"  错误码: {e.errno}")
        print(f"  错误信息: {e.strerror}")
        
        # 分析具体错误原因
        if e.errno == errno.EINVAL:
            print("  可能原因: 测试功能未启用或命令ID无效")
        elif e.errno == errno.ENOTTY:
            print("  可能原因: 设备不支持此ioctl命令")
        elif e.errno == errno.EPERM:
            print("  可能原因: 权限不足")
        elif e.errno == errno.ENODEV:
            print("  可能原因: 设备不可用")
        else:
            print(f"  其他错误: {e}")
            
        return False
    except Exception as e:
        print(f"✗ 异常: {e}")
        return False

def main():
    print("UVM单个测试诊断工具")
    print("==================")
    print()
    
    # 检查设备文件
    if not os.path.exists('/dev/nvidia-uvm'):
        print("错误: /dev/nvidia-uvm 不存在")
        sys.exit(1)
    
    print("设备文件存在，开始测试...")
    print()
    
    # 测试一些关键的UVM命令
    test_cases = [
        (290, "GET_USER_SPACE_END_ADDRESS"),  # 应该总是成功的测试
        (296, "CGROUP_ACCOUNTING_SUPPORTED"),  # 简单的查询测试
        (201, "RNG_SANITY"),                   # 基本的sanity测试
        (218, "LOCK_SANITY"),                  # 另一个基本测试
        (200, "GET_GPU_REF_COUNT"),            # GPU相关测试
    ]
    
    passed = 0
    total = len(test_cases)
    
    for cmd_id, cmd_name in test_cases:
        if test_uvm_ioctl(cmd_id, cmd_name):
            passed += 1
        print()
    
    print("=" * 40)
    print(f"测试结果: {passed}/{total} 通过")
    
    if passed == 0:
        print()
        print("所有测试都失败！可能的解决方案:")
        print("1. 确保以root身份运行此脚本")
        print("2. 重新加载UVM模块:")
        print("   sudo modprobe -r nvidia_uvm")
        print("   sudo modprobe nvidia-uvm uvm_enable_builtin_tests=1")
        print("3. 检查NVIDIA驱动是否正确安装")
        
    elif passed < total:
        print()
        print("部分测试失败，可能是:")
        print("- GPU硬件相关的测试需要NVIDIA GPU")
        print("- 某些高级功能在当前系统上不可用")
        
    else:
        print()
        print("所有测试都通过！UVM测试功能正常工作。")
        print("原始测试脚本的问题可能在于:")
        print("- 测试参数格式")
        print("- 特定测试用例的兼容性")

if __name__ == "__main__":
    main()