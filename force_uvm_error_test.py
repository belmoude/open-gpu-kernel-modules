#!/usr/bin/env python3
"""
强制UVM错误测试 - 用于验证内核调试输出是否工作
通过故意触发错误来产生内核输出
"""

import os
import sys
import fcntl
import array
import errno

def test_uvm_error_output():
    """故意触发UVM错误来测试内核输出"""
    print("UVM错误输出测试")
    print("===============")
    print()
    print("此工具故意触发UVM错误来验证内核调试输出是否正常工作")
    print()
    
    device_path = "/dev/nvidia-uvm"
    
    if not os.path.exists(device_path):
        print(f"❌ 设备文件不存在: {device_path}")
        return False
    
    try:
        fd = os.open(device_path, os.O_RDWR)
        
        print("测试1: 使用无效的ioctl命令...")
        try:
            params = array.array('B', [0] * 1024)
            # 使用一个肯定无效的命令ID
            result = fcntl.ioctl(fd, 99999, params)
            print("❌ 意外成功 - 这不应该发生")
        except OSError as e:
            print(f"✅ 预期失败: 错误码 {e.errno} ({e.strerror})")
            print("   这应该在内核日志中产生错误消息")
        
        print()
        print("测试2: 使用错误的参数大小...")
        try:
            # 使用非常小的缓冲区，可能触发EFAULT
            params = array.array('B', [0] * 1)  # 只有1字节
            result = fcntl.ioctl(fd, 235, params)  # VA_RESIDENCY_INFO
            print("❌ 意外成功")
        except OSError as e:
            print(f"✅ 预期失败: 错误码 {e.errno} ({e.strerror})")
            print("   这应该在内核日志中产生EFAULT错误消息")
        
        print()
        print("测试3: 运行一个正常的测试...")
        try:
            params = array.array('B', [0] * 1024)
            result = fcntl.ioctl(fd, 201, params)  # RNG_SANITY
            print(f"✅ 正常测试成功: 返回值 {result}")
            print("   这可能在内核日志中产生成功消息")
        except OSError as e:
            print(f"❌ 正常测试失败: {e}")
        
        os.close(fd)
        
    except Exception as e:
        print(f"❌ 设备访问失败: {e}")
        return False
    
    print()
    print("测试完成！")
    print("现在检查内核日志:")
    print("dmesg | tail -20")
    print("或者:")
    print("dmesg | grep -E '(nvidia_uvm|UVM)' | tail -10")
    
    return True

if __name__ == "__main__":
    if os.geteuid() != 0:
        print("建议以root身份运行以获得完整权限")
        print("sudo python3", sys.argv[0])
        print()
    
    test_uvm_error_output()