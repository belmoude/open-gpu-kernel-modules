#!/usr/bin/env python3
"""
提取所有UVM测试参数结构的脚本
系统性地分析每个测试的参数要求
"""

import re
import os

def extract_test_definitions():
    """从uvm_test_ioctl.h中提取所有测试定义"""
    
    ioctl_file = "/workspace/kernel-open/nvidia-uvm/uvm_test_ioctl.h"
    
    if not os.path.exists(ioctl_file):
        print("错误: 找不到uvm_test_ioctl.h文件")
        return []
    
    with open(ioctl_file, 'r') as f:
        content = f.read()
    
    # 提取所有测试定义
    test_pattern = r'#define\s+(UVM_TEST_\w+)\s+UVM_TEST_IOCTL_BASE\((\d+)\)'
    struct_pattern = r'typedef\s+struct\s*\{([^}]+)\}\s*(\w+);'
    
    tests = []
    lines = content.split('\n')
    
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        
        # 查找测试定义
        test_match = re.match(test_pattern, line)
        if test_match:
            test_name = test_match.group(1)
            test_id = int(test_match.group(2))
            
            # 查找对应的结构定义
            struct_lines = []
            j = i + 1
            in_struct = False
            brace_count = 0
            
            while j < len(lines):
                if 'typedef struct' in lines[j]:
                    in_struct = True
                    brace_count = 0
                
                if in_struct:
                    struct_lines.append(lines[j])
                    brace_count += lines[j].count('{') - lines[j].count('}')
                    
                    if brace_count == 0 and '}' in lines[j]:
                        break
                
                j += 1
            
            struct_content = '\n'.join(struct_lines)
            
            tests.append({
                'name': test_name,
                'id': test_id,
                'cmd_id': 200 + test_id,
                'struct_def': struct_content
            })
        
        i += 1
    
    return sorted(tests, key=lambda x: x['id'])

def analyze_struct_fields(struct_def):
    """分析结构字段"""
    fields = []
    
    # 简单的字段提取（可以改进）
    lines = struct_def.split('\n')
    for line in lines:
        line = line.strip()
        if line and not line.startswith('//') and not line.startswith('typedef') and not line.startswith('}'):
            # 移除注释
            if '//' in line:
                line = line[:line.index('//')]
            
            line = line.strip().rstrip(';')
            if line:
                fields.append(line)
    
    return fields

def main():
    print("UVM测试参数结构提取工具")
    print("======================")
    print()
    
    tests = extract_test_definitions()
    
    print(f"找到 {len(tests)} 个测试定义:")
    print()
    
    for test in tests[:10]:  # 显示前10个作为示例
        print(f"测试: {test['name']} (ID: {test['cmd_id']})")
        
        fields = analyze_struct_fields(test['struct_def'])
        print("  参数字段:")
        for field in fields:
            print(f"    {field}")
        print()
    
    print("...")
    print(f"(还有 {len(tests)-10} 个测试)")
    
    # 生成测试参数设置代码
    print("\n生成测试参数设置代码...")
    
    with open('/workspace/all_test_params.py', 'w') as f:
        f.write('# 所有UVM测试的参数设置\n\n')
        f.write('import struct\nimport array\nimport time\nimport random\n\n')
        
        for test in tests:
            f.write(f"def setup_{test['name'].lower()}():\n")
            f.write(f'    """设置{test["name"]}测试的参数"""\n')
            
            # 分析字段并生成设置代码
            fields = analyze_struct_fields(test['struct_def'])
            
            if not fields or (len(fields) == 1 and 'rmStatus' in fields[0]):
                # 只有rmStatus的简单测试
                f.write('    params = array.array("B", [0] * 8)\n')
                f.write('    return params, 0  # rmStatus offset\n\n')
            else:
                # 复杂参数的测试
                f.write('    params = array.array("B", [0] * 1024)\n')
                f.write('    # TODO: 根据字段设置具体参数\n')
                
                offset = 0
                for field in fields:
                    if 'NvU32' in field:
                        f.write(f'    # {field}\n')
                        if 'seed' in field.lower():
                            f.write(f'    struct.pack_into("<I", params, {offset}, int(time.time()) % 0xFFFFFFFF)\n')
                        elif 'probability' in field.lower():
                            f.write(f'    struct.pack_into("<I", params, {offset}, 50)  # 50%\n')
                        elif 'count' in field.lower() or 'iterations' in field.lower():
                            f.write(f'    struct.pack_into("<I", params, {offset}, 10)\n')
                        else:
                            f.write(f'    struct.pack_into("<I", params, {offset}, 0)\n')
                        offset += 4
                    elif 'NvU64' in field:
                        f.write(f'    # {field}\n')
                        f.write(f'    struct.pack_into("<Q", params, {offset}, 0x1000)\n')
                        offset += 8
                    elif 'rmStatus' in field:
                        f.write(f'    # {field} - output field\n')
                        break
                
                f.write(f'    return params, {offset}  # rmStatus offset\n\n')
    
    print("✅ 已生成 all_test_params.py")

if __name__ == "__main__":
    main()