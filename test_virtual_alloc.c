/*
 * 测试程序：演示虚拟内存分配的延迟物理分配行为
 * 
 * 编译方法：
 * gcc -o test_virtual_alloc test_virtual_alloc.c -I./src/common/sdk/nvidia/inc -I./src/nvidia/arch/nvalloc/unix/include
 * 
 * 运行方法：
 * 1. 运行程序前先记录 nvidia-smi 显示的显存使用量
 * 2. 运行 ./test_virtual_alloc
 * 3. 在程序暂停时查看 nvidia-smi，观察显存使用量变化
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdint.h>

// 简化的 NVIDIA 类型和常量定义
typedef uint32_t NvU32;
typedef uint64_t NvU64;
typedef int32_t  NvS32;
typedef void*    NvP64;
typedef uint16_t NvU16;

#define NV_IOCTL_MAGIC      'F'
#define NV_ESC_RM_ALLOC                   _IOWR(NV_IOCTL_MAGIC, 0x27, void*)
#define NV_ESC_RM_VID_HEAP_CONTROL        _IOWR(NV_IOCTL_MAGIC, 0x4A, void*)
#define NV_ESC_RM_FREE                    _IOWR(NV_IOCTL_MAGIC, 0x29, void*)
#define NV_ESC_RM_MAP_MEMORY_DMA          _IOWR(NV_IOCTL_MAGIC, 0x52, void*)

// NVOS32 常量
#define NVOS32_FUNCTION_ALLOC_SIZE        2
#define NVOS32_FUNCTION_FREE              3

// 分配标志
#define NVOS32_ALLOC_FLAGS_VIRTUAL                  0x00080000
#define NVOS32_ALLOC_FLAGS_LAZY                     0x00000400
#define NVOS32_ALLOC_FLAGS_EXTERNALLY_MANAGED       0x00400000
#define NVOS32_ALLOC_FLAGS_ALIGNMENT_FORCE          0x00000100
#define NVOS32_ALLOC_FLAGS_MEMORY_HANDLE_PROVIDED   0x00004000

// 属性标志
#define NVOS32_ATTR_LOCATION_VIDMEM       0x00000002
#define NVOS32_ATTR_PHYSICALITY_CONTIGUOUS 0x00000000

// 类分配参数
#define NV01_ROOT_CLIENT                  0x00000041
#define NV01_DEVICE_0                     0x00000080
#define NV50_MEMORY_VIRTUAL               0x000050a0

// 简化的参数结构
typedef struct {
    NvU32  hRoot;
    NvU32  hObjectParent;
    NvU32  hObjectNew;
    NvU32  hClass;
    void  *pAllocParms;
    NvU32  status;
} NVOS21_PARAMETERS;

typedef struct {
    NvU32  hRoot;
    NvU32  hObjectOld;
    NvU32  status;
} NVOS00_PARAMETERS;

typedef struct {
    NvU32  owner;
    NvU32  hMemory;
    NvU32  type;
    NvU32  flags;
    NvU32  attr;
    NvU32  attr2;
    NvU32  format;
    NvU32  comprCovg;
    NvU32  zcullCovg;
    NvU32  partitionStride;
    NvU32  width;
    NvU32  height;
    NvU64  size;
    NvU64  alignment;
    NvU64  offset;
    NvU64  limit;
    NvU64  address;
    NvU64  rangeBegin;
    NvU64  rangeEnd;
    NvU32  hVASpace;
} AllocSizeParams;

typedef struct {
    NvU32  hRoot;
    NvU32  hObjectParent;
    NvU32  function;
    NvU32  hVASpace;
    NvS32  ivcHeapNumber;
    NvU32  status;
    NvU64  total;
    NvU64  free;
    union {
        AllocSizeParams AllocSize;
        struct {
            NvU32 owner;
            NvU32 hMemory;
            NvU32 flags;
        } Free;
    } data;
} NVOS32_PARAMETERS;

typedef struct {
    NvU32  owner;
    NvU32  type;
    NvU32  flags;
    NvU32  attr;
    NvU32  attr2;
    NvU64  size;
    NvU64  alignment;
    NvU64  offset;
    NvU64  limit;
    NvU64  rangeLo;
    NvU64  rangeHi;
    NvU32  hVASpace;
} NV_MEMORY_ALLOCATION_PARAMS;

void print_memory_info(const char* stage) {
    printf("\n==================== %s ====================\n", stage);
    printf("请在另一个终端运行 'nvidia-smi' 查看显存使用情况\n");
    printf("按回车继续...\n");
    getchar();
}

int main() {
    int fd;
    int ret;
    NvU32 hClient = 0;
    NvU32 hDevice = 0;
    NvU32 hVASpace = 0;
    NvU32 hMemory = 0;
    
    // 打开 NVIDIA 控制设备
    fd = open("/dev/nvidiactl", O_RDWR);
    if (fd < 0) {
        perror("无法打开 /dev/nvidiactl");
        return 1;
    }
    
    printf("成功打开 /dev/nvidiactl\n");
    
    // 1. 分配 Client
    NVOS21_PARAMETERS clientParams = {0};
    clientParams.hRoot = 0;
    clientParams.hObjectParent = 0;
    clientParams.hObjectNew = 0; // RM 会分配句柄
    clientParams.hClass = NV01_ROOT_CLIENT;
    clientParams.pAllocParms = NULL;
    
    ret = ioctl(fd, NV_ESC_RM_ALLOC, &clientParams);
    if (ret != 0 || clientParams.status != 0) {
        printf("分配 Client 失败: ret=%d, status=0x%x\n", ret, clientParams.status);
        close(fd);
        return 1;
    }
    hClient = clientParams.hObjectNew;
    printf("成功分配 Client: 0x%x\n", hClient);
    
    // 2. 分配 Device
    NVOS21_PARAMETERS deviceParams = {0};
    deviceParams.hRoot = hClient;
    deviceParams.hObjectParent = hClient;
    deviceParams.hObjectNew = 0x12345678; // 指定设备句柄
    deviceParams.hClass = NV01_DEVICE_0;
    deviceParams.pAllocParms = NULL;
    
    ret = ioctl(fd, NV_ESC_RM_ALLOC, &deviceParams);
    if (ret != 0 || deviceParams.status != 0) {
        printf("分配 Device 失败: ret=%d, status=0x%x\n", ret, deviceParams.status);
        goto cleanup_client;
    }
    hDevice = deviceParams.hObjectNew;
    printf("成功分配 Device: 0x%x\n", hDevice);
    
    print_memory_info("初始状态 - 未分配任何显存");
    
    // 3. 测试场景1：只使用 VIRTUAL 标志（不带 LAZY）
    printf("\n>>> 场景1：分配虚拟内存（只有 VIRTUAL 标志）- 不应立即占用数据显存 <<<\n");
    
    NV_MEMORY_ALLOCATION_PARAMS virtualAllocParams1 = {0};
    virtualAllocParams1.owner = hClient;
    virtualAllocParams1.type = 0;
    virtualAllocParams1.flags = NVOS32_ALLOC_FLAGS_VIRTUAL |   // 只有 VIRTUAL
                                NVOS32_ALLOC_FLAGS_ALIGNMENT_FORCE;
    virtualAllocParams1.attr = NVOS32_ATTR_LOCATION_VIDMEM;
    virtualAllocParams1.attr2 = 0;
    virtualAllocParams1.size = 1024 * 1024 * 1024; // 1GB 虚拟地址空间
    virtualAllocParams1.alignment = 4096;
    virtualAllocParams1.offset = 0;
    virtualAllocParams1.hVASpace = 0; // 使用默认 VA space
    
    NVOS21_PARAMETERS virtualMemParams1 = {0};
    virtualMemParams1.hRoot = hClient;
    virtualMemParams1.hObjectParent = hDevice;
    virtualMemParams1.hObjectNew = 0x87654321; // 指定内存对象句柄
    virtualMemParams1.hClass = NV50_MEMORY_VIRTUAL;
    virtualMemParams1.pAllocParms = &virtualAllocParams1;
    
    ret = ioctl(fd, NV_ESC_RM_ALLOC, &virtualMemParams1);
    if (ret != 0 || virtualMemParams1.status != 0) {
        printf("分配虚拟内存失败: ret=%d, status=0x%x\n", ret, virtualMemParams1.status);
        goto cleanup_device;
    }
    NvU32 hMemory1 = virtualMemParams1.hObjectNew;
    printf("成功分配虚拟内存对象: 0x%x (大小: %llu MB)\n", 
           hMemory1, (unsigned long long)(virtualAllocParams1.size / 1024 / 1024));
    printf("虚拟地址偏移: 0x%llx\n", (unsigned long long)virtualAllocParams1.offset);
    printf("说明: 可能有很小的页表内存占用（通常 < 1%%），但数据显存不会分配\n");
    
    print_memory_info("分配虚拟内存后（只有 VIRTUAL）- nvidia-smi 基本不变");
    
    // 释放场景1的虚拟内存
    NVOS00_PARAMETERS freeVirtual1Params = {0};
    freeVirtual1Params.hRoot = hClient;
    freeVirtual1Params.hObjectOld = hMemory1;
    ioctl(fd, NV_ESC_RM_FREE, &freeVirtual1Params);
    printf("已释放场景1的虚拟内存\n");
    
    // 4. 测试场景2：使用 VIRTUAL + LAZY 标志（更彻底）
    printf("\n>>> 场景2：分配虚拟内存（VIRTUAL + LAZY）- 完全不占用显存 <<<\n");
    
    NV_MEMORY_ALLOCATION_PARAMS virtualAllocParams = {0};
    virtualAllocParams.owner = hClient;
    virtualAllocParams.type = 0;
    virtualAllocParams.flags = NVOS32_ALLOC_FLAGS_VIRTUAL | 
                               NVOS32_ALLOC_FLAGS_LAZY |     // 加上 LAZY
                               NVOS32_ALLOC_FLAGS_ALIGNMENT_FORCE;
    virtualAllocParams.attr = NVOS32_ATTR_LOCATION_VIDMEM;
    virtualAllocParams.attr2 = 0;
    virtualAllocParams.size = 1024 * 1024 * 1024; // 1GB 虚拟地址空间
    virtualAllocParams.alignment = 4096;
    virtualAllocParams.offset = 0;
    virtualAllocParams.hVASpace = 0; // 使用默认 VA space
    
    NVOS21_PARAMETERS virtualMemParams = {0};
    virtualMemParams.hRoot = hClient;
    virtualMemParams.hObjectParent = hDevice;
    virtualMemParams.hObjectNew = 0x98765432; // 不同的句柄
    virtualMemParams.hClass = NV50_MEMORY_VIRTUAL;
    virtualMemParams.pAllocParms = &virtualAllocParams;
    
    ret = ioctl(fd, NV_ESC_RM_ALLOC, &virtualMemParams);
    if (ret != 0 || virtualMemParams.status != 0) {
        printf("分配虚拟内存失败: ret=%d, status=0x%x\n", ret, virtualMemParams.status);
        goto cleanup_device;
    }
    hMemory = virtualMemParams.hObjectNew;
    printf("成功分配虚拟内存对象: 0x%x (大小: %llu MB)\n", 
           hMemory, (unsigned long long)(virtualAllocParams.size / 1024 / 1024));
    printf("虚拟地址偏移: 0x%llx\n", (unsigned long long)virtualAllocParams.offset);
    printf("说明: 连页表内存也不预分配\n");
    
    print_memory_info("分配虚拟内存后（VIRTUAL + LAZY）- nvidia-smi 完全不变");
    
    // 5. 测试场景3：使用标准 NVOS32 分配物理显存（对比）
    printf("\n>>> 场景3：分配物理显存（不带 VIRTUAL/LAZY 标志）- 应立即占用物理显存 <<<\n");
    
    NVOS32_PARAMETERS physAllocParams = {0};
    physAllocParams.hRoot = hClient;
    physAllocParams.hObjectParent = hDevice;
    physAllocParams.function = NVOS32_FUNCTION_ALLOC_SIZE;
    physAllocParams.hVASpace = 0;
    physAllocParams.ivcHeapNumber = 0;
    physAllocParams.data.AllocSize.owner = hClient;
    physAllocParams.data.AllocSize.hMemory = 0; // RM 分配句柄
    physAllocParams.data.AllocSize.type = 0;
    physAllocParams.data.AllocSize.flags = 0; // 无 VIRTUAL/LAZY 标志
    physAllocParams.data.AllocSize.attr = NVOS32_ATTR_LOCATION_VIDMEM;
    physAllocParams.data.AllocSize.attr2 = 0;
    physAllocParams.data.AllocSize.size = 256 * 1024 * 1024; // 256MB
    physAllocParams.data.AllocSize.alignment = 4096;
    
    ret = ioctl(fd, NV_ESC_RM_VID_HEAP_CONTROL, &physAllocParams);
    if (ret != 0 || physAllocParams.status != 0) {
        printf("分配物理显存失败: ret=%d, status=0x%x\n", ret, physAllocParams.status);
    } else {
        NvU32 hPhysMemory = physAllocParams.data.AllocSize.hMemory;
        printf("成功分配物理显存: 0x%x (大小: %llu MB)\n", 
               hPhysMemory, (unsigned long long)(physAllocParams.data.AllocSize.size / 1024 / 1024));
        
        print_memory_info("分配物理显存后 - nvidia-smi 应显示显存增加约 256MB");
        
        // 释放物理显存
        NVOS32_PARAMETERS freeParams = {0};
        freeParams.hRoot = hClient;
        freeParams.hObjectParent = hDevice;
        freeParams.function = NVOS32_FUNCTION_FREE;
        freeParams.data.Free.owner = hClient;
        freeParams.data.Free.hMemory = hPhysMemory;
        freeParams.data.Free.flags = NVOS32_ALLOC_FLAGS_MEMORY_HANDLE_PROVIDED;
        
        ioctl(fd, NV_ESC_RM_VID_HEAP_CONTROL, &freeParams);
        printf("已释放物理显存\n");
    }
    
    // 清理虚拟内存
    NVOS00_PARAMETERS freeVirtualParams = {0};
    freeVirtualParams.hRoot = hClient;
    freeVirtualParams.hObjectOld = hMemory;
    ioctl(fd, NV_ESC_RM_FREE, &freeVirtualParams);
    
cleanup_device:
    // 清理 Device
    NVOS00_PARAMETERS freeDeviceParams = {0};
    freeDeviceParams.hRoot = hClient;
    freeDeviceParams.hObjectOld = hDevice;
    ioctl(fd, NV_ESC_RM_FREE, &freeDeviceParams);
    
cleanup_client:
    // 清理 Client
    NVOS00_PARAMETERS freeClientParams = {0};
    freeClientParams.hRoot = hClient;
    freeClientParams.hObjectOld = hClient;
    ioctl(fd, NV_ESC_RM_FREE, &freeClientParams);
    
    close(fd);
    printf("\n测试完成，所有资源已清理\n");
    return 0;
}
