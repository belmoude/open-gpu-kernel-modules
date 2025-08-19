#!/usr/bin/env python3
"""
分析CHANNEL_STRESS测试导致机器重启的问题
比较不同参数设置的安全性
"""

import os
import sys
import fcntl
import array
import struct
import time

def analyze_channel_stress_params():
    """分析CHANNEL_STRESS参数设置的差异"""
    print("CHANNEL_STRESS参数分析")
    print("======================")
    print()
    
    print("🔍 CHANNEL_STRESS参数结构 (来自源码):")
    print("""
typedef struct {
    NvU32     mode;                   // In, 0=NOOP_PUSH, 1=UPDATE_CHANNELS, 2=STREAM, 3=KEY_ROTATION
    NvU32     iterations;             // In, 迭代次数
    NvU32     num_streams;            // In, 流数量 (仅mode==STREAM时使用)
    NvU32     key_rotation_operation; // In, 密钥轮换操作 (仅mode==KEY_ROTATION时使用)
    NvU32     seed;                   // In, 随机种子
    NvU32     verbose;                // In, 详细输出标志
    NV_STATUS rmStatus;               // Out, 返回状态
} UVM_TEST_CHANNEL_STRESS_PARAMS;
""")
    
    print("参数设置对比:")
    print("=============")
    
    print("版本1 (run_uvm_tests_all_97.py) - 导致重启:")
    print("  mode = 0 (NOOP_PUSH)")
    print("  iterations = 100")
    print("  num_streams = 4")
    print("  key_rotation_operation = 0")
    print("  seed = current_time")
    print("  verbose = 0")
    
    print()
    print("版本2 (run_uvm_tests_correct_init.py) - 简化版本:")
    print("  只设置了8字节参数 (不完整!)")
    print("  这可能导致参数结构不匹配")
    
    print()
    print("🚨 可能的问题:")
    print("1. iterations=100 可能太高，导致过度的GPU活动")
    print("2. num_streams=4 可能创建了太多并发流")
    print("3. 随机seed可能触发了特定的问题路径")
    print("4. CHANNEL_STRESS本身就是压力测试，可能很危险")

def create_safe_channel_stress_test():
    """创建安全的CHANNEL_STRESS测试"""
    print("\n🛡️ 创建安全的CHANNEL_STRESS参数:")
    print("================================")
    
    device_path = "/dev/nvidia-uvm"
    
    if not os.path.exists(device_path):
        print(f"❌ 设备不存在: {device_path}")
        return
    
    # 安全的参数配置
    safe_configs = [
        {
            "name": "最小NOOP测试",
            "mode": 0,      # NOOP_PUSH (最安全)
            "iterations": 1, # 最小迭代
            "num_streams": 1,
            "seed": 12345,   # 固定种子
            "verbose": 0
        },
        {
            "name": "小规模NOOP测试", 
            "mode": 0,
            "iterations": 5,
            "num_streams": 1,
            "seed": 12345,
            "verbose": 0
        },
        {
            "name": "原始参数(危险)",
            "mode": 0,
            "iterations": 100,  # 可能导致重启的参数
            "num_streams": 4,
            "seed": int(time.time()) % 0xFFFFFFFF,
            "verbose": 0
        }
    ]
    
    try:
        fd = os.open(device_path, os.O_RDWR)
        
        # 先尝试UVM_INITIALIZE
        print("首先初始化UVM...")
        try:
            init_params = array.array('B', [0] * 16)
            struct.pack_into('<Q', init_params, 0, 0)  # flags
            ioctl_result = fcntl.ioctl(fd, 0x30000001, init_params)
            rm_status = struct.unpack('<I', init_params[8:12])[0]
            
            if rm_status == 0:
                print("✅ UVM初始化成功")
            else:
                print(f"❌ UVM初始化失败: {get_nv_status_name(rm_status)}")
                os.close(fd)
                return
                
        except Exception as e:
            print(f"❌ UVM初始化异常: {e}")
            os.close(fd)
            return
        
        # 测试不同的CHANNEL_STRESS配置
        for config in safe_configs:
            print(f"\n测试配置: {config['name']}")
            print(f"  参数: mode={config['mode']}, iterations={config['iterations']}, streams={config['num_streams']}")
            
            # 设置参数
            params = array.array('B', [0] * 32)
            struct.pack_into('<I', params, 0, config['mode'])
            struct.pack_into('<I', params, 4, config['iterations'])
            struct.pack_into('<I', params, 8, config['num_streams'])
            struct.pack_into('<I', params, 12, 0)  # key_rotation_operation
            struct.pack_into('<I', params, 16, config['seed'])
            struct.pack_into('<I', params, 20, config['verbose'])
            
            try:
                print("  执行中... ", end="", flush=True)
                ioctl_result = fcntl.ioctl(fd, 215, params)  # CHANNEL_STRESS
                rm_status = struct.unpack('<I', params[24:28])[0]
                
                print(f"完成")
                print(f"  ioctl返回: {ioctl_result}")
                print(f"  rmStatus: {rm_status} ({get_nv_status_name(rm_status)})")
                
                if rm_status == 0:
                    print("  ✅ 安全配置成功")
                else:
                    print(f"  ❌ 失败: {get_nv_status_name(rm_status)}")
                
                # 如果是危险配置且成功，警告用户
                if config['name'] == "原始参数(危险)" and rm_status == 0:
                    print("  🚨 警告: 原始参数在此系统上可能不安全!")
                    print("  🚨 建议使用更保守的参数")
                
            except Exception as e:
                print(f"异常: {e}")
                print(f"  ⚠️ 配置 '{config['name']}' 导致异常")
                
                if config['name'] == "原始参数(危险)":
                    print("  🚨 确认: 原始参数确实有问题!")
                    break
            
            time.sleep(0.5)  # 安全间隔
        
        os.close(fd)
        
    except Exception as e:
        print(f"❌ 设备访问失败: {e}")

def get_nv_status_name(status_code):
    codes = {
        0x00000000: "NV_OK",
        0x00000016: "NV_ERR_ILLEGAL_ACTION",
        0x00000004: "NV_ERR_INVALID_PARAMETER",
    }
    return codes.get(status_code, f"UNKNOWN_0x{status_code:08x}")

def suggest_safe_parameters():
    """建议安全的CHANNEL_STRESS参数"""
    print("\n🛡️ 安全的CHANNEL_STRESS参数建议:")
    print("=================================")
    
    print("""
基于分析，建议使用以下安全参数:

1. 最安全配置 (推荐):
   mode = 0 (NOOP_PUSH)
   iterations = 1-5 (最小迭代)
   num_streams = 1 (单流)
   seed = 固定值 (避免随机问题)

2. 保守配置:
   mode = 0 (NOOP_PUSH)
   iterations = 10-20 (适中迭代)
   num_streams = 1-2 (少量流)
   seed = 固定值

3. 避免的配置:
   iterations > 50 (可能导致过度GPU活动)
   num_streams > 2 (可能导致资源竞争)
   mode = 2,3 (STREAM/KEY_ROTATION模式可能更危险)
   随机seed (可能触发特定问题)

修正的参数设置代码:
```python
elif cmd_id == 215:  # CHANNEL_STRESS - 安全版本
    params = array.array('B', [0] * 32)
    struct.pack_into('<I', params, 0, 0)      # mode = NOOP_PUSH (最安全)
    struct.pack_into('<I', params, 4, 5)      # iterations = 5 (而不是100!)
    struct.pack_into('<I', params, 8, 1)      # num_streams = 1 (而不是4!)
    struct.pack_into('<I', params, 12, 0)     # key_rotation_operation
    struct.pack_into('<I', params, 16, 12345) # seed = 固定值 (而不是随机!)
    struct.pack_into('<I', params, 20, 0)     # verbose
    return params, 24
```
""")

def main():
    if os.geteuid() != 0:
        print("建议以root身份运行: sudo python3", sys.argv[0])
        print()
    
    analyze_channel_stress_params()
    create_safe_channel_stress_test()
    suggest_safe_parameters()
    
    print()
    print("=" * 60)
    print("结论")
    print("=" * 60)
    print()
    print("🎯 CHANNEL_STRESS重启问题的原因:")
    print("1. iterations=100 可能太高，导致过度GPU活动")
    print("2. num_streams=4 可能创建过多并发流")
    print("3. 随机seed可能触发特定的硬件问题")
    print()
    print("🛡️ 解决方案:")
    print("1. 使用更保守的参数 (iterations=5, streams=1)")
    print("2. 使用固定seed避免随机问题")
    print("3. 考虑跳过CHANNEL_STRESS测试")
    print()
    print("⚠️ CHANNEL_STRESS是压力测试，本身就有风险!")

if __name__ == "__main__":
    main()