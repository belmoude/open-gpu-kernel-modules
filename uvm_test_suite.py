#!/usr/bin/env python3
"""
uvm_test_suite.py - NVIDIA UVM测试套件 (Python版本)

这个脚本提供了比C程序更友好的测试界面，包括：
- 彩色输出
- 详细的错误信息
- 测试结果保存
- 可选择性运行测试

使用方法:
    sudo python3 uvm_test_suite.py                    # 运行所有测试
    sudo python3 uvm_test_suite.py --list             # 列出所有测试
    sudo python3 uvm_test_suite.py --test memory      # 运行内存相关测试
    sudo python3 uvm_test_suite.py --test conf        # 运行机密计算测试
"""

import os
import sys
import fcntl
import argparse
import json
import time
from datetime import datetime
from enum import IntEnum

# ANSI颜色代码
class Colors:
    RED = '\033[91m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    MAGENTA = '\033[95m'
    CYAN = '\033[96m'
    WHITE = '\033[97m'
    BOLD = '\033[1m'
    RESET = '\033[0m'

# UVM IOCTL定义 - Linux下就是简单的数字
UVM_IOCTL_BASE = lambda i: i
UVM_TEST_IOCTL_BASE = lambda i: UVM_IOCTL_BASE(200 + i)

class UVMTestCmds(IntEnum):
    RNG_SANITY = 201                # UVM_TEST_IOCTL_BASE(1)
    RANGE_TREE_DIRECTED = 202       # UVM_TEST_IOCTL_BASE(2)
    RANGE_TREE_RANDOM = 203         # UVM_TEST_IOCTL_BASE(3)
    RM_MEM_SANITY = 205             # UVM_TEST_IOCTL_BASE(5)
    GPU_SEMAPHORE_SANITY = 206      # UVM_TEST_IOCTL_BASE(6)
    TRACKER_SANITY = 212            # UVM_TEST_IOCTL_BASE(12)
    PUSH_SANITY = 213               # UVM_TEST_IOCTL_BASE(13)
    CHANNEL_SANITY = 214            # UVM_TEST_IOCTL_BASE(14)
    CE_SANITY = 216                 # UVM_TEST_IOCTL_BASE(16)
    LOCK_SANITY = 218               # UVM_TEST_IOCTL_BASE(18)
    PERF_UTILS_SANITY = 219         # UVM_TEST_IOCTL_BASE(19)
    KVMALLOC = 220                  # UVM_TEST_IOCTL_BASE(20)
    PERF_EVENTS_SANITY = 223        # UVM_TEST_IOCTL_BASE(23)
    PERF_MODULE_SANITY = 224        # UVM_TEST_IOCTL_BASE(24)
    RANGE_ALLOCATOR_SANITY = 225    # UVM_TEST_IOCTL_BASE(25)
    FAULT_BUFFER_FLUSH = 227        # UVM_TEST_IOCTL_BASE(27)
    SEC2_SANITY = 295               # UVM_TEST_IOCTL_BASE(95)
    SEC2_CPU_GPU_ROUNDTRIP = 299    # UVM_TEST_IOCTL_BASE(99)

# 测试用例定义
TEST_CASES = {
    # 基础数据结构测试
    'basic': [
        (UVMTestCmds.RNG_SANITY, "RNG_SANITY", "随机数生成器完整性测试"),
        (UVMTestCmds.RANGE_TREE_DIRECTED, "RANGE_TREE_DIRECTED", "范围树有向测试"),
        (UVMTestCmds.RANGE_ALLOCATOR_SANITY, "RANGE_ALLOCATOR_SANITY", "范围分配器完整性测试"),
        (UVMTestCmds.LOCK_SANITY, "LOCK_SANITY", "锁机制完整性测试"),
    ],
    
    # 内存管理测试
    'memory': [
        (UVMTestCmds.RM_MEM_SANITY, "RM_MEM_SANITY", "RM内存管理完整性测试"),
        (UVMTestCmds.KVMALLOC, "KVMALLOC", "内核内存分配测试"),
    ],
    
    # GPU硬件测试
    'gpu': [
        (UVMTestCmds.GPU_SEMAPHORE_SANITY, "GPU_SEMAPHORE_SANITY", "GPU信号量完整性测试"),
        (UVMTestCmds.CHANNEL_SANITY, "CHANNEL_SANITY", "GPU通道完整性测试"),
        (UVMTestCmds.CE_SANITY, "CE_SANITY", "拷贝引擎完整性测试"),
        (UVMTestCmds.FAULT_BUFFER_FLUSH, "FAULT_BUFFER_FLUSH", "故障缓冲区刷新测试"),
    ],
    
    # 同步和跟踪测试
    'sync': [
        (UVMTestCmds.TRACKER_SANITY, "TRACKER_SANITY", "跟踪器完整性测试"),
        (UVMTestCmds.PUSH_SANITY, "PUSH_SANITY", "Push机制完整性测试"),
    ],
    
    # 性能测试
    'perf': [
        (UVMTestCmds.PERF_UTILS_SANITY, "PERF_UTILS_SANITY", "性能工具完整性测试"),
        (UVMTestCmds.PERF_EVENTS_SANITY, "PERF_EVENTS_SANITY", "性能事件完整性测试"),
        (UVMTestCmds.PERF_MODULE_SANITY, "PERF_MODULE_SANITY", "性能模块完整性测试"),
    ],
    
    # 机密计算测试
    'conf': [
        (UVMTestCmds.SEC2_SANITY, "SEC2_SANITY", "SEC2引擎完整性测试（机密计算）"),
        (UVMTestCmds.SEC2_CPU_GPU_ROUNDTRIP, "SEC2_CPU_GPU_ROUNDTRIP", "SEC2 CPU-GPU往返测试（机密计算）"),
    ],
}

class UVMTestSuite:
    def __init__(self):
        self.uvm_fd = None
        self.results = []
        self.start_time = None
        
    def print_colored(self, text, color=Colors.WHITE):
        """打印彩色文本"""
        print(f"{color}{text}{Colors.RESET}")
        
    def check_environment(self):
        """检查测试环境"""
        self.print_colored("=== 检查UVM测试环境 ===", Colors.CYAN)
        
        # 检查UVM设备
        if not os.path.exists("/dev/nvidia-uvm"):
            self.print_colored("❌ /dev/nvidia-uvm 设备未找到", Colors.RED)
            self.print_colored("   请确保NVIDIA UVM驱动已加载", Colors.YELLOW)
            return False
        self.print_colored("✅ UVM设备文件存在", Colors.GREEN)
        
        # 检查模块参数
        param_file = "/sys/module/nvidia_uvm/parameters/uvm_enable_builtin_tests"
        try:
            with open(param_file, 'r') as f:
                if f.read().strip() == '1':
                    self.print_colored("✅ UVM内置测试已启用", Colors.GREEN)
                    return True
                else:
                    self.print_colored("❌ UVM内置测试未启用", Colors.RED)
                    self.print_colored("   运行: sudo modprobe nvidia-uvm uvm_enable_builtin_tests=1", Colors.YELLOW)
                    return False
        except FileNotFoundError:
            self.print_colored("❌ UVM模块未加载", Colors.RED)
            return False
            
    def open_uvm_device(self):
        """打开UVM设备"""
        try:
            self.uvm_fd = os.open("/dev/nvidia-uvm", os.O_RDWR)
            self.print_colored("✅ UVM设备打开成功", Colors.GREEN)
            return True
        except OSError as e:
            self.print_colored(f"❌ 打开UVM设备失败: {e}", Colors.RED)
            return False
    
    def run_single_test(self, ioctl_cmd, test_name, description):
        """运行单个测试"""
        # 创建参数结构（简化为4字节状态）
        params = bytearray(4)  # 简单的rmStatus字段
        
        try:
            # 执行IOCTL
            fcntl.ioctl(self.uvm_fd, ioctl_cmd, params, True)
            
            # 解析返回状态
            status = int.from_bytes(params[:4], byteorder='little')
            
            if status == 0:  # NV_OK
                return True, None
            else:
                return False, f"status=0x{status:x}"
                
        except OSError as e:
            return False, f"ioctl error: {e}"
    
    def run_test_category(self, category):
        """运行指定类别的测试"""
        if category not in TEST_CASES:
            self.print_colored(f"❌ 未知的测试类别: {category}", Colors.RED)
            return False
            
        tests = TEST_CASES[category]
        self.print_colored(f"\n=== 运行 {category.upper()} 测试 ({len(tests)} 个测试) ===", Colors.CYAN)
        
        category_passed = 0
        
        for ioctl_cmd, test_name, description in tests:
            print(f"  {test_name:25} ... ", end="")
            sys.stdout.flush()
            
            success, error = self.run_single_test(ioctl_cmd, test_name, description)
            
            if success:
                self.print_colored("PASSED", Colors.GREEN)
                category_passed += 1
            else:
                self.print_colored(f"FAILED ({error})", Colors.RED)
            
            self.results.append({
                'category': category,
                'name': test_name,
                'description': description,
                'success': success,
                'error': error,
                'timestamp': datetime.now().isoformat()
            })
        
        success_rate = 100.0 * category_passed / len(tests)
        color = Colors.GREEN if category_passed == len(tests) else Colors.YELLOW
        self.print_colored(f"类别 {category}: {category_passed}/{len(tests)} 通过 ({success_rate:.1f}%)", color)
        
        return category_passed == len(tests)
    
    def run_all_tests(self):
        """运行所有测试"""
        self.print_colored("\n=== 运行所有UVM测试 ===", Colors.CYAN)
        
        total_passed = 0
        total_tests = 0
        
        for category in TEST_CASES.keys():
            success = self.run_test_category(category)
            category_tests = len(TEST_CASES[category])
            total_tests += category_tests
            if success:
                total_passed += category_tests
            else:
                total_passed += sum(1 for r in self.results[-category_tests:] if r['success'])
        
        return total_passed, total_tests
    
    def save_results(self, filename="uvm_test_results.json"):
        """保存测试结果到JSON文件"""
        results_data = {
            'timestamp': datetime.now().isoformat(),
            'total_tests': len(self.results),
            'passed_tests': sum(1 for r in self.results if r['success']),
            'test_duration': time.time() - self.start_time if self.start_time else 0,
            'results': self.results
        }
        
        try:
            with open(filename, 'w') as f:
                json.dump(results_data, f, indent=2)
            self.print_colored(f"✅ 测试结果已保存到: {filename}", Colors.GREEN)
        except Exception as e:
            self.print_colored(f"⚠️  保存测试结果失败: {e}", Colors.YELLOW)
    
    def print_summary(self, passed, total):
        """打印测试总结"""
        self.print_colored("\n" + "="*50, Colors.CYAN)
        self.print_colored("           UVM测试结果总结", Colors.BOLD)
        self.print_colored("="*50, Colors.CYAN)
        
        print(f"总测试数:   {total}")
        print(f"通过测试:   {Colors.GREEN}{passed}{Colors.RESET}")
        print(f"失败测试:   {Colors.RED}{total - passed}{Colors.RESET}")
        
        success_rate = 100.0 * passed / total if total > 0 else 0
        color = Colors.GREEN if passed == total else Colors.YELLOW
        print(f"成功率:     {color}{success_rate:.1f}%{Colors.RESET}")
        
        if self.start_time:
            duration = time.time() - self.start_time
            print(f"测试耗时:   {duration:.2f} 秒")
        
        if passed == total:
            self.print_colored("\n🎉 所有测试通过！", Colors.GREEN)
        else:
            self.print_colored(f"\n⚠️  {total - passed} 个测试失败", Colors.YELLOW)
            self.print_colored("这可能是正常的，如果：", Colors.YELLOW)
            print("   - 某些GPU功能不可用")
            print("   - 机密计算不支持")
            print("   - 运行在虚拟环境中")
    
    def list_tests(self):
        """列出所有可用测试"""
        self.print_colored("=== 可用的UVM测试 ===", Colors.CYAN)
        
        for category, tests in TEST_CASES.items():
            self.print_colored(f"\n{category.upper()} 测试:", Colors.BOLD)
            for _, test_name, description in tests:
                print(f"  {test_name:25} - {description}")
        
        print(f"\n总计: {sum(len(tests) for tests in TEST_CASES.values())} 个测试")
    
    def run(self, test_category=None):
        """运行测试套件"""
        self.start_time = time.time()
        
        try:
            # 检查环境
            if not self.check_environment():
                return False
                
            # 打开UVM设备
            if not self.open_uvm_device():
                return False
            
            # 运行测试
            if test_category:
                if test_category not in TEST_CASES:
                    self.print_colored(f"❌ 未知测试类别: {test_category}", Colors.RED)
                    self.print_colored("可用类别: " + ", ".join(TEST_CASES.keys()), Colors.YELLOW)
                    return False
                    
                success = self.run_test_category(test_category)
                passed = sum(1 for r in self.results if r['success'])
                total = len(self.results)
            else:
                passed, total = self.run_all_tests()
                success = (passed == total)
            
            # 打印总结
            self.print_summary(passed, total)
            
            # 保存结果
            self.save_results()
            
            return success
            
        except KeyboardInterrupt:
            self.print_colored("\n\n⚠️  测试被用户中断", Colors.YELLOW)
            return False
        except Exception as e:
            self.print_colored(f"\n❌ 测试过程中发生错误: {e}", Colors.RED)
            return False
        finally:
            if self.uvm_fd is not None:
                os.close(self.uvm_fd)

def main():
    parser = argparse.ArgumentParser(
        description="NVIDIA UVM测试套件",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  %(prog)s                    # 运行所有测试
  %(prog)s --list             # 列出所有测试
  %(prog)s --test memory      # 运行内存相关测试
  %(prog)s --test conf        # 运行机密计算测试
  %(prog)s --test basic       # 运行基础测试
        """
    )
    
    parser.add_argument('--list', action='store_true', 
                       help='列出所有可用测试')
    parser.add_argument('--test', metavar='CATEGORY',
                       help='运行指定类别的测试 (basic/memory/gpu/sync/perf/conf)')
    parser.add_argument('--verbose', '-v', action='store_true',
                       help='详细输出')
    
    args = parser.parse_args()
    
    # 检查root权限
    if os.geteuid() != 0:
        print(f"{Colors.RED}❌ 此脚本需要root权限{Colors.RESET}")
        print(f"{Colors.YELLOW}   运行: sudo python3 {sys.argv[0]}{Colors.RESET}")
        sys.exit(1)
    
    runner = UVMTestSuite()
    
    if args.list:
        runner.list_tests()
        sys.exit(0)
    
    # 运行测试
    success = runner.run(args.test)
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()