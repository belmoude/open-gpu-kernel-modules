#!/usr/bin/env python3
"""
UVM正确初始化测试运行器
使用UVM_INITIALIZE (0x30000001)进行正确的初始化
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
    0x00000017: "NV_ERR_IN_USE",
    0x00000032: "NV_ERR_INVALID_DEVICE",
    0x00000046: "NV_ERR_NO_MEMORY",
    0x00000065: "NV_ERR_NOT_SUPPORTED",
}

def get_nv_status_name(status_code):
    return NV_STATUS_CODES.get(status_code, f"UNKNOWN_0x{status_code:08x}")

def setup_test_params(cmd_id, test_name):
    """为每个测试设置正确的参数"""
    
    if cmd_id == 203:  # RANGE_TREE_RANDOM
        params = array.array('B', [0] * 256)
        struct.pack_into('<I', params, 0, int(time.time()) % 0xFFFFFFFF)  # seed
        struct.pack_into('<Q', params, 8, 50)      # main_iterations
        struct.pack_into('<I', params, 16, 0)      # verbose
        struct.pack_into('<I', params, 20, 75)     # high_probability
        struct.pack_into('<I', params, 24, 60)     # add_remove_shrink_group_probability
        struct.pack_into('<I', params, 28, 30)     # shrink_probability
        struct.pack_into('<I', params, 32, 10)     # collision_checks
        struct.pack_into('<I', params, 36, 5)      # iterator_checks
        struct.pack_into('<Q', params, 40, 0x100000)  # max_end
        struct.pack_into('<Q', params, 48, 100)    # max_ranges
        struct.pack_into('<Q', params, 56, 10)     # max_batch_count (>0!)
        struct.pack_into('<I', params, 64, 100)    # max_attempts
        return params, 252
    elif cmd_id == 215:  # CHANNEL_STRESS
        params = array.array('B', [0] * 32)
        struct.pack_into('<I', params, 0, 0)        # mode
        struct.pack_into('<I', params, 4, 100)      # iterations
        struct.pack_into('<I', params, 8, 4)        # num_streams
        struct.pack_into('<I', params, 12, 0)       # key_rotation_operation
        struct.pack_into('<I', params, 16, int(time.time()) % 0xFFFFFFFF)  # seed
        struct.pack_into('<I', params, 20, 0)       # verbose
        return params, 24
    elif cmd_id == 235:  # VA_RESIDENCY_INFO
        params = array.array('B', [0] * 4096)
        struct.pack_into('<Q', params, 0, 0xB00000)  # lookup_address
        struct.pack_into('<Q', params, 8, 0x1000)   # length
        return params, 4092
    elif cmd_id == 220:  # KVMALLOC
        params = array.array('B', [0] * 16)
        struct.pack_into('<I', params, 0, 4)        # num_threads
        struct.pack_into('<I', params, 4, 100)      # iterations_per_thread
        struct.pack_into('<I', params, 8, 4096)     # max_allocation_size
        return params, 12
    elif cmd_id in [201, 202, 205, 206, 212, 213, 214, 216, 218, 219, 223, 224, 230, 232, 244, 245, 281, 288, 295, 301]:
        # 简单的只有rmStatus的测试
        params = array.array('B', [0] * 8)
        return params, 0
    elif cmd_id == 290:  # GET_USER_SPACE_END_ADDRESS
        params = array.array('B', [0] * 16)
        return params, 8
    elif cmd_id == 291:  # GET_CPU_CHUNK_ALLOC_SIZES
        params = array.array('B', [0] * 16)
        return params, 4
    elif cmd_id == 296:  # CGROUP_ACCOUNTING_SUPPORTED
        params = array.array('B', [0] * 8)
        return params, 0
    else:
        # 其他测试的默认设置
        params = array.array('B', [0] * 1024)
        return params, 1020

class UVMTestRunnerCorrect:
    """正确的UVM测试运行器 - 使用UVM_INITIALIZE"""
    
    def __init__(self, device_path="/dev/nvidia-uvm"):
        self.device_path = device_path
        self.fd = None
        self.initialized = False
        self.verbose = False
    
    def initialize_uvm(self):
        """使用UVM_INITIALIZE正确初始化UVM"""
        if self.verbose:
            print("🔧 使用UVM_INITIALIZE初始化UVM...")
        
        try:
            # UVM_INITIALIZE_PARAMS: flags (8字节) + rmStatus (4字节)
            params = array.array('B', [0] * 16)
            
            # 设置flags为0 (默认标志)
            struct.pack_into('<Q', params, 0, 0)  # flags
            
            # 执行UVM_INITIALIZE
            ioctl_result = fcntl.ioctl(self.fd, 0x30000001, params)  # UVM_INITIALIZE
            rm_status = struct.unpack('<I', params[8:12])[0]  # rmStatus在8字节flags之后
            
            if self.verbose:
                print(f"  ioctl返回值: {ioctl_result}")
                print(f"  rmStatus: {rm_status} ({get_nv_status_name(rm_status)})")
            
            if rm_status == 0:
                if self.verbose:
                    print("  ✅ UVM初始化成功!")
                self.initialized = True
                return True
            else:
                if self.verbose:
                    print(f"  ❌ UVM初始化失败: {get_nv_status_name(rm_status)}")
                return False
                
        except Exception as e:
            if self.verbose:
                print(f"  ❌ UVM初始化异常: {e}")
            return False
    
    def deinitialize_uvm(self):
        """清理UVM"""
        if not self.initialized:
            return
        
        try:
            # 执行UVM_DEINITIALIZE (如果需要)
            ioctl_result = fcntl.ioctl(self.fd, 0x30000002, 0)  # UVM_DEINITIALIZE
            if self.verbose:
                print("🧹 UVM清理完成")
        except Exception as e:
            if self.verbose:
                print(f"⚠️ UVM清理失败: {e}")
    
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
        """运行所有97个测试，使用正确的初始化"""
        
        # 完整的97个测试用例
        ALL_UVM_TESTS = [
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
            (253, "PMA_ALLOC_FREE", "PMA allocation/free test", True),
            (254, "PMM_ALLOC_FREE_ROOT", "PMM alloc/free root test", True),
            (255, "PMM_INJECT_PMA_EVICT_ERROR", "PMM inject PMA evict error test", True),
            (256, "RECONFIGURE_ACCESS_COUNTERS", "Reconfigure access counters test", True),
            (257, "RESET_ACCESS_COUNTERS", "Reset access counters test", True),
            (258, "SET_IGNORE_ACCESS_COUNTERS", "Set ignore access counters test", True),
            (259, "CHECK_CHANNEL_VA_SPACE", "Check channel VA space test", True),
            (260, "ENABLE_NVLINK_PEER_ACCESS", "Enable NVLink peer access test", True),
            (261, "DISABLE_NVLINK_PEER_ACCESS", "Disable NVLink peer access test", True),
            (262, "GET_PAGE_THRASHING_POLICY", "Get page thrashing policy test", False),
            (263, "SET_PAGE_THRASHING_POLICY", "Set page thrashing policy test", False),
            (264, "PMM_SYSMEM", "PMM system memory test", False),
            (265, "PMM_REVERSE_MAP", "PMM reverse mapping test", True),
            (266, "PMM_INDIRECT_PEERS", "PMM indirect peers test", True),
            (267, "VA_SPACE_MM_RETAIN", "VA space MM retain test", False),
            (269, "PMM_CHUNK_WITH_ELEVATED_PAGE", "PMM chunk with elevated page test", True),
            (270, "GET_GPU_TIME", "Get GPU time test", True),
            (271, "ACCESS_COUNTERS_ENABLED_BY_DEFAULT", "Access counters enabled by default", True),
            (272, "VA_SPACE_INJECT_ERROR", "VA space error injection test", False),
            (273, "PMM_RELEASE_FREE_ROOT_CHUNKS", "PMM release free root chunks test", True),
            (274, "DRAIN_REPLAYABLE_FAULTS", "Drain replayable faults test", True),
            (275, "PMA_GET_BATCH_SIZE", "PMA get batch size test", True),
            (276, "PMM_QUERY_PMA_STATS", "PMM query PMA stats test", True),
            (278, "NUMA_CHECK_AFFINITY", "NUMA check affinity test", False),
            (279, "VA_SPACE_ADD_DUMMY_THREAD_CONTEXTS", "VA space add dummy thread contexts", False),
            (280, "VA_SPACE_REMOVE_DUMMY_THREAD_CONTEXTS", "VA space remove dummy thread contexts", False),
            (281, "THREAD_CONTEXT_SANITY", "Thread context sanity test", False),
            (282, "THREAD_CONTEXT_PERF", "Thread context performance test", False),
            (283, "GET_PAGEABLE_MEM_ACCESS_TYPE", "Get pageable memory access type test", False),
            (284, "TOOLS_FLUSH_REPLAY_EVENTS", "Tools flush replay events test", False),
            (285, "REGISTER_UNLOAD_STATE_BUFFER", "Register unload state buffer test", False),
            (286, "RB_TREE_DIRECTED", "Red-black tree directed test", False),
            (287, "RB_TREE_RANDOM", "Red-black tree random test", False),
            (288, "HOST_SANITY", "Host sanity test", True),
            (289, "VA_SPACE_MM_OR_CURRENT_RETAIN", "VA space MM or current retain test", False),
            (290, "GET_USER_SPACE_END_ADDRESS", "Get user space end address test", False),
            (291, "GET_CPU_CHUNK_ALLOC_SIZES", "Get CPU chunk allocation sizes test", False),
            (293, "VA_RANGE_INJECT_ADD_GPU_VA_SPACE_ERROR", "VA range inject add GPU VA space error", True),
            (294, "DESTROY_GPU_VA_SPACE_DELAY", "Destroy GPU VA space delay test", True),
            (295, "SEC2_SANITY", "SEC2 sanity test", True),
            (296, "CGROUP_ACCOUNTING_SUPPORTED", "CGroup accounting supported test", False),
            (298, "SPLIT_INVALIDATE_DELAY", "Split invalidate delay test", False),
            (299, "SEC2_CPU_GPU_ROUNDTRIP", "SEC2 CPU-GPU roundtrip test", True),
            (300, "CPU_CHUNK_API", "CPU chunk API test", False),
            (301, "FORCE_CPU_TO_CPU_COPY_WITH_CE", "Force CPU to CPU copy with CE test", True),
            (302, "VA_SPACE_ALLOW_MOVABLE_ALLOCATIONS", "VA space allow movable allocations", False),
            (303, "SKIP_MIGRATE_VMA", "Skip migrate VMA test", False),
        ]
        
        # 验证测试数量
        assert len(ALL_UVM_TESTS) == 97, f"❌ 错误：只有{len(ALL_UVM_TESTS)}个测试，应该是97个！"
        
        # 过滤测试
        if test_filter:
            tests_to_run = [(cmd_id, name, desc, gpu) for cmd_id, name, desc, gpu in ALL_UVM_TESTS 
                           if test_filter.lower() in name.lower()]
        else:
            tests_to_run = ALL_UVM_TESTS
        
        print(f"UVM测试运行器 - 正确初始化版本")
        print(f"=============================")
        print(f"✅ 使用UVM_INITIALIZE (0x30000001)进行正确初始化")
        print(f"✅ 包含所有 {len(ALL_UVM_TESTS)} 个测试用例")
        print(f"✅ 应该解决所有NV_ERR_ILLEGAL_ACTION错误")
        print()
        
        # 打开设备
        self.fd = os.open(self.device_path, os.O_RDWR)
        
        try:
            # 正确的UVM初始化
            uvm_initialized = self.initialize_uvm()
            if uvm_initialized:
                print("✅ UVM初始化成功，所有测试现在应该可以运行")
            else:
                print("❌ UVM初始化失败，测试可能仍然失败")
            print()
            
            # 运行测试
            stats = {"pass": 0, "fail": 0, "error": 0}
            start_time = time.time()
            
            print(f"开始运行 {len(tests_to_run)} 个测试...")
            print()
            
            for i, (cmd_id, test_name, description, requires_gpu) in enumerate(tests_to_run, 1):
                print(f"[{i:2}/{len(tests_to_run)}] {test_name:35} ", end="", flush=True)
                
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
            
            # 清理
            self.deinitialize_uvm()
            
            # 打印总结
            total = sum(stats.values())
            print()
            print("=" * 70)
            print("正确初始化版本测试结果")
            print("=" * 70)
            print(f"UVM初始化:    {'成功' if uvm_initialized else '失败'}")
            print(f"总测试数:     {total}")
            print(f"通过:         {stats['pass']}")
            print(f"失败:         {stats['fail']}")
            print(f"系统错误:     {stats['error']}")
            print(f"成功率:       {stats['pass']*100//total if total > 0 else 0}%")
            print(f"执行时间:     {end_time - start_time:.1f} 秒")
            
            # 与之前结果比较
            print()
            print("与之前结果比较:")
            print("- 之前成功率: 78% (76/97) [VA空间创建失败]")
            print(f"- 当前成功率: {stats['pass']*100//total if total > 0 else 0}%")
            
            if stats['pass'] > 76:
                improvement = stats['pass'] - 76
                print(f"- 🎉 改善了 {improvement} 个测试!")
                print("- ✅ UVM_INITIALIZE确实是正确的方法!")
            elif stats['pass'] == 76:
                print("- ⚠️ 成功率相同，可能还需要其他步骤")
            else:
                print("- ❌ 成功率下降，需要调查原因")
            
            return stats['error'] == 0
            
        finally:
            if self.fd:
                os.close(self.fd)

def test_uvm_initialize_only():
    """只测试UVM_INITIALIZE是否工作"""
    print("UVM_INITIALIZE测试")
    print("==================")
    print()
    
    device_path = "/dev/nvidia-uvm"
    
    if not os.path.exists(device_path):
        print(f"❌ 设备不存在: {device_path}")
        return
    
    try:
        fd = os.open(device_path, os.O_RDWR)
        
        print("测试UVM_INITIALIZE (0x30000001)...")
        
        # UVM_INITIALIZE_PARAMS: flags (8字节) + rmStatus (4字节)
        params = array.array('B', [0] * 16)
        struct.pack_into('<Q', params, 0, 0)  # flags = 0
        
        try:
            ioctl_result = fcntl.ioctl(fd, 0x30000001, params)
            rm_status = struct.unpack('<I', params[8:12])[0]
            
            print(f"ioctl返回值: {ioctl_result}")
            print(f"rmStatus: {rm_status} ({get_nv_status_name(rm_status)})")
            
            if rm_status == 0:
                print("✅ UVM_INITIALIZE成功!")
                
                # 测试之前失败的RNG_SANITY
                print("\n测试RNG_SANITY是否现在能工作...")
                test_params = array.array('B', [0] * 8)
                test_result = fcntl.ioctl(fd, 201, test_params)
                test_rm_status = struct.unpack('<I', test_params[0:4])[0]
                
                print(f"RNG_SANITY rmStatus: {test_rm_status} ({get_nv_status_name(test_rm_status)})")
                
                if test_rm_status == 0:
                    print("🎉 RNG_SANITY现在成功了!")
                elif test_rm_status == 0x16:
                    print("❌ 仍然是NV_ERR_ILLEGAL_ACTION")
                else:
                    print(f"🔄 错误类型改变了: {get_nv_status_name(test_rm_status)}")
                
            else:
                print(f"❌ UVM_INITIALIZE失败: {get_nv_status_name(rm_status)}")
                
        except Exception as e:
            print(f"❌ UVM_INITIALIZE异常: {e}")
        
        os.close(fd)
        
    except Exception as e:
        print(f"❌ 设备访问失败: {e}")

def main():
    parser = argparse.ArgumentParser(description="UVM Test Runner with Correct Initialization")
    parser.add_argument("-v", "--verbose", action="store_true", help="详细输出")
    parser.add_argument("-c", "--continue", action="store_true", help="错误后继续")
    parser.add_argument("-t", "--test", help="运行指定测试")
    parser.add_argument("-f", "--filter", help="过滤测试名称")
    parser.add_argument("--test-init-only", action="store_true", help="只测试UVM_INITIALIZE")
    
    args = parser.parse_args()
    
    if args.test_init_only:
        test_uvm_initialize_only()
        return
    
    if os.geteuid() != 0:
        print("建议以root身份运行: sudo python3", sys.argv[0])
        print()
    
    runner = UVMTestRunnerCorrect()
    runner.verbose = args.verbose
    
    # 运行测试
    success = runner.run_all_tests(args.filter, getattr(args, 'continue'))
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()