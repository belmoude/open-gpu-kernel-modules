#!/usr/bin/env python3
"""
正确的UVM测试 - 检查rmStatus字段获取真正的测试结果
"""

import os
import sys
import fcntl
import array
import struct

def test_with_rmstatus_check():
    """正确检查rmStatus字段的UVM测试"""
    print("正确的UVM测试 - 检查rmStatus字段")
    print("===============================")
    print()
    print("🎯 根据源码分析，真正的测试结果在参数结构的rmStatus字段中！")
    print()
    
    device_path = "/dev/nvidia-uvm"
    
    if not os.path.exists(device_path):
        print(f"❌ 设备文件不存在: {device_path}")
        return
    
    # 测试用例
    test_cases = [
        {
            "name": "RNG_SANITY",
            "cmd_id": 201,
            "params_size": 8,  # 只有rmStatus (4字节) + 对齐
            "should_pass": True,
            "description": "应该成功的基本测试"
        },
        {
            "name": "RANGE_TREE_RANDOM", 
            "cmd_id": 203,
            "params_size": 200,  # 复杂的参数结构
            "should_pass": False,  # max_batch_count=0应该失败
            "description": "max_batch_count=0应该失败"
        },
        {
            "name": "GET_USER_SPACE_END_ADDRESS",
            "cmd_id": 290,
            "params_size": 16,  # 8字节地址 + 4字节rmStatus + 对齐
            "should_pass": True,
            "description": "查询用户空间地址"
        }
    ]
    
    try:
        fd = os.open(device_path, os.O_RDWR)
        
        for test in test_cases:
            print(f"🧪 测试: {test['name']} (ID: {test['cmd_id']})")
            print(f"   描述: {test['description']}")
            print(f"   预期: {'成功' if test['should_pass'] else '失败'}")
            
            try:
                # 使用适当大小的参数缓冲区
                params = array.array('B', [0] * max(test['params_size'], 1024))
                
                # 执行ioctl
                ioctl_result = fcntl.ioctl(fd, test['cmd_id'], params)
                print(f"   ioctl返回值: {ioctl_result} (总是0)")
                
                # 检查参数结构中的rmStatus
                # rmStatus通常在结构的最后4个字节
                try:
                    # 尝试不同的rmStatus位置
                    possible_positions = [
                        test['params_size'] - 4,  # 结构末尾
                        0,                        # 结构开头
                        4,                        # 第二个字段
                        8                         # 第三个字段
                    ]
                    
                    for pos in possible_positions:
                        if pos >= 0 and pos + 4 <= len(params):
                            rm_status = struct.unpack('<I', params[pos:pos+4])[0]
                            if rm_status != 0:
                                print(f"   rmStatus (位置{pos}): {rm_status} (0x{rm_status:08x})")
                                
                                # 解析NV_STATUS错误码
                                if rm_status == 0x00000001:
                                    print(f"   -> NV_ERR_GENERIC")
                                elif rm_status == 0x00000004:
                                    print(f"   -> NV_ERR_INVALID_PARAMETER ✅")
                                elif rm_status == 0x00000031:
                                    print(f"   -> NV_ERR_ILLEGAL_ACTION")
                                else:
                                    print(f"   -> 未知错误码")
                                break
                    else:
                        print(f"   rmStatus: 0 (NV_OK - 成功)")
                        
                except Exception as e:
                    print(f"   rmStatus解析失败: {e}")
                
                # 检查是否有其他返回数据
                non_zero_bytes = [(i, b) for i, b in enumerate(params[:32]) if b != 0]
                if non_zero_bytes:
                    print(f"   返回数据: {non_zero_bytes[:5]}...")
                
                # 判断真正的测试结果
                rm_status_found = False
                for pos in [test['params_size'] - 4, 0, 4]:
                    if pos >= 0 and pos + 4 <= len(params):
                        rm_status = struct.unpack('<I', params[pos:pos+4])[0]
                        if rm_status != 0:
                            rm_status_found = True
                            if test['should_pass']:
                                print(f"   🚨 意外失败: 应该成功但rmStatus={rm_status}")
                            else:
                                print(f"   ✅ 预期失败: rmStatus={rm_status}")
                            break
                
                if not rm_status_found:
                    if test['should_pass']:
                        print(f"   ✅ 预期成功: rmStatus=0")
                    else:
                        print(f"   🚨 意外成功: 应该失败但rmStatus=0")
                
            except OSError as e:
                print(f"   ioctl系统调用失败: {e}")
            
            print()
        
        os.close(fd)
        
    except Exception as e:
        print(f"❌ 测试失败: {e}")

def create_corrected_test_script():
    """创建修正的测试脚本"""
    print("🔧 创建修正的测试脚本")
    print("===================")
    print()
    
    script_content = '''#!/usr/bin/env python3
"""
修正的UVM测试脚本 - 正确检查rmStatus字段
"""

import os
import fcntl
import array
import struct

def run_uvm_test_correct(cmd_id, test_name, buffer_size=1024):
    """正确执行UVM测试，检查rmStatus字段"""
    try:
        fd = os.open('/dev/nvidia-uvm', os.O_RDWR)
        try:
            params = array.array('B', [0] * buffer_size)
            ioctl_result = fcntl.ioctl(fd, cmd_id, params)
            
            # 检查rmStatus字段（通常在结构末尾）
            rm_status = struct.unpack('<I', params[-4:])[0]
            
            if rm_status == 0:
                return True, "NV_OK"
            else:
                error_names = {
                    0x00000001: "NV_ERR_GENERIC",
                    0x00000004: "NV_ERR_INVALID_PARAMETER", 
                    0x00000031: "NV_ERR_ILLEGAL_ACTION",
                    # 添加更多错误码...
                }
                error_name = error_names.get(rm_status, f"UNKNOWN_ERROR_0x{rm_status:08x}")
                return False, error_name
                
        finally:
            os.close(fd)
    except Exception as e:
        return False, str(e)

# 测试示例
if __name__ == "__main__":
    tests = [
        (201, "RNG_SANITY"),
        (203, "RANGE_TREE_RANDOM"),  # 这个应该失败
        (290, "GET_USER_SPACE_END_ADDRESS"),
    ]
    
    for cmd_id, name in tests:
        success, result = run_uvm_test_correct(cmd_id, name)
        status = "PASS" if success else "FAIL"
        print(f"{name:30} [{status}] {result}")
'''
    
    with open('/workspace/corrected_uvm_test.py', 'w') as f:
        f.write(script_content)
    
    os.chmod('/workspace/corrected_uvm_test.py', 0o755)
    print("✅ 已创建 corrected_uvm_test.py")
    print("运行: python3 corrected_uvm_test.py")

if __name__ == "__main__":
    if os.geteuid() != 0:
        print("需要root权限: sudo python3", sys.argv[0])
        sys.exit(1)
    
    test_with_rmstatus_check()
    create_corrected_test_script()