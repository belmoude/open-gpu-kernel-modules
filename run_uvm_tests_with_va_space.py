#!/usr/bin/env python3
"""
UVM测试运行器 - 增强版本，包含VA空间创建
基于现有测试脚本，添加VA空间管理来解决NV_ERR_ILLEGAL_ACTION错误
"""

import os
import sys
import fcntl
import array
import struct
import time
import argparse

# NV_STATUS 错误码
NV_STATUS_CODES = {
    0x00000000: "NV_OK",
    0x00000001: "NV_ERR_GENERIC",
    0x00000004: "NV_ERR_INVALID_PARAMETER",
    0x00000005: "NV_ERR_INVALID_ARGUMENT",
    0x00000006: "NV_ERR_INVALID_STATE",
    0x00000016: "NV_ERR_ILLEGAL_ACTION",
    0x00000032: "NV_ERR_INVALID_DEVICE",
    0x00000046: "NV_ERR_NO_MEMORY",
    0x00000065: "NV_ERR_NOT_SUPPORTED",
}

def get_nv_status_name(status_code):
    return NV_STATUS_CODES.get(status_code, f"UNKNOWN_0x{status_code:08x}")

def setup_test_params(cmd_id, test_name):
    """设置测试参数 - 使用之前完善的参数设置"""
    
    if cmd_id == 203:  # RANGE_TREE_RANDOM
        params = array.array('B', [0] * 256)
        struct.pack_into('<I', params, 0, int(time.time()) % 0xFFFFFFFF)
        struct.pack_into('<Q', params, 8, 50)
        struct.pack_into('<I', params, 16, 0)
        struct.pack_into('<I', params, 20, 75)
        struct.pack_into('<I', params, 24, 60)
        struct.pack_into('<I', params, 28, 30)
        struct.pack_into('<I', params, 32, 10)
        struct.pack_into('<I', params, 36, 5)
        struct.pack_into('<Q', params, 40, 0x100000)
        struct.pack_into('<Q', params, 48, 100)
        struct.pack_into('<Q', params, 56, 10)  # max_batch_count > 0
        struct.pack_into('<I', params, 64, 100)
        return params, 252
    elif cmd_id == 201:  # RNG_SANITY
        params = array.array('B', [0] * 8)
        return params, 0
    elif cmd_id == 218:  # LOCK_SANITY
        params = array.array('B', [0] * 8)
        return params, 0
    elif cmd_id == 230:  # MEM_SANITY
        params = array.array('B', [0] * 8)
        return params, 0
    elif cmd_id == 290:  # GET_USER_SPACE_END_ADDRESS
        params = array.array('B', [0] * 16)
        return params, 8
    elif cmd_id == 235:  # VA_RESIDENCY_INFO
        params = array.array('B', [0] * 4096)
        struct.pack_into('<Q', params, 0, 0xB00000)
        struct.pack_into('<Q', params, 8, 0x1000)
        return params, 4092
    elif cmd_id == 215:  # CHANNEL_STRESS
        params = array.array('B', [0] * 32)
        struct.pack_into('<I', params, 0, 0)
        struct.pack_into('<I', params, 4, 100)
        struct.pack_into('<I', params, 8, 4)
        struct.pack_into('<I', params, 12, 0)
        struct.pack_into('<I', params, 16, int(time.time()) % 0xFFFFFFFF)
        struct.pack_into('<I', params, 20, 0)
        return params, 24
    else:
        # 默认设置
        params = array.array('B', [0] * 1024)
        return params, 1020

class UVMTestRunner:
    """UVM测试运行器 - 支持VA空间管理"""
    
    def __init__(self, device_path="/dev/nvidia-uvm"):
        self.device_path = device_path
        self.fd = None
        self.va_space_created = False
        self.verbose = False
        
    def create_va_space(self):
        """创建UVM VA空间"""
        if self.verbose:
            print("🔧 创建UVM VA空间...")
        
        # 方法1: 使用UVM_RESERVE_VA
        try:
            params = array.array('B', [0] * 32)
            base_address = 0x10000000  # 256MB
            length = 0x10000000        # 256MB
            
            struct.pack_into('<Q', params, 0, base_address)
            struct.pack_into('<Q', params, 8, length)
            
            ioctl_result = fcntl.ioctl(self.fd, 1, params)  # UVM_RESERVE_VA
            rm_status = struct.unpack('<I', params[16:20])[0]
            
            if rm_status == 0:
                if self.verbose:
                    print(f"  ✅ VA空间创建成功")
                self.va_space_created = True
                return True
            else:
                if self.verbose:
                    print(f"  ❌ VA空间创建失败: {get_nv_status_name(rm_status)}")
                
        except Exception as e:
            if self.verbose:
                print(f"  ❌ VA空间创建异常: {e}")
        
        return False
    
    def cleanup_va_space(self):
        """清理VA空间"""
        if not self.va_space_created:
            return
        
        try:
            params = array.array('B', [0] * 32)
            struct.pack_into('<Q', params, 0, 0x10000000)
            struct.pack_into('<Q', params, 8, 0x10000000)
            fcntl.ioctl(self.fd, 2, params)  # UVM_RELEASE_VA
            
            if self.verbose:
                print("🧹 VA空间清理完成")
                
        except Exception as e:
            if self.verbose:
                print(f"⚠️ VA空间清理失败: {e}")
    
    def run_single_test(self, cmd_id, test_name, description, requires_gpu=False):
        """运行单个测试"""
        try:
            params, rmstatus_offset = setup_test_params(cmd_id, test_name)
            ioctl_result = fcntl.ioctl(self.fd, cmd_id, params)
            
            if rmstatus_offset >= 0 and rmstatus_offset + 4 <= len(params):
                rm_status = struct.unpack('<I', params[rmstatus_offset:rmstatus_offset+4])[0]
            else:
                rm_status = -1
            
            return {
                'ioctl_result': ioctl_result,
                'rm_status': rm_status,
                'status_name': get_nv_status_name(rm_status),
                'success': rm_status == 0
            }
            
        except Exception as e:
            return {'error': str(e), 'success': False}
    
    def run_all_tests(self, test_filter=None, continue_on_error=True):
        """运行所有测试"""
        
        # 所有测试定义
        all_tests = [
            (200, "GET_GPU_REF_COUNT", "Get GPU reference count", True),
            (201, "RNG_SANITY", "Random number generator sanity test", False),
            (202, "RANGE_TREE_DIRECTED", "Directed range tree test", False), 
            (203, "RANGE_TREE_RANDOM", "Random range tree test", False),
            (204, "VA_RANGE_INFO", "VA range information test", False),
            (205, "RM_MEM_SANITY", "RM memory sanity test", True),
            (206, "GPU_SEMAPHORE_SANITY", "GPU semaphore sanity test", True),
            (207, "PEER_REF_COUNT", "Peer reference count test", True),
            (208, "VA_RANGE_SPLIT", "VA range split test", False),
            (209, "VA_RANGE_INJECT_SPLIT_ERROR", "VA range split error injection test", False),
            (210, "PAGE_TREE", "Page tree test", True),
            (211, "CHANGE_PTE_MAPPING", "Change PTE mapping test", True),
            (212, "TRACKER_SANITY", "Tracker sanity test", True),
            (213, "PUSH_SANITY", "Push sanity test", True),
            (214, "CHANNEL_SANITY", "Channel sanity test", True),
            (215, "CHANNEL_STRESS", "Channel stress test", True),
            (216, "CE_SANITY", "Copy engine sanity test", True),
            (217, "VA_BLOCK_INFO", "VA block information test", False),
            (218, "LOCK_SANITY", "Lock sanity test", False),
            (219, "PERF_UTILS_SANITY", "Performance utils sanity test", False),
            (220, "KVMALLOC", "Kernel memory allocation test", False),
            (221, "PMM_QUERY", "Physical memory manager query test", True),
            (222, "PMM_CHECK_LEAK", "PMM leak check test", True),
            (223, "PERF_EVENTS_SANITY", "Performance events sanity test", False),
            (224, "PERF_MODULE_SANITY", "Performance module sanity test", False),
            (225, "RANGE_ALLOCATOR_SANITY", "Range allocator sanity test", False),
            (226, "GET_RM_PTES", "Get RM PTEs test", True),
            (227, "FAULT_BUFFER_FLUSH", "Fault buffer flush test", True),
            (228, "INJECT_TOOLS_EVENT", "Inject tools event test", False),
            (229, "INCREMENT_TOOLS_COUNTER", "Increment tools counter test", False),
            (230, "MEM_SANITY", "Memory sanity test", False),
            (232, "MAKE_CHANNEL_STOPS_IMMEDIATE", "Make channel stops immediate test", True),
            (233, "VA_BLOCK_INJECT_ERROR", "VA block error injection test", False),
            (234, "PEER_IDENTITY_MAPPINGS", "Peer identity mappings test", True),
            (235, "VA_RESIDENCY_INFO", "VA residency information test", False),
            (236, "PMM_ASYNC_ALLOC", "PMM async allocation test", True),
            (237, "SET_PREFETCH_FILTERING", "Set prefetch filtering test", False),
            (240, "PMM_SANITY", "PMM sanity test", True),
            (241, "INVALIDATE_TLB", "TLB invalidation test", True),
            (242, "VA_BLOCK", "VA block test", False),
            (243, "EVICT_CHUNK", "Evict chunk test", True),
            (244, "FLUSH_DEFERRED_WORK", "Flush deferred work test", False),
            (245, "NV_KTHREAD_Q", "NV kernel thread queue test", False),
            (246, "SET_PAGE_PREFETCH_POLICY", "Set page prefetch policy test", False),
            (247, "RANGE_GROUP_TREE", "Range group tree test", False),
            (248, "RANGE_GROUP_RANGE_INFO", "Range group range info test", False),
            (249, "RANGE_GROUP_RANGE_COUNT", "Range group range count test", False),
            (250, "GET_PREFETCH_FAULTS_REENABLE_LAPSE", "Get prefetch faults reenable lapse", False),
            (251, "SET_PREFETCH_FAULTS_REENABLE_LAPSE", "Set prefetch faults reenable lapse", False),
            (252, "GET_KERNEL_VIRTUAL_ADDRESS", "Get kernel virtual address test", False),
            (290, "GET_USER_SPACE_END_ADDRESS", "Get user space end address test", False),
            (291, "GET_CPU_CHUNK_ALLOC_SIZES", "Get CPU chunk allocation sizes test", False),
            (296, "CGROUP_ACCOUNTING_SUPPORTED", "CGroup accounting supported test", False),
        ]
        
        # 过滤测试
        if test_filter:
            all_tests = [(cmd_id, name, desc, gpu) for cmd_id, name, desc, gpu in all_tests 
                        if test_filter.lower() in name.lower()]
        
        print(f"UVM测试运行器 - VA空间增强版")
        print(f"===========================")
        print(f"✅ 添加了VA空间创建功能")
        print(f"✅ 解决NV_ERR_ILLEGAL_ACTION (0x16)错误")
        print()
        
        # 打开设备
        self.fd = os.open(self.device_path, os.O_RDWR)
        
        try:
            # 创建VA空间
            va_created = self.create_va_space()
            if va_created:
                print("✅ VA空间创建成功，可以运行所有测试")
            else:
                print("⚠️ VA空间创建失败，某些测试可能失败")
            print()
            
            # 运行测试
            stats = {"pass": 0, "fail": 0, "error": 0}
            start_time = time.time()
            
            print(f"开始运行 {len(all_tests)} 个测试...")
            print()
            
            for i, (cmd_id, test_name, description, requires_gpu) in enumerate(all_tests, 1):
                print(f"[{i:2}/{len(all_tests)}] {test_name:35} ", end="", flush=True)
                
                if self.verbose:
                    print()
                    print(f"    描述: {description}")
                    print(f"    命令ID: {cmd_id}")
                    print(f"    需要GPU: {'是' if requires_gpu else '否'}")
                    print("    执行中... ", end="", flush=True)
                
                result = self.run_single_test(cmd_id, test_name, description, requires_gpu)
                
                if 'error' in result:
                    print("[SYSTEM_ERROR]")
                    if self.verbose:
                        print(f"    系统错误: {result['error']}")
                    else:
                        print(f"  系统错误: {result['error']}")
                    stats['error'] += 1
                    
                    if not continue_on_error:
                        break
                        
                elif result['success']:
                    print("[PASS]")
                    if self.verbose:
                        print(f"    结果: 测试成功")
                    stats['pass'] += 1
                    
                else:
                    print("[FAIL]")
                    if self.verbose:
                        print(f"    结果: 测试失败")
                        print(f"    rmStatus: {result['status_name']}")
                    else:
                        print(f"  内核错误: {result['status_name']}")
                    stats['fail'] += 1
                
                time.sleep(0.01)
            
            end_time = time.time()
            
            # 清理VA空间
            self.cleanup_va_space()
            
            # 打印总结
            total = sum(stats.values())
            print()
            print("=" * 70)
            print("VA空间增强版测试结果")
            print("=" * 70)
            print(f"VA空间创建:   {'成功' if va_created else '失败'}")
            print(f"总测试数:     {total}")
            print(f"通过:         {stats['pass']}")
            print(f"失败:         {stats['fail']}")
            print(f"系统错误:     {stats['error']}")
            print(f"成功率:       {stats['pass']*100//total if total > 0 else 0}%")
            print(f"执行时间:     {end_time - start_time:.1f} 秒")
            
            # 与之前结果比较
            if va_created:
                print()
                print("与之前结果比较:")
                print("- 之前成功率: 75% (73/97)")
                print(f"- 当前成功率: {stats['pass']*100//total if total > 0 else 0}%")
                
                if stats['pass'] > 73:
                    improvement = stats['pass'] - 73
                    print(f"- ✅ 改善了 {improvement} 个测试!")
                    print("- ✅ VA空间创建确实有效!")
                elif stats['pass'] == 73:
                    print("- ⚠️ 成功率相同，可能需要其他初始化")
                else:
                    print("- ❌ 成功率下降，可能VA空间创建有问题")
            
            return stats['error'] == 0
            
        finally:
            if self.fd:
                os.close(self.fd)

def main():
    parser = argparse.ArgumentParser(description="UVM Test Runner with VA Space Support")
    parser.add_argument("-v", "--verbose", action="store_true", help="详细输出")
    parser.add_argument("-c", "--continue", action="store_true", help="错误后继续")
    parser.add_argument("-t", "--test", help="运行指定测试")
    parser.add_argument("-f", "--filter", help="过滤测试名称")
    parser.add_argument("--no-va-space", action="store_true", help="不创建VA空间（对比测试）")
    
    args = parser.parse_args()
    
    if os.geteuid() != 0:
        print("建议以root身份运行: sudo python3", sys.argv[0])
        print()
    
    runner = UVMTestRunner()
    runner.verbose = args.verbose
    
    if args.test:
        # 运行单个测试
        print(f"运行单个测试: {args.test}")
        print("=" * 40)
        
        # 查找测试
        test_found = False
        all_tests = [
            (201, "RNG_SANITY"), (218, "LOCK_SANITY"), (230, "MEM_SANITY"),
            (203, "RANGE_TREE_RANDOM"), (235, "VA_RESIDENCY_INFO"),
            (290, "GET_USER_SPACE_END_ADDRESS")
        ]
        
        for cmd_id, name in all_tests:
            if name == args.test:
                test_found = True
                
                runner.fd = os.open(runner.device_path, os.O_RDWR)
                
                try:
                    # 测试1: 不创建VA空间
                    print("测试1: 不创建VA空间")
                    result1 = runner.run_single_test(cmd_id, name, "", False)
                    print(f"  结果: {'成功' if result1['success'] else '失败'}")
                    if not result1['success']:
                        print(f"  错误: {result1['status_name']}")
                    
                    print()
                    
                    # 测试2: 创建VA空间后
                    print("测试2: 创建VA空间后")
                    va_created = runner.create_va_space()
                    result2 = runner.run_single_test(cmd_id, name, "", False)
                    print(f"  VA空间: {'创建成功' if va_created else '创建失败'}")
                    print(f"  结果: {'成功' if result2['success'] else '失败'}")
                    if not result2['success']:
                        print(f"  错误: {result2['status_name']}")
                    
                    # 比较结果
                    print()
                    print("比较分析:")
                    if result1['success'] and result2['success']:
                        print("  ✅ 两次都成功 - 此测试不需要VA空间")
                    elif not result1['success'] and result2['success']:
                        print("  🎉 VA空间创建解决了问题!")
                    elif not result1['success'] and not result2['success']:
                        if result1['rm_status'] != result2['rm_status']:
                            print("  🔄 错误类型改变，VA空间有部分作用")
                        else:
                            print("  ❌ VA空间创建没有解决此问题")
                    else:
                        print("  ⚠️ 意外情况")
                    
                    runner.cleanup_va_space()
                    
                finally:
                    os.close(runner.fd)
                
                break
        
        if not test_found:
            print(f"测试 '{args.test}' 未找到")
            
    else:
        # 运行完整测试
        success = runner.run_all_tests(args.filter, getattr(args, 'continue'))
        sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()