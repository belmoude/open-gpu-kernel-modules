# 问题排查指南

## 🔥 快速诊断

根据您遇到的错误信息，快速跳转到对应章节：

| 错误现象 | 跳转到 |
|---------|--------|
| `ret=-1, status=0x0` | [问题 1: ioctl 调用失败](#问题-1-ioctl-调用失败) |
| `status=0x1b` | [问题 2: Device 分配权限错误](#问题-2-device-分配权限错误) |
| `a label can only be part of a statement` | [问题 3: C 编译错误](#问题-3-c-编译错误) |
| `Permission denied` | [问题 4: 设备权限不足](#问题-4-设备权限不足) |
| `nvidia-smi: command not found` | [问题 5: 驱动未安装](#问题-5-驱动未安装) |

---

## 问题 1: ioctl 调用失败

### 现象
```bash
./test_virtual_alloc
成功打开 /dev/nvidiactl
分配 Client 失败: ret=-1, status=0x0
```

或者：
```
ioctl 返回: -1
errno: 25 (Inappropriate ioctl for device)
```

### 原因
使用了错误的 ioctl 调用方式。原程序直接使用 escape code 作为 ioctl 命令号，但 NVIDIA 驱动要求使用 `NV_ESC_IOCTL_XFER_CMD` 机制。

### 解决方案

#### ✅ 使用修复版程序（推荐）
```bash
cd /workspace
gcc -o test_virtual_alloc_fixed test_virtual_alloc_fixed.c -std=c99
sudo ./test_virtual_alloc_fixed
```

#### 📖 详细说明
查看 **[IOCTL_FIX_GUIDE_zh.md](IOCTL_FIX_GUIDE_zh.md)**

#### 核心修复
```c
// ❌ 错误方式
#define NV_ESC_RM_ALLOC  _IOWR('F', 0x2B, void*)
ioctl(fd, NV_ESC_RM_ALLOC, &params);

// ✅ 正确方式
nv_ioctl_xfer_t xfer = {
    .cmd = NV_ESC_RM_ALLOC,  // 0x2B
    .size = sizeof(params),
    .ptr = (NvU64)&params
};
ioctl(fd, _IOWR('F', 211, nv_ioctl_xfer_t), &xfer);
```

---

## 问题 2: Device 分配权限错误

### 现象
```bash
>>> 步骤 2: 分配 Device
❌ 分配 Device 失败:
   ioctl 返回: 0
   errno: 0 (Success)
   status: 0x1b
```

### 原因
分配 Device 时 `pAllocParms` 为 NULL，必须提供 `NV0080_ALLOC_PARAMETERS` 参数结构。

### 解决方案

#### ✅ 已在修复版程序中解决
最新的 `test_virtual_alloc_fixed.c` 已包含此修复。重新编译即可：

```bash
cd /workspace
gcc -o test_virtual_alloc_fixed test_virtual_alloc_fixed.c -std=c99
sudo ./test_virtual_alloc_fixed
```

#### 📖 详细说明
查看 **[DEVICE_ALLOC_FIX_zh.md](DEVICE_ALLOC_FIX_zh.md)**

#### 核心修复
```c
// ❌ 错误：参数为 NULL
deviceParams.pAllocParms = NULL;

// ✅ 正确：提供参数结构
NV0080_ALLOC_PARAMETERS deviceAllocParams = {0};
deviceAllocParams.deviceId = 0;
deviceAllocParams.vaMode = NV_DEVICE_ALLOCATION_VAMODE_MULTIPLE_VASPACES;
deviceParams.pAllocParms = &deviceAllocParams;
```

---

## 问题 3: C 编译错误

### 现象
```bash
test_virtual_alloc.c:313:5: error: a label can only be part of a statement 
and a declaration is not a statement
```

### 原因
C89/C90 不允许在 `goto` 标签后直接声明变量。

### 解决方案

#### ✅ 方案 1: 使用修复版程序
```bash
cd /workspace
gcc -o test_virtual_alloc_fixed test_virtual_alloc_fixed.c -std=c99
```

#### ✅ 方案 2: 显式使用 C99 标准
```bash
gcc -o test_virtual_alloc test_virtual_alloc.c -std=c99
```

#### 📖 详细说明
查看 **[BUILD_FIX.md](BUILD_FIX.md)**

---

## 问题 4: 设备权限不足

### 现象
```bash
无法打开 /dev/nvidiactl: Permission denied
```

或者后续 status=0x1b 错误。

### 解决方案

#### 1. 使用 sudo 运行
```bash
sudo ./test_virtual_alloc_fixed
```

#### 2. 检查设备权限
```bash
ls -l /dev/nvidia*

# 应该看到类似：
# crw-rw-rw- 1 root root 195,   0 ... /dev/nvidia0
# crw-rw-rw- 1 root root 195, 255 ... /dev/nvidiactl
```

#### 3. 修复权限（如果需要）
```bash
sudo chmod 666 /dev/nvidia*
```

#### 4. 检查用户组
```bash
# 将用户添加到 video 组
sudo usermod -a -G video $USER

# 重新登录使更改生效
```

---

## 问题 5: 驱动未安装

### 现象
```bash
nvidia-smi: command not found
```

或者：
```bash
ls: cannot access '/dev/nvidia*': No such file or directory
```

### 解决方案

#### 1. 检查驱动是否加载
```bash
lsmod | grep nvidia

# 应该看到类似：
# nvidia               123456  0
# nvidia_modeset        12345  1
# nvidia_uvm            12345  0
```

#### 2. 如果未加载，手动加载
```bash
sudo modprobe nvidia
sudo modprobe nvidia_uvm
sudo modprobe nvidia_modeset
```

#### 3. 验证 nvidia-smi
```bash
nvidia-smi

# 应该显示 GPU 信息
```

#### 4. 如果驱动未安装
请参考您的 Linux 发行版文档安装 NVIDIA 驱动：
- Ubuntu/Debian: `apt install nvidia-driver-XXX`
- RHEL/CentOS: `yum install nvidia-driver`
- Arch: `pacman -S nvidia`

---

## 问题 6: GPU 不可见

### 现象
程序运行但找不到 GPU，或者 `deviceId=0` 失败。

### 解决方案

#### 1. 确认 GPU 存在
```bash
lspci | grep -i nvidia

# 应该看到类似：
# 01:00.0 VGA compatible controller: NVIDIA Corporation ...
```

#### 2. 检查所有 GPU
```bash
nvidia-smi -L

# 应该列出所有 GPU：
# GPU 0: Tesla V100 (UUID: GPU-...)
# GPU 1: Tesla V100 (UUID: GPU-...)
```

#### 3. 尝试不同的 deviceId
如果有多个 GPU：
```c
deviceAllocParams.deviceId = 0;  // 尝试 0, 1, 2 等
```

---

## 问题 7: 虚拟内存分配失败

### 现象
```bash
>>> 场景 1: 只有 VIRTUAL 标志 - 不应占用数据显存
❌ 场景1 分配失败:
   status: 0x56 (NV_ERR_NO_MEMORY)
```

### 可能原因
1. 显存已满
2. 虚拟地址空间不足
3. 请求的大小超过限制

### 解决方案

#### 1. 检查显存使用情况
```bash
nvidia-smi

# 查看 Memory-Usage 列
```

#### 2. 减小分配大小
```c
// 从 1GB 减小到 256MB
virtParams1.size = 256 * 1024 * 1024;
```

#### 3. 释放其他进程占用的显存
```bash
# 查找使用 GPU 的进程
nvidia-smi

# 终止特定进程
sudo kill -9 <PID>
```

---

## 问题 8: nvidia-smi 仍然显示显存增加

### 现象
使用 `NVOS32_ALLOC_FLAGS_VIRTUAL` 后，nvidia-smi 仍然显示显存占用增加。

### 可能原因
1. 还使用了其他标志导致物理分配
2. 页表内存占用（通常很小）
3. 同时进行了物理内存分配

### 排查步骤

#### 1. 验证标志
确保只使用了 `VIRTUAL` 标志：
```c
virtParams.flags = NVOS32_ALLOC_FLAGS_VIRTUAL |
                   NVOS32_ALLOC_FLAGS_ALIGNMENT_FORCE;
// 不要包含其他会导致物理分配的标志
```

#### 2. 检查是否有其他分配
确保没有同时进行物理内存分配（测试场景 3）。

#### 3. 检查显存基线
在分配前后多次运行 `nvidia-smi`，记录 "Used" 字段。

---

## 通用调试技巧

### 1. 启用详细错误输出
修改测试程序，添加更多诊断信息：
```c
printf("调试信息:\n");
printf("  hClient: 0x%x\n", hClient);
printf("  hDevice: 0x%x\n", hDevice);
printf("  ioctl cmd: 0x%lx\n", (unsigned long)cmd);
printf("  params size: %lu\n", sizeof(params));
```

### 2. 检查内核日志
```bash
# 实时监控
sudo dmesg -w | grep -i nvidia

# 查看最近的日志
sudo dmesg | grep -i nvidia | tail -50
```

### 3. 验证驱动版本
```bash
cat /proc/driver/nvidia/version

# 或
nvidia-smi --query-gpu=driver_version --format=csv,noheader
```

推荐版本：>= 470.x

### 4. 使用 strace 追踪系统调用
```bash
sudo strace -e ioctl ./test_virtual_alloc_fixed 2>&1 | grep ioctl
```

### 5. 检查文件描述符
```c
if (fd < 0) {
    printf("打开失败: %s\n", strerror(errno));
    return 1;
}
printf("fd = %d (有效)\n", fd);
```

---

## 错误码快速参考

| status | 错误名称 | 常见原因 |
|--------|---------|---------|
| 0x00 | `NV_OK` | 成功 ✅ |
| 0x1A | `INSUFFICIENT_RESOURCES` | 资源不足 |
| **0x1B** | **`INSUFFICIENT_PERMISSIONS`** | **权限/参数问题** ⚠️ |
| 0x22 | `INVALID_CLASS` | hClass 错误 |
| 0x23 | `INVALID_CLIENT` | hClient 无效 |
| 0x26 | `INVALID_DEVICE` | hDevice 无效 |
| 0x56 | `NO_MEMORY` | 显存不足 |

**详细列表**: [ERROR_CODES_zh.md](ERROR_CODES_zh.md)

---

## 诊断流程图

```
开始
  │
  ├─ 编译失败？
  │   └─ 查看 BUILD_FIX.md
  │
  ├─ 打开设备失败？
  │   ├─ Permission denied → 使用 sudo
  │   └─ No such file → 安装驱动
  │
  ├─ ioctl 返回 -1？
  │   └─ 查看 IOCTL_FIX_GUIDE_zh.md
  │
  ├─ status = 0x1B？
  │   └─ 查看 DEVICE_ALLOC_FIX_zh.md
  │
  ├─ status = 0x56？
  │   └─ 减小分配大小
  │
  └─ 其他 status？
      └─ 查看 ERROR_CODES_zh.md
```

---

## 获取帮助

### 📚 查看相关文档

| 问题类型 | 文档 |
|---------|------|
| 快速开始 | [START_HERE_zh.md](START_HERE_zh.md) |
| ioctl 错误 | [IOCTL_FIX_GUIDE_zh.md](IOCTL_FIX_GUIDE_zh.md) |
| Device 错误 | [DEVICE_ALLOC_FIX_zh.md](DEVICE_ALLOC_FIX_zh.md) |
| 编译错误 | [BUILD_FIX.md](BUILD_FIX.md) |
| 错误码 | [ERROR_CODES_zh.md](ERROR_CODES_zh.md) |
| 完整索引 | [INDEX.md](INDEX.md) |

### 🔍 信息收集

如果上述方法都无法解决问题，请收集以下信息：

```bash
# 1. 系统信息
uname -a
cat /etc/os-release

# 2. 驱动版本
cat /proc/driver/nvidia/version
nvidia-smi --query-gpu=driver_version --format=csv

# 3. GPU 信息
lspci | grep -i nvidia
nvidia-smi -L

# 4. 设备文件
ls -l /dev/nvidia*

# 5. 内核模块
lsmod | grep nvidia

# 6. 内核日志
sudo dmesg | grep -i nvidia | tail -50

# 7. 测试程序输出
sudo ./test_virtual_alloc_fixed 2>&1 | tee output.log
```

---

## 常见环境问题

### WSL (Windows Subsystem for Linux)
NVIDIA 驱动支持 WSL2，但需要：
- Windows 11 或 Windows 10 (21H2+)
- WSL2 (不是 WSL1)
- NVIDIA GeForce Game Ready 或 Studio Driver (>=510.06)

### Docker 容器
需要使用 NVIDIA Container Toolkit：
```bash
docker run --gpus all -it nvidia/cuda:11.8.0-base-ubuntu22.04
```

### 虚拟机
GPU 直通（passthrough）可能有限制，确保：
- IOMMU 已启用
- GPU 正确直通给 VM
- VM 内安装了 NVIDIA 驱动

---

## 最后的检查清单

运行测试前，确认：

- [ ] NVIDIA 驱动已安装并加载
- [ ] `nvidia-smi` 命令可用
- [ ] `/dev/nvidiactl` 和 `/dev/nvidia0` 存在
- [ ] 使用 `sudo` 运行测试程序
- [ ] 使用 `test_virtual_alloc_fixed.c`（不是旧版本）
- [ ] 编译使用了 `-std=c99` 标志
- [ ] GPU 未被其他程序占满

全部确认后：
```bash
cd /workspace
./QUICK_START_FIXED.sh
sudo ./test_virtual_alloc_fixed
```

---

**祝测试顺利！** 🚀
