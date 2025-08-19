#!/usr/bin/env python3
"""
专门测试RANGE_TREE_RANDOM以验证是否真的执行到内核
"""

import os
import sys
import fcntl
import array
import struct

def test_range_tree_random():
    """测试RANGE_TREE_RANDOM是否真的执行到内核验证代码"""
    print("RANGE_TREE_RANDOM 验证测试")
    print("=========================")
    print()
    
    device_path = "/dev/nvidia-uvm"
    cmd_id = 203  # RANGE_TREE_RANDOM
    
    if not os.path.exists(device_path):
        print(f"❌ 设备文件不存在: {device_path}")
        return
    
    print("🔍 根据您的分析，RANGE_TREE_RANDOM测试应该在max_batch_count=0时失败")
    print("让我们验证是否真的执行到了内核验证代码...")
    print()
    
    try:
        fd = os.open(device_path, os.O_RDWR)
        
        # 测试1: 全零参数（应该失败）
        print("测试1: 全零参数")
        print("预期: 失败 (max_batch_count=0 应该返回 NV_ERR_INVALID_PARAMETER)")
        
        try:
            params = array.array('B', [0] * 1024)
            result = fcntl.ioctl(fd, cmd_id, params)
            print(f"结果: ❌ 意外成功 (返回值: {result})")
            print("🚨 这证明测试可能没有真正执行到内核验证代码！")
            
            # 检查返回的参数
            if any(params[:16]):
                hex_data = ' '.join(f'{b:02x}' for b in params[:16])
                print(f"返回数据: {hex_data}")
            else:
                print("参数未被修改")
                
        except OSError as e:
            print(f"结果: ✅ 预期失败 (错误码: {e.errno}, 消息: {e.strerror})")
            print("✅ 这证明确实执行到了内核验证代码")
        
        print()
        
        # 测试2: 设置合理的参数（应该成功）
        print("测试2: 设置合理的参数")
        print("预期: 成功 (max_batch_count=10, high_probability=50)")
        
        try:
            params = array.array('B', [0] * 1024)
            
            # 根据UVM_TEST_RANGE_TREE_RANDOM_PARAMS结构设置参数
            # 假设结构布局（需要根据实际结构调整）:
            # NvU32 seed;                    // offset 0
            # NvU64 main_iterations;         // offset 8  
            # NvU32 verbose;                 // offset 16
            # NvU32 high_probability;        // offset 20
            # ...
            # NvU32 max_batch_count;         // 需要找到正确的offset
            
            # 设置一些合理的值
            struct.pack_into('<I', params, 0, 12345)      # seed
            struct.pack_into('<Q', params, 8, 100)        # main_iterations  
            struct.pack_into('<I', params, 16, 0)         # verbose
            struct.pack_into('<I', params, 20, 50)        # high_probability (50%)
            
            # 尝试在不同位置设置max_batch_count
            for offset in [24, 28, 32, 36, 40]:
                test_params = params[:]
                struct.pack_into('<I', test_params, offset, 10)  # max_batch_count=10
                
                try:
                    result = fcntl.ioctl(fd, cmd_id, test_params)
                    print(f"offset {offset}: ✅ 成功 (返回值: {result})")
                    
                    # 检查返回的参数
                    if any(test_params[:32]):
                        non_zero = [(i, b) for i, b in enumerate(test_params[:32]) if b != 0]
                        print(f"  修改的字节: {non_zero[:5]}...")
                    break
                    
                except OSError as e:
                    print(f"offset {offset}: ❌ 失败 (错误: {e.errno})")
                    continue
            else:
                print("❌ 所有offset尝试都失败了")
                
        except Exception as e:
            print(f"结果: ❌ 测试异常: {e}")
        
        print()
        
        # 测试3: 使用strace跟踪系统调用
        print("测试3: 系统调用验证")
        print("检查ioctl系统调用是否真的发生...")
        
        import subprocess
        import tempfile
        
        # 创建临时测试脚本
        with tempfile.NamedTemporaryFile(mode='w', suffix='.py', delete=False) as f:
            f.write(f"""#!/usr/bin/env python3
import os, fcntl, array
fd = os.open('{device_path}', os.O_RDWR)
try:
    params = array.array('B', [0] * 1024)
    result = fcntl.ioctl(fd, {cmd_id}, params)
    print(f"ioctl结果: {{result}}")
except Exception as e:
    print(f"ioctl错误: {{e}}")
finally:
    os.close(fd)
""")
            temp_script = f.name
        
        try:
            # 使用strace跟踪
            cmd = ['strace', '-e', 'ioctl', 'python3', temp_script]
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
            
            print("strace输出 (stderr):")
            if result.stderr:
                for line in result.stderr.split('\n'):
                    if 'ioctl' in line:
                        print(f"  {line}")
            else:
                print("  无strace输出")
                
            print("程序输出 (stdout):")
            if result.stdout:
                print(f"  {result.stdout.strip()}")
            else:
                print("  无程序输出")
                
        except subprocess.TimeoutExpired:
            print("strace执行超时")
        except FileNotFoundError:
            print("strace命令不可用，跳过此测试")
        finally:
            os.unlink(temp_script)
        
        os.close(fd)
        
    except Exception as e:
        print(f"❌ 测试失败: {e}")
    
    print()
    print("🎯 结论:")
    print("如果测试1显示成功而不是失败，这强烈表明：")
    print("1. ioctl调用可能没有真正到达内核测试代码")
    print("2. 可能存在早期的成功返回路径")
    print("3. 参数验证可能在不同的代码路径中")
    print("4. 驱动版本可能与源码不匹配")

if __name__ == "__main__":
    if os.geteuid() != 0:
        print("建议以root身份运行: sudo python3", sys.argv[0])
        print()
    
    test_range_tree_random()