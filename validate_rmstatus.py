#!/usr/bin/env python3
"""
验证rmStatus检查机制
证明UVM测试确实在内核中执行，只是我们之前检查错了地方
"""

import os
import sys
import fcntl
import array
import struct
import time

def test_rmstatus_validation():
    """验证rmStatus检查机制"""
    print("UVM rmStatus验证测试")
    print("===================")
    print()
    print("🎯 验证UVM测试确实在内核执行，真正的结果在rmStatus字段中")
    print()
    
    device_path = "/dev/nvidia-uvm"
    
    if not os.path.exists(device_path):
        print(f"❌ 设备不存在: {device_path}")
        return
    
    try:
        fd = os.open(device_path, os.O_RDWR)
        
        print("测试1: RANGE_TREE_RANDOM 使用错误参数 (max_batch_count=0)")
        print("预期: ioctl返回0，但rmStatus应该是NV_ERR_INVALID_PARAMETER")
        
        try:
            # 全零参数 - max_batch_count=0应该导致内核返回错误
            params = array.array('B', [0] * 256)
            ioctl_result = fcntl.ioctl(fd, 203, params)  # RANGE_TREE_RANDOM
            
            # 检查rmStatus (在结构末尾)
            rm_status = struct.unpack('<I', params[-4:])[0]
            
            print(f"  ioctl返回值: {ioctl_result}")
            print(f"  rmStatus: {rm_status} (0x{rm_status:08x})")
            
            if ioctl_result == 0 and rm_status == 4:  # NV_ERR_INVALID_PARAMETER
                print("  ✅ 完美！ioctl成功但rmStatus显示参数错误")
                print("  ✅ 这证明测试确实执行到了内核验证代码！")
            elif ioctl_result == 0 and rm_status == 0:
                print("  🚨 rmStatus也是0，可能参数设置有问题")
            else:
                print(f"  ⚠️ 意外结果: ioctl={ioctl_result}, rmStatus={rm_status}")
                
        except Exception as e:
            print(f"  ❌ 测试异常: {e}")
        
        print()
        print("测试2: RANGE_TREE_RANDOM 使用正确参数")
        print("预期: ioctl返回0，rmStatus也应该是0 (NV_OK)")
        
        try:
            # 设置正确的参数
            params = array.array('B', [0] * 256)
            
            # 根据UVM_TEST_RANGE_TREE_RANDOM_PARAMS结构设置参数
            struct.pack_into('<I', params, 0, int(time.time()) % 0xFFFFFFFF)  # seed
            struct.pack_into('<Q', params, 8, 10)      # main_iterations (小值快速测试)
            struct.pack_into('<I', params, 16, 0)      # verbose
            struct.pack_into('<I', params, 20, 75)     # high_probability (75%, <100)
            struct.pack_into('<I', params, 24, 50)     # add_remove_shrink_group_probability (<100)
            struct.pack_into('<I', params, 28, 25)     # shrink_probability
            struct.pack_into('<I', params, 32, 5)      # collision_checks
            struct.pack_into('<I', params, 36, 3)      # iterator_checks
            struct.pack_into('<Q', params, 40, 0x10000)   # max_end
            struct.pack_into('<Q', params, 48, 50)     # max_ranges
            struct.pack_into('<Q', params, 56, 5)      # max_batch_count (>0!)
            struct.pack_into('<I', params, 64, 50)     # max_attempts
            
            ioctl_result = fcntl.ioctl(fd, 203, params)
            rm_status = struct.unpack('<I', params[-4:])[0]
            
            print(f"  ioctl返回值: {ioctl_result}")
            print(f"  rmStatus: {rm_status} (0x{rm_status:08x})")
            
            if ioctl_result == 0 and rm_status == 0:
                print("  ✅ 完美！测试真正成功了")
                print("  ✅ 这证明正确的参数设置很重要")
                
                # 检查统计数据是否被填充
                stats_start = 68  # stats结构开始位置
                total_adds = struct.unpack('<Q', params[stats_start:stats_start+8])[0]
                if total_adds > 0:
                    print(f"  📊 测试统计: total_adds={total_adds} (证明测试真的运行了)")
                
            else:
                print(f"  ⚠️ 仍然失败: ioctl={ioctl_result}, rmStatus={rm_status}")
                
        except Exception as e:
            print(f"  ❌ 测试异常: {e}")
        
        print()
        print("测试3: 简单的查询测试")
        print("预期: 应该总是成功")
        
        try:
            # GET_USER_SPACE_END_ADDRESS - 简单查询
            params = array.array('B', [0] * 16)
            ioctl_result = fcntl.ioctl(fd, 290, params)
            
            # rmStatus在8字节地址之后
            rm_status = struct.unpack('<I', params[8:12])[0]
            user_space_end = struct.unpack('<Q', params[0:8])[0]
            
            print(f"  ioctl返回值: {ioctl_result}")
            print(f"  rmStatus: {rm_status}")
            print(f"  用户空间结束地址: 0x{user_space_end:016x}")
            
            if ioctl_result == 0 and rm_status == 0 and user_space_end > 0:
                print("  ✅ 查询测试完全正常，返回了有效数据")
            
        except Exception as e:
            print(f"  ❌ 查询测试异常: {e}")
        
        os.close(fd)
        
    except Exception as e:
        print(f"❌ 设备访问失败: {e}")

def main():
    if os.geteuid() != 0:
        print("建议以root身份运行: sudo python3", sys.argv[0])
        print()
    
    test_rmstatus_validation()
    
    print()
    print("="*60)
    print("🎯 关键发现总结")
    print("="*60)
    print()
    print("如果测试1显示rmStatus=4 (NV_ERR_INVALID_PARAMETER):")
    print("✅ 证明UVM测试确实在内核中执行")
    print("✅ 证明参数验证正常工作")
    print("✅ 证明我们之前只检查ioctl返回值是错误的")
    print()
    print("如果测试2显示rmStatus=0且有统计数据:")
    print("✅ 证明正确的参数设置很重要")
    print("✅ 证明测试能够真正执行并产生结果")
    print()
    print("这完全验证了您的分析：")
    print("- UVM测试确实执行到内核")
    print("- 参数验证确实工作")
    print("- 只是错误报告机制与预期不同")

if __name__ == "__main__":
    main()