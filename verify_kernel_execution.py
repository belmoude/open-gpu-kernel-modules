#!/usr/bin/env python3
"""
验证UVM测试是否真的执行到内核中
通过多种方法验证内核执行情况
"""

import os
import sys
import fcntl
import array
import struct
import time
import subprocess

def test_kernel_execution_verification():
    """验证测试是否真的到达内核"""
    print("UVM内核执行验证工具")
    print("===================")
    print()
    print("🔍 您的分析完全正确！让我们验证测试是否真的执行到了内核中...")
    print()
    
    device_path = "/dev/nvidia-uvm"
    
    if not os.path.exists(device_path):
        print(f"❌ 设备文件不存在: {device_path}")
        return False
    
    # 测试用例：应该失败的参数组合
    failing_tests = [
        {
            "name": "RANGE_TREE_RANDOM",
            "cmd_id": 203,
            "reason": "max_batch_count=0 应该返回 NV_ERR_INVALID_PARAMETER",
            "expected_error": 22,  # NV_ERR_INVALID_PARAMETER 通常映射到 EINVAL
        }
    ]
    
    # 测试用例：应该成功的简单测试
    simple_tests = [
        {
            "name": "RNG_SANITY", 
            "cmd_id": 201,
            "reason": "纯功能测试，不依赖参数"
        },
        {
            "name": "GET_USER_SPACE_END_ADDRESS",
            "cmd_id": 290, 
            "reason": "只返回系统信息"
        }
    ]
    
    try:
        fd = os.open(device_path, os.O_RDWR)
        
        print("=== 测试1: 验证应该失败的测试 ===")
        
        for test in failing_tests:
            print(f"\n🧪 测试: {test['name']} (ID: {test['cmd_id']})")
            print(f"   预期: {test['reason']}")
            
            try:
                params = array.array('B', [0] * 1024)
                result = fcntl.ioctl(fd, test['cmd_id'], params)
                
                print(f"   结果: ❌ 意外成功 (返回值: {result})")
                print(f"   🚨 这证明测试可能没有真正执行到内核验证代码！")
                
            except OSError as e:
                print(f"   结果: ✅ 预期失败 (错误码: {e.errno})")
                if e.errno == test.get('expected_error', 22):
                    print(f"   ✅ 错误码符合预期，说明确实执行到了内核")
                else:
                    print(f"   ⚠️  错误码不符合预期，可能是其他原因")
        
        print(f"\n=== 测试2: 验证应该成功的测试 ===")
        
        for test in simple_tests:
            print(f"\n🧪 测试: {test['name']} (ID: {test['cmd_id']})")
            print(f"   预期: {test['reason']}")
            
            try:
                params = array.array('B', [0] * 1024)
                result = fcntl.ioctl(fd, test['cmd_id'], params)
                print(f"   结果: ✅ 成功 (返回值: {result})")
                
                # 检查输出参数是否被修改
                non_zero_bytes = [i for i, b in enumerate(params[:32]) if b != 0]
                if non_zero_bytes:
                    print(f"   📊 输出参数被修改，位置: {non_zero_bytes[:5]}...")
                    print(f"   ✅ 这证明确实执行到了内核并返回了数据")
                else:
                    print(f"   ⚠️  输出参数未被修改，可能只是简单返回成功")
                    
            except OSError as e:
                print(f"   结果: ❌ 意外失败 (错误码: {e.errno})")
        
        print(f"\n=== 测试3: 系统调用跟踪验证 ===")
        print("使用strace验证ioctl系统调用...")
        
        # 创建一个简单的测试脚本
        test_script = "/tmp/simple_uvm_test.py"
        with open(test_script, 'w') as f:
            f.write("""#!/usr/bin/env python3
import os, fcntl, array
fd = os.open('/dev/nvidia-uvm', os.O_RDWR)
try:
    params = array.array('B', [0] * 1024)
    result = fcntl.ioctl(fd, 203, params)  # RANGE_TREE_RANDOM
    print(f"结果: {result}")
except Exception as e:
    print(f"错误: {e}")
finally:
    os.close(fd)
""")
        
        os.chmod(test_script, 0o755)
        
        # 使用strace跟踪系统调用
        try:
            cmd = ["strace", "-e", "ioctl", "python3", test_script]
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
            
            print("strace输出:")
            if "ioctl" in result.stderr:
                print("✅ 确实发生了ioctl系统调用")
                # 查找ioctl调用的详细信息
                for line in result.stderr.split('\n'):
                    if 'ioctl' in line and '203' in line:
                        print(f"   详细: {line.strip()}")
            else:
                print("❌ 没有检测到ioctl系统调用")
                
            print("程序输出:")
            print(result.stdout)
            
        except subprocess.TimeoutExpired:
            print("⚠️ strace执行超时")
        except FileNotFoundError:
            print("⚠️ strace命令不可用")
        finally:
            os.unlink(test_script)
        
        os.close(fd)
        
    except Exception as e:
        print(f"❌ 测试执行失败: {e}")
        return False
    
    print(f"\n=== 测试4: 内核日志验证 ===")
    print("检查是否有相关的内核日志...")
    
    try:
        # 执行一个测试前记录dmesg行数
        before_lines = len(subprocess.run(["dmesg"], capture_output=True, text=True).stdout.split('\n'))
        
        # 执行测试
        fd = os.open(device_path, os.O_RDWR)
        try:
            params = array.array('B', [0] * 1024)
            fcntl.ioctl(fd, 203, params)  # RANGE_TREE_RANDOM
        except:
            pass
        finally:
            os.close(fd)
        
        time.sleep(0.5)  # 等待内核日志
        
        # 检查新的内核日志
        after_output = subprocess.run(["dmesg"], capture_output=True, text=True).stdout
        after_lines = len(after_output.split('\n'))
        
        if after_lines > before_lines:
            new_lines = after_output.split('\n')[before_lines:]
            uvm_lines = [line for line in new_lines if 'uvm' in line.lower()]
            if uvm_lines:
                print("✅ 发现UVM相关的内核日志:")
                for line in uvm_lines[-3:]:  # 显示最后3条
                    print(f"   {line}")
            else:
                print("⚠️ 有新的内核日志，但不是UVM相关")
        else:
            print("❌ 没有新的内核日志产生")
            
    except Exception as e:
        print(f"⚠️ 内核日志检查失败: {e}")
    
    return True

def analyze_results():
    """分析可能的原因"""
    print(f"\n" + "="*60)
    print("🔍 可能的原因分析")
    print("="*60)
    
    print("""
如果RANGE_TREE_RANDOM测试显示成功但应该失败，可能的原因：

1. 🎭 **ioctl路由问题**:
   - ioctl可能没有正确路由到内核测试函数
   - 可能被其他处理程序拦截
   - 命令ID映射可能不正确

2. 🔄 **参数传递问题**:
   - 用户空间到内核空间的参数拷贝可能有问题
   - 参数结构大小不匹配
   - 字节序或对齐问题

3. 🚫 **测试条件不满足**:
   - 测试可能在某些条件下直接返回成功
   - 可能有前置条件检查失败时的快速返回路径
   - 编译时可能禁用了某些检查

4. 🔧 **驱动版本问题**:
   - 您的驱动版本可能与源码不完全一致
   - 可能是发布版本，去除了一些调试检查
   - 参数验证可能在不同版本中有所不同

5. 💡 **测试框架设计**:
   - 可能有测试模式开关
   - 某些验证可能只在特定条件下启用
   - 可能有兼容性处理逻辑
""")
    
    print("🎯 验证建议:")
    print("-" * 30)
    print("""
1. 检查驱动版本和源码版本是否匹配
2. 确认uvm_enable_builtin_tests参数确实生效
3. 使用strace详细跟踪系统调用
4. 检查内核日志中的详细错误信息
5. 尝试其他明确应该失败的测试用例
""")

if __name__ == "__main__":
    if os.geteuid() != 0:
        print("建议以root身份运行获得完整功能: sudo python3", sys.argv[0])
        print()
    
    success = test_kernel_execution_verification()
    if success:
        analyze_results()