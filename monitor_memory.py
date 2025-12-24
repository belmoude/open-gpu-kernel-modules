#!/usr/bin/env python3
"""
显存监控脚本 - 配合 test_virtual_alloc 使用

安装依赖:
    pip install pynvml

运行方式:
    python3 monitor_memory.py

功能:
    持续监控 GPU 显存使用情况，每秒刷新一次
"""

import time
import os
from datetime import datetime

try:
    import pynvml
    HAS_PYNVML = True
except ImportError:
    HAS_PYNVML = False
    print("警告: 未安装 pynvml 库，将使用 nvidia-smi 命令")

def format_bytes(bytes_val):
    """格式化字节数为人类可读格式"""
    for unit in ['B', 'KB', 'MB', 'GB', 'TB']:
        if bytes_val < 1024.0:
            return f"{bytes_val:.2f} {unit}"
        bytes_val /= 1024.0
    return f"{bytes_val:.2f} PB"

def monitor_with_pynvml():
    """使用 pynvml 库监控显存"""
    pynvml.nvmlInit()
    device_count = pynvml.nvmlDeviceGetCount()
    
    print(f"检测到 {device_count} 个 GPU 设备")
    print("\n按 Ctrl+C 停止监控\n")
    print("=" * 100)
    
    try:
        while True:
            os.system('clear' if os.name != 'nt' else 'cls')
            
            timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            print(f"显存监控 - {timestamp}")
            print("=" * 100)
            
            for i in range(device_count):
                handle = pynvml.nvmlDeviceGetHandleByIndex(i)
                name = pynvml.nvmlDeviceGetName(handle)
                
                # 获取显存信息
                mem_info = pynvml.nvmlDeviceGetMemoryInfo(handle)
                
                total = mem_info.total
                used = mem_info.used
                free = mem_info.free
                used_percent = (used / total) * 100
                
                print(f"\nGPU {i}: {name}")
                print(f"  总显存:   {format_bytes(total)}")
                print(f"  已使用:   {format_bytes(used)} ({used_percent:.1f}%)")
                print(f"  空闲:     {format_bytes(free)}")
                
                # 获取进程信息
                try:
                    processes = pynvml.nvmlDeviceGetComputeRunningProcesses(handle)
                    if processes:
                        print(f"  运行进程: {len(processes)} 个")
                        for proc in processes[:5]:  # 只显示前5个
                            try:
                                pid = proc.pid
                                used_mem = proc.usedGpuMemory
                                print(f"    PID {pid}: {format_bytes(used_mem)}")
                            except:
                                pass
                except:
                    pass
                
                print("-" * 100)
            
            time.sleep(1)
            
    except KeyboardInterrupt:
        print("\n\n监控已停止")
    finally:
        pynvml.nvmlShutdown()

def monitor_with_nvidia_smi():
    """使用 nvidia-smi 命令监控显存"""
    print("\n使用 nvidia-smi 监控显存")
    print("按 Ctrl+C 停止监控\n")
    print("=" * 100)
    
    try:
        while True:
            os.system('clear' if os.name != 'nt' else 'cls')
            
            timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            print(f"显存监控 - {timestamp}")
            print("=" * 100)
            
            # 执行 nvidia-smi 并显示结果
            ret = os.system('nvidia-smi --query-gpu=index,name,memory.total,memory.used,memory.free --format=csv,noheader,nounits')
            
            if ret != 0:
                print("错误: 无法执行 nvidia-smi 命令")
                print("请确保:")
                print("  1. 已安装 NVIDIA 驱动")
                print("  2. nvidia-smi 在 PATH 中")
                break
            
            print("\n" + "-" * 100)
            print("说明:")
            print("  - 正在监控显存变化")
            print("  - 在另一个终端运行测试程序观察显存变化")
            print("  - 虚拟内存分配（LAZY）不会增加 'used' 字段")
            print("  - 物理内存分配会立即增加 'used' 字段")
            print("-" * 100)
            
            time.sleep(1)
            
    except KeyboardInterrupt:
        print("\n\n监控已停止")

def main():
    print("""
╔══════════════════════════════════════════════════════════════╗
║          NVIDIA GPU 显存监控工具                              ║
║                                                               ║
║  用途: 配合 test_virtual_alloc 测试程序观察显存分配行为      ║
╚══════════════════════════════════════════════════════════════╝
    """)
    
    if HAS_PYNVML:
        try:
            monitor_with_pynvml()
        except Exception as e:
            print(f"使用 pynvml 监控失败: {e}")
            print("切换到 nvidia-smi 方式...")
            monitor_with_nvidia_smi()
    else:
        monitor_with_nvidia_smi()

if __name__ == "__main__":
    main()
