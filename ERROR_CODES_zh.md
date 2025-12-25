# NVIDIA 驱动错误码参考

## 常见错误码

| 错误码 | 宏定义 | 说明 | 常见原因 |
|--------|--------|------|---------|
| 0x00 | `NV_OK` | 成功 | 无 |
| 0x1A | `NV_ERR_INSUFFICIENT_RESOURCES` | 资源不足 | 内存不足、对象太多 |
| **0x1B** | **`NV_ERR_INSUFFICIENT_PERMISSIONS`** | **权限不足** | **未用 sudo、参数缺失** |
| 0x1C | `NV_ERR_INSUFFICIENT_POWER` | 电源不足 | GPU 功耗限制 |
| 0x22 | `NV_ERR_INVALID_CLASS` | 无效的类 | 类 ID 错误、驱动不支持 |
| 0x23 | `NV_ERR_INVALID_CLIENT` | 无效的客户端 | 客户端未分配或已释放 |
| 0x26 | `NV_ERR_INVALID_DEVICE` | 无效的设备 | 设备未分配或不存在 |
| 0x31 | `NV_ERR_INVALID_OBJECT` | 无效的对象 | 对象句柄错误 |
| 0x36 | `NV_ERR_INVALID_OBJECT_PARENT` | 无效的父对象 | 父对象句柄错误 |
| 0x56 | `NV_ERR_NO_MEMORY` | 内存不足 | 显存已满、分配失败 |
| FFFF | `NV_ERR_GENERIC` | 通用错误 | 未分类的错误 |

## 本测试程序遇到的错误

### 错误 1: ioctl 返回 -1 (errno=ENOTTY)

**现象**：
```
分配 Client 失败: ret=-1, status=0x0
```

**原因**：
- 使用了错误的 ioctl 调用方式
- 直接使用 escape code 而不是 `NV_ESC_IOCTL_XFER_CMD`

**解决方案**：
- 查看 [IOCTL_FIX_GUIDE_zh.md](IOCTL_FIX_GUIDE_zh.md)
- 使用 `test_virtual_alloc_fixed.c`

---

### 错误 2: status = 0x1B (NV_ERR_INSUFFICIENT_PERMISSIONS)

**现象**：
```
分配 Device 失败: status=0x1b
```

**原因**：
- 分配 Device 时 `pAllocParms` 为 NULL
- 缺少 `NV0080_ALLOC_PARAMETERS` 参数结构

**解决方案**：
- 查看 [DEVICE_ALLOC_FIX_zh.md](DEVICE_ALLOC_FIX_zh.md)
- 提供正确的 `NV0080_ALLOC_PARAMETERS`

**正确代码**：
```c
NV0080_ALLOC_PARAMETERS deviceAllocParams = {0};
deviceAllocParams.deviceId = 0;
deviceAllocParams.vaMode = NV_DEVICE_ALLOCATION_VAMODE_MULTIPLE_VASPACES;

NVOS64_PARAMETERS deviceParams = {0};
deviceParams.pAllocParms = &deviceAllocParams;  // 关键！
```

---

## 详细错误码列表

**来源**: `src/common/sdk/nvidia/inc/nvstatuscodes.h`

### 内存相关错误

| 错误码 | 宏定义 | 说明 |
|--------|--------|------|
| 0x08 | `NV_ERR_DMA_MEM_NOT_LOCKED` | DMA 内存未锁定 |
| 0x09 | `NV_ERR_DMA_MEM_NOT_UNLOCKED` | DMA 内存未解锁 |
| 0x1A | `NV_ERR_INSUFFICIENT_RESOURCES` | 资源不足（除内存外） |
| 0x56 | `NV_ERR_NO_MEMORY` | 内存不足 |
| 0x7E | `NV_ERR_OUT_OF_RANGE` | 超出范围 |

### 权限和访问相关错误

| 错误码 | 宏定义 | 说明 |
|--------|--------|------|
| 0x16 | `NV_ERR_ILLEGAL_ACTION` | 当前操作不允许 |
| 0x1B | `NV_ERR_INSUFFICIENT_PERMISSIONS` | 权限不足 |
| 0x1D | `NV_ERR_INVALID_ACCESS_TYPE` | 访问类型无效 |
| 0x84 | `NV_ERR_PROTECTION_FAULT` | 保护错误 |

### 参数验证错误

| 错误码 | 宏定义 | 说明 |
|--------|--------|------|
| 0x1E | `NV_ERR_INVALID_ADDRESS` | 地址无效 |
| 0x1F | `NV_ERR_INVALID_ARGUMENT` | 参数无效 |
| 0x25 | `NV_ERR_INVALID_DATA` | 数据无效 |
| 0x29 | `NV_ERR_INVALID_FLAGS` | 标志无效 |
| 0x3A | `NV_ERR_INVALID_PARAM_STRUCT` | 参数结构无效 |
| 0x3B | `NV_ERR_INVALID_PARAMETER` | 至少一个参数无效 |
| 0x3D | `NV_ERR_INVALID_POINTER` | 指针无效 |

### 对象相关错误

| 错误码 | 宏定义 | 说明 |
|--------|--------|------|
| 0x22 | `NV_ERR_INVALID_CLASS` | 类 ID 无效 |
| 0x23 | `NV_ERR_INVALID_CLIENT` | 客户端无效 |
| 0x26 | `NV_ERR_INVALID_DEVICE` | 设备无效 |
| 0x31 | `NV_ERR_INVALID_OBJECT` | 对象无效 |
| 0x33 | `NV_ERR_INVALID_OBJECT_HANDLE` | 对象句柄无效 |
| 0x34 | `NV_ERR_INVALID_OBJECT_NEW` | 新对象无效 |
| 0x35 | `NV_ERR_INVALID_OBJECT_OLD` | 旧对象无效 |
| 0x36 | `NV_ERR_INVALID_OBJECT_PARENT` | 父对象无效 |
| 0x39 | `NV_ERR_INVALID_OWNER` | 所有者无效 |

### 状态相关错误

| 错误码 | 宏定义 | 说明 |
|--------|--------|------|
| 0x03 | `NV_ERR_BUSY_RETRY` | 系统忙，稍后重试 |
| 0x0F | `NV_ERR_GPU_IS_LOST` | GPU 从总线丢失 |
| 0x10 | `NV_ERR_GPU_IN_FULLCHIP_RESET` | GPU 处于全芯片复位 |
| 0x11 | `NV_ERR_GPU_NOT_FULL_POWER` | GPU 未满功率 |
| 0x17 | `NV_ERR_IN_USE` | 通用忙错误 |
| 0x40 | `NV_ERR_INVALID_STATE` | 状态无效 |

## 调试技巧

### 1. 打印详细的错误信息

```c
if (params.status != 0) {
    printf("错误: status=0x%x", params.status);
    
    // 添加已知错误码的解释
    switch (params.status) {
        case 0x1B:
            printf(" (NV_ERR_INSUFFICIENT_PERMISSIONS - 权限不足)\n");
            printf("  可能原因: 未使用 sudo、参数缺失、设备不可访问\n");
            break;
        case 0x22:
            printf(" (NV_ERR_INVALID_CLASS - 类无效)\n");
            printf("  可能原因: hClass 值错误、驱动不支持该类\n");
            break;
        case 0x23:
            printf(" (NV_ERR_INVALID_CLIENT - 客户端无效)\n");
            printf("  可能原因: hClient 未分配或已释放\n");
            break;
        case 0x56:
            printf(" (NV_ERR_NO_MEMORY - 内存不足)\n");
            printf("  可能原因: 显存已满、请求大小超限\n");
            break;
        default:
            printf(" (未知错误)\n");
            break;
    }
}
```

### 2. 检查 ioctl 返回值

```c
int ret = ioctl(fd, cmd, &params);
if (ret != 0) {
    printf("ioctl 失败: ret=%d, errno=%d (%s)\n", 
           ret, errno, strerror(errno));
}

if (params.status != 0) {
    printf("RM 错误: status=0x%x\n", params.status);
}
```

### 3. 验证前置条件

分配对象前，确保：
- ✅ 父对象已成功分配
- ✅ 所有句柄 (handles) 有效
- ✅ 必需的参数结构已提供
- ✅ 使用了正确的类 ID (hClass)

### 4. 使用 dmesg 查看内核日志

```bash
# 查看最近的 NVIDIA 驱动消息
sudo dmesg | grep -i nvidia | tail -20

# 实时监控
sudo dmesg -w | grep -i nvidia
```

### 5. 启用驱动调试日志

```bash
# 设置日志级别（需要驱动支持）
echo 0xFFFFFFFF | sudo tee /proc/driver/nvidia/params

# 查看日志
cat /proc/driver/nvidia/log
```

## 错误码映射表（按值排序）

| 十六进制 | 十进制 | 宏定义 |
|---------|-------|--------|
| 0x00 | 0 | `NV_OK` |
| 0x01 | 1 | `NV_ERR_BROKEN_FB` |
| 0x02 | 2 | `NV_ERR_BUFFER_TOO_SMALL` |
| 0x03 | 3 | `NV_ERR_BUSY_RETRY` |
| ... | ... | ... |
| 0x1A | 26 | `NV_ERR_INSUFFICIENT_RESOURCES` |
| **0x1B** | **27** | **`NV_ERR_INSUFFICIENT_PERMISSIONS`** |
| 0x1C | 28 | `NV_ERR_INSUFFICIENT_POWER` |
| ... | ... | ... |
| 0x22 | 34 | `NV_ERR_INVALID_CLASS` |
| 0x23 | 35 | `NV_ERR_INVALID_CLIENT` |
| ... | ... | ... |
| 0x56 | 86 | `NV_ERR_NO_MEMORY` |
| ... | ... | ... |
| 0xFFFF | 65535 | `NV_ERR_GENERIC` |

## 快速诊断流程

```
遇到错误
  ├─ ioctl 返回 -1？
  │   ├─ errno = ENOTTY → ioctl 命令号错误 → 查看 IOCTL_FIX_GUIDE_zh.md
  │   ├─ errno = EACCES → 权限不足 → 使用 sudo
  │   └─ errno = EFAULT → 参数指针无效 → 检查地址
  │
  └─ status != 0？
      ├─ 0x1B → 权限/参数问题 → 查看 DEVICE_ALLOC_FIX_zh.md
      ├─ 0x22 → 类 ID 错误 → 检查 hClass
      ├─ 0x23 → 客户端无效 → 检查 Client 是否分配
      ├─ 0x26 → 设备无效 → 检查 Device 是否分配
      ├─ 0x56 → 内存不足 → 减小分配大小或释放资源
      └─ 其他 → 查看上面的错误码表
```

## 源码位置

### 错误码定义
**文件**: `src/common/sdk/nvidia/inc/nvstatuscodes.h`

完整的错误码定义，包含错误消息字符串。

### 错误码头文件
**文件**: `src/common/sdk/nvidia/inc/nvstatus.h`

错误码的宏定义和辅助函数。

### 错误处理示例
**文件**: `src/nvidia/interface/deprecated/rmapi_deprecated_alloc.c`

展示了驱动内部如何生成和处理错误码。

## 总结

| 遇到的错误 | 错误码 | 解决文档 |
|-----------|--------|---------|
| ioctl 返回 -1 | N/A (errno) | IOCTL_FIX_GUIDE_zh.md |
| Device 分配失败 | 0x1B | DEVICE_ALLOC_FIX_zh.md |
| 编译错误 | N/A | BUILD_FIX.md |

**关键点**：
- 区分 **ioctl 错误**（返回值 -1）和 **RM 错误**（status 字段）
- 0x1B 通常意味着参数缺失或权限问题
- 使用 `sudo` 运行测试程序
- 提供所有必需的参数结构

---

**相关文档**：
- [IOCTL_FIX_GUIDE_zh.md](IOCTL_FIX_GUIDE_zh.md)
- [DEVICE_ALLOC_FIX_zh.md](DEVICE_ALLOC_FIX_zh.md)
- [START_HERE_zh.md](START_HERE_zh.md)
