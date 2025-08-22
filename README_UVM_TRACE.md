# UVM API Parameter Tracer

这个项目包含用于跟踪 NVIDIA UVM (Unified Virtual Memory) API 函数参数的 bpftrace 脚本。

## 脚本文件

### 1. `uvm_trace_safe.bt` - 推荐版本 ⭐
最稳定的版本，避免了复杂的结构体访问，适合生产环境使用。

### 2. `uvm_trace_simple.bt` - 简化版本  
专注于核心参数，输出更简洁，适合快速监控。

### 3. `uvm_trace_fixed.bt` - 修复版本
修复了语法错误的版本，包含基本的参数信息。

### 4. `uvm_trace.bt` - 完整版本
包含详细的参数解析和 GPU 属性信息，适合深度调试（可能有兼容性问题）。

### 5. `uvm_trace_debug.bt` - 调试版本
包含完整的调用栈和详细调试信息（可能有兼容性问题）。

## 跟踪的函数

### `uvm_api_create_external_range`
**参数结构**: `UVM_CREATE_EXTERNAL_RANGE_PARAMS`
- `base` (NvU64): 虚拟地址基址
- `length` (NvU64): 地址范围长度
- `rmStatus` (NV_STATUS): 返回状态（输出参数）

### `uvm_api_map_external_allocation`  
**参数结构**: `UVM_MAP_EXTERNAL_ALLOCATION_PARAMS`
- `base` (NvU64): 虚拟地址基址
- `length` (NvU64): 映射长度
- `offset` (NvU64): 内存偏移量
- `perGpuAttributes[]`: GPU 映射属性数组
- `gpuAttributesCount` (NvU64): GPU 属性数量
- `rmCtrlFd` (NvS32): RM 控制文件描述符
- `hClient` (NvU32): 客户端句柄
- `hMemory` (NvU32): 内存句柄
- `rmStatus` (NV_STATUS): 返回状态（输出参数）

## 使用方法

### 前提条件
1. 安装 bpftrace
2. 加载 nvidia-uvm 内核模块
3. 具有 root 权限

### 运行脚本

```bash
# 推荐：运行安全版本（最稳定）
sudo bpftrace uvm_trace_safe.bt

# 运行修复版本（包含参数详情）
sudo bpftrace uvm_trace_fixed.bt

# 运行简化版本（简洁输出）
sudo bpftrace uvm_trace_simple.bt

# 将输出保存到文件
sudo bpftrace uvm_trace_safe.bt > uvm_trace.log 2>&1
```

### 示例输出

#### uvm_api_create_external_range
```
[14:30:25] PID:1234 uvm_api_create_external_range(
  base:   0x00007f8000000000
  length: 0x0000000040000000 (1073741824 bytes)
)
```

#### uvm_api_map_external_allocation
```
[14:30:26] PID:1234 uvm_api_map_external_allocation(
  base:              0x00007f8000000000
  length:            0x0000000040000000 (1073741824 bytes)
  offset:            0x0000000000000000
  rmCtrlFd:          3
  hClient:           0x12345678
  hMemory:           0x87654321
  gpuAttributesCount: 1
)
```

## 注意事项

1. **权限要求**: 需要 root 权限来运行 bpftrace
2. **性能影响**: 跟踪会对系统性能产生轻微影响
3. **内核符号**: 确保内核符号可用，可能需要安装调试符号包
4. **模块加载**: 确保 nvidia-uvm 模块已加载

## 故障排除

### 符号未找到
如果出现 "symbol not found" 错误：
```bash
# 检查模块是否加载
lsmod | grep nvidia_uvm

# 检查符号是否可用
sudo cat /proc/kallsyms | grep uvm_api_create_external_range
sudo cat /proc/kallsyms | grep uvm_api_map_external_allocation
```

### 权限问题
确保以 root 权限运行：
```bash
sudo bpftrace --help
```

### 调试模式
启用详细输出进行调试：
```bash
sudo bpftrace -v uvm_trace_simple.bt
```