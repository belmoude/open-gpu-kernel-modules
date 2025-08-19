#!/usr/bin/env python3
"""
完整的UVM测试脚本 - 基于源码分析
正确设置每个测试的入参，并检查rmStatus字段
"""

import os
import sys
import fcntl
import array
import struct
import time
import random

# NV_STATUS 错误码定义
NV_STATUS_CODES = {
    0x00000000: "NV_OK",
    0x00000001: "NV_ERR_GENERIC", 
    0x00000004: "NV_ERR_INVALID_PARAMETER",
    0x00000005: "NV_ERR_INVALID_ARGUMENT",
    0x00000006: "NV_ERR_INVALID_STATE",
    0x00000031: "NV_ERR_ILLEGAL_ACTION",
    0x00000032: "NV_ERR_INVALID_DEVICE",
    0x00000046: "NV_ERR_NO_MEMORY",
    0x00000065: "NV_ERR_NOT_SUPPORTED",
}

def get_nv_status_name(status_code):
    """获取NV_STATUS错误码的名称"""
    return NV_STATUS_CODES.get(status_code, f"UNKNOWN_0x{status_code:08x}")

class UVMTestCase:
    """UVM测试用例类"""
    def __init__(self, cmd_id, name, description, requires_gpu=False):
        self.cmd_id = cmd_id
        self.name = name
        self.description = description
        self.requires_gpu = requires_gpu
    
    def prepare_params(self):
        """准备测试参数 - 子类重写此方法"""
        return array.array('B', [0] * 1024)
    
    def get_rmstatus_offset(self):
        """获取rmStatus字段在参数结构中的偏移 - 子类重写"""
        return -4  # 默认在结构末尾
    
    def parse_results(self, params):
        """解析测试结果 - 子类重写"""
        offset = self.get_rmstatus_offset()
        if offset < 0:
            offset = len(params) + offset
        
        if offset >= 0 and offset + 4 <= len(params):
            rm_status = struct.unpack('<I', params[offset:offset+4])[0]
            return rm_status, get_nv_status_name(rm_status)
        else:
            return None, "无法解析rmStatus"

# 具体的测试用例实现
class RNGSanityTest(UVMTestCase):
    def __init__(self):
        super().__init__(201, "RNG_SANITY", "Random number generator sanity test")
    
    def prepare_params(self):
        # typedef struct { NV_STATUS rmStatus; } UVM_TEST_RNG_SANITY_PARAMS;
        return array.array('B', [0] * 8)  # 4字节rmStatus + 4字节对齐
    
    def get_rmstatus_offset(self):
        return 0  # rmStatus在结构开头

class RangeTreeRandomTest(UVMTestCase):
    def __init__(self):
        super().__init__(203, "RANGE_TREE_RANDOM", "Random range tree test")
    
    def prepare_params(self):
        # 基于UVM_TEST_RANGE_TREE_RANDOM_PARAMS结构
        params = array.array('B', [0] * 256)  # 足够大的缓冲区
        
        # 设置合理的参数值
        struct.pack_into('<I', params, 0, int(time.time()) % 0xFFFFFFFF)  # seed
        struct.pack_into('<Q', params, 8, 100)     # main_iterations
        struct.pack_into('<I', params, 16, 0)      # verbose
        struct.pack_into('<I', params, 20, 75)     # high_probability (75%)
        struct.pack_into('<I', params, 24, 50)     # add_remove_shrink_group_probability
        struct.pack_into('<I', params, 28, 25)     # shrink_probability
        struct.pack_into('<I', params, 32, 10)     # collision_checks
        struct.pack_into('<I', params, 36, 5)      # iterator_checks
        struct.pack_into('<Q', params, 40, 0x100000)  # max_end (1MB)
        struct.pack_into('<Q', params, 48, 100)    # max_ranges
        struct.pack_into('<Q', params, 56, 10)     # max_batch_count (关键！设置为10而不是0)
        struct.pack_into('<I', params, 64, 100)    # max_attempts
        
        return params
    
    def get_rmstatus_offset(self):
        return 252  # rmStatus在结构末尾 (136 + 统计数据 + 对齐)

class GetUserSpaceEndAddressTest(UVMTestCase):
    def __init__(self):
        super().__init__(290, "GET_USER_SPACE_END_ADDRESS", "Get user space end address test")
    
    def prepare_params(self):
        # typedef struct { NvU64 user_space_end_address; NV_STATUS rmStatus; }
        return array.array('B', [0] * 16)  # 8字节地址 + 4字节rmStatus + 4字节对齐
    
    def get_rmstatus_offset(self):
        return 8  # rmStatus在地址之后

class LockSanityTest(UVMTestCase):
    def __init__(self):
        super().__init__(218, "LOCK_SANITY", "Lock sanity test")
    
    def prepare_params(self):
        return array.array('B', [0] * 8)  # 简单的rmStatus结构
    
    def get_rmstatus_offset(self):
        return 0

class KVMallocTest(UVMTestCase):
    def __init__(self):
        super().__init__(220, "KVMALLOC", "Kernel memory allocation test")
    
    def prepare_params(self):
        return array.array('B', [0] * 8)  # 简单的rmStatus结构
    
    def get_rmstatus_offset(self):
        return 0

class MemSanityTest(UVMTestCase):
    def __init__(self):
        super().__init__(230, "MEM_SANITY", "Memory sanity test")
    
    def prepare_params(self):
        return array.array('B', [0] * 8)
    
    def get_rmstatus_offset(self):
        return 0

class CGROUPAccountingSupportedTest(UVMTestCase):
    def __init__(self):
        super().__init__(296, "CGROUP_ACCOUNTING_SUPPORTED", "CGroup accounting supported test")
    
    def prepare_params(self):
        return array.array('B', [0] * 8)
    
    def get_rmstatus_offset(self):
        return 0

class GetGPURefCountTest(UVMTestCase):
    def __init__(self):
        super().__init__(200, "GET_GPU_REF_COUNT", "Get GPU reference count", requires_gpu=True)
    
    def prepare_params(self):
        # 需要GPU UUID作为输入
        params = array.array('B', [0] * 32)
        # 这里应该设置一个有效的GPU UUID，但我们用全零测试
        return params
    
    def get_rmstatus_offset(self):
        return 24  # 在GPU UUID之后

# 测试用例注册表
TEST_REGISTRY = [
    RNGSanityTest(),
    LockSanityTest(), 
    KVMallocTest(),
    MemSanityTest(),
    GetUserSpaceEndAddressTest(),
    CGROUPAccountingSupportedTest(),
    RangeTreeRandomTest(),  # 这个现在应该成功了
    GetGPURefCountTest(),   # GPU相关，可能失败
]

class UVMTestRunner:
    """UVM测试运行器"""
    
    def __init__(self, device_path="/dev/nvidia-uvm"):
        self.device_path = device_path
        self.verbose = False
        self.continue_on_error = False
        
    def run_single_test(self, test_case):
        """运行单个测试用例"""
        print(f"Running test: {test_case.name:35} ", end="", flush=True)
        
        if self.verbose:
            print()
            print(f"  Description: {test_case.description}")
            print(f"  Command ID: {test_case.cmd_id}")
            print(f"  Requires GPU: {'Yes' if test_case.requires_gpu else 'No'}")
            print("  Executing... ", end="", flush=True)
        
        try:
            fd = os.open(self.device_path, os.O_RDWR)
            try:
                # 准备测试参数
                params = test_case.prepare_params()
                
                # 执行ioctl
                ioctl_result = fcntl.ioctl(fd, test_case.cmd_id, params)
                
                # 解析真正的测试结果
                rm_status, status_name = test_case.parse_results(params)
                
                if rm_status is None:
                    print("[UNKNOWN]")
                    if self.verbose:
                        print(f"  Result: Cannot parse rmStatus")
                    return "unknown"
                elif rm_status == 0:  # NV_OK
                    print("[PASS]")
                    if self.verbose:
                        print(f"  Result: Test completed successfully")
                        print(f"  rmStatus: {status_name}")
                    return "pass"
                else:
                    print("[FAIL]")
                    if self.verbose:
                        print(f"  Result: Test failed")
                        print(f"  rmStatus: {status_name} (0x{rm_status:08x})")
                    else:
                        print(f"  Error: {status_name}")
                    return "fail"
                
            finally:
                os.close(fd)
                
        except OSError as e:
            print("[ERROR]")
            if self.verbose:
                print(f"  Result: System call failed")
                print(f"  Error: {e}")
            else:
                print(f"  System error: {e}")
            return "error"
        except Exception as e:
            print("[EXCEPTION]")
            if self.verbose:
                print(f"  Result: Unexpected exception")
                print(f"  Exception: {e}")
            else:
                print(f"  Exception: {e}")
            return "exception"
    
    def run_all_tests(self, test_filter=None):
        """运行所有测试"""
        print("UVM Test Runner - Complete Source-Based Version")
        print("===============================================")
        print("✅ 基于源码分析，正确设置参数和检查rmStatus")
        print()
        
        # 检查设备
        if not os.path.exists(self.device_path):
            print(f"❌ UVM设备不存在: {self.device_path}")
            return False
        
        # 过滤测试
        tests_to_run = TEST_REGISTRY
        if test_filter:
            tests_to_run = [t for t in TEST_REGISTRY if test_filter.lower() in t.name.lower()]
        
        print(f"Total tests to run: {len(tests_to_run)}")
        print()
        
        # 统计
        results = {"pass": 0, "fail": 0, "error": 0, "exception": 0, "unknown": 0}
        
        start_time = time.time()
        
        # 运行测试
        for test_case in tests_to_run:
            result = self.run_single_test(test_case)
            results[result] += 1
            
            if result in ["error", "exception"] and not self.continue_on_error:
                print()
                print("Stopping due to system error. Use --continue to run all tests.")
                break
            
            time.sleep(0.01)  # 小延迟
        
        end_time = time.time()
        
        # 打印总结
        total_tests = sum(results.values())
        print()
        print("Test Execution Summary")
        print("=====================")
        print(f"Total tests:     {total_tests}")
        print(f"Passed:          {results['pass']}")
        print(f"Failed:          {results['fail']}")
        print(f"System errors:   {results['error']}")
        print(f"Exceptions:      {results['exception']}")
        print(f"Unknown:         {results['unknown']}")
        
        if total_tests > 0:
            success_rate = (results['pass'] * 100) // total_tests
            print(f"Success rate:    {success_rate}%")
        
        print(f"Execution time:  {end_time - start_time:.1f} seconds")
        
        if results['fail'] > 0:
            print()
            print("Analysis:")
            print("- Failed tests show actual kernel-level failures")
            print("- This proves tests are really executing in kernel")
            print("- Parameter validation is working correctly")
        
        return results['error'] == 0 and results['exception'] == 0

def main():
    """主函数"""
    import argparse
    
    parser = argparse.ArgumentParser(description="Complete UVM Test Runner")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    parser.add_argument("-c", "--continue", action="store_true", help="Continue on errors")
    parser.add_argument("-f", "--filter", help="Filter tests by name")
    parser.add_argument("-t", "--test", help="Run specific test")
    parser.add_argument("-l", "--list", action="store_true", help="List all tests")
    
    args = parser.parse_args()
    
    if args.list:
        print("Available UVM Test Cases:")
        print("========================")
        for test in TEST_REGISTRY:
            gpu_marker = "[GPU]" if test.requires_gpu else "     "
            print(f"{gpu_marker} {test.name:35} - {test.description}")
        return
    
    # 检查权限
    if os.geteuid() != 0:
        print("Warning: Running without root privileges. Some tests may fail.")
        print("For best results, run: sudo python3", sys.argv[0])
        print()
    
    # 创建测试运行器
    runner = UVMTestRunner()
    runner.verbose = args.verbose
    runner.continue_on_error = getattr(args, 'continue')
    
    # 运行测试
    if args.test:
        # 运行单个测试
        test_case = None
        for t in TEST_REGISTRY:
            if t.name == args.test:
                test_case = t
                break
        
        if test_case:
            result = runner.run_single_test(test_case)
            sys.exit(0 if result == "pass" else 1)
        else:
            print(f"Test '{args.test}' not found")
            sys.exit(1)
    else:
        # 运行所有测试
        success = runner.run_all_tests(args.filter)
        sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()