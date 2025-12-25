/*
 * 修复版测试程序：使用正确的 NV_ESC_IOCTL_XFER_CMD 方式
 * 
 * 编译：gcc -o test_virtual_alloc_fixed test_virtual_alloc_fixed.c -std=c99
 * 运行：sudo ./test_virtual_alloc_fixed
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <errno.h>

// NVIDIA 类型定义
typedef uint32_t NvU32;
typedef uint64_t NvU64;
typedef int32_t  NvS32;
typedef void*    NvP64;
typedef uint8_t  NvU8;
typedef uint16_t NvU16;
typedef int NvBool;

#define NV_ALIGN_BYTES(size) __attribute__ ((aligned (size)))
#define NV_TRUE  1
#define NV_FALSE 0

// ioctl 定义
#define NV_IOCTL_MAGIC      'F'
#define NV_IOCTL_BASE       200
#define NV_ESC_IOCTL_XFER_CMD        (NV_IOCTL_BASE + 11)

// Escape codes
#define NV_ESC_RM_ALLOC                     0x2B
#define NV_ESC_RM_FREE                      0x29
#define NV_ESC_RM_VID_HEAP_CONTROL          0x4A

// ioctl xfer 结构
typedef struct nv_ioctl_xfer
{
    NvU32   cmd;
    NvU32   size;
    NvU64   ptr  NV_ALIGN_BYTES(8);
} nv_ioctl_xfer_t;

// NV 类定义
#define NV01_ROOT_CLIENT              0x00000041
#define NV01_DEVICE_0                 0x00000080
#define NV50_MEMORY_VIRTUAL           0x000050a0

// NVOS32 定义
#define NVOS32_FUNCTION_ALLOC_SIZE    2
#define NVOS32_FUNCTION_FREE          3

// 标志定义
#define NVOS32_ALLOC_FLAGS_VIRTUAL                  0x00080000
#define NVOS32_ALLOC_FLAGS_LAZY                     0x00000400
#define NVOS32_ALLOC_FLAGS_ALIGNMENT_FORCE          0x00000100
#define NVOS32_ALLOC_FLAGS_MEMORY_HANDLE_PROVIDED   0x00004000

// 属性定义
#define NVOS32_ATTR_LOCATION_VIDMEM       0x00000002

// 参数结构
typedef struct {
    NvU32  hRoot;
    NvU32  hObjectParent;
    NvU32  hObjectNew;
    NvU32  hClass;
    void  *pAllocParms;
    NvU32  status;
    NvU32  rightsRequested;
    NvU32  flags;
} NVOS64_PARAMETERS;

typedef struct {
    NvU32  hRoot;
    NvU32  hObjectOld;
    NvU32  status;
} NVOS00_PARAMETERS;

typedef struct {
    NvU32  owner;
    NvU32  type;
    NvU32  flags;
    NvU32  attr;
    NvU32  attr2;
    NvU64  size NV_ALIGN_BYTES(8);
    NvU64  alignment NV_ALIGN_BYTES(8);
    NvU64  offset NV_ALIGN_BYTES(8);
    NvU64  limit NV_ALIGN_BYTES(8);
    NvU64  rangeLo NV_ALIGN_BYTES(8);
    NvU64  rangeHi NV_ALIGN_BYTES(8);
    NvU32  hVASpace;
} NV_MEMORY_ALLOCATION_PARAMS;

typedef struct {
    NvU32     owner;
    NvU32     hMemory;
    NvU32     type;
    NvU32     flags;
    NvU32     attr;
    NvU32     format;
    NvU32     comprCovg;
    NvU32     zcullCovg;
    NvU32     partitionStride;
    NvU32     width;
    NvU32     height;
    NvU64     size NV_ALIGN_BYTES(8);
    NvU64     alignment NV_ALIGN_BYTES(8);
    NvU64     offset NV_ALIGN_BYTES(8);
    NvU64     limit NV_ALIGN_BYTES(8);
    NvU64     address NV_ALIGN_BYTES(8);
    NvU64     rangeBegin NV_ALIGN_BYTES(8);
    NvU64     rangeEnd NV_ALIGN_BYTES(8);
    NvU32     attr2;
    NvU32     ctagOffset;
    NvS32     numaNode;
} AllocSizeParams;

typedef struct {
    NvU32  hRoot;
    NvU32  hObjectParent;
    NvU32  function;
    NvU32  hVASpace;
    NvS32  ivcHeapNumber;
    NvU32  status;
    NvU64  total NV_ALIGN_BYTES(8);
    NvU64  free  NV_ALIGN_BYTES(8);
    union {
        AllocSizeParams AllocSize;
        struct {
            NvU32 owner;
            NvU32 hMemory;
            NvU32 flags;
        } Free;
    } data;
} NVOS32_PARAMETERS;

// ioctl 封装函数
int nv_ioctl(int fd, NvU32 cmd, void *params, NvU32 size) {
    nv_ioctl_xfer_t xfer;
    xfer.cmd = cmd;
    xfer.size = size;
    xfer.ptr = (NvU64)(uintptr_t)params;
    
    return ioctl(fd, _IOWR(NV_IOCTL_MAGIC, NV_ESC_IOCTL_XFER_CMD, nv_ioctl_xfer_t), &xfer);
}

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
    
    printf("=== NVIDIA 虚拟内存分配测试（修复版）===\n\n");
    
    // 打开设备
    fd = open("/dev/nvidiactl", O_RDWR);
    if (fd < 0) {
        perror("无法打开 /dev/nvidiactl");
        return 1;
    }
    printf("✅ 成功打开 /dev/nvidiactl\n\n");
    
    // 1. 分配 Client
    printf(">>> 步骤 1: 分配 Client\n");
    NVOS64_PARAMETERS clientParams = {0};
    clientParams.hRoot = 0;
    clientParams.hObjectParent = 0;
    clientParams.hObjectNew = 0;
    clientParams.hClass = NV01_ROOT_CLIENT;
    clientParams.pAllocParms = NULL;
    
    ret = nv_ioctl(fd, NV_ESC_RM_ALLOC, &clientParams, sizeof(clientParams));
    if (ret != 0 || clientParams.status != 0) {
        printf("❌ 分配 Client 失败:\n");
        printf("   ioctl 返回: %d\n", ret);
        printf("   errno: %d (%s)\n", errno, strerror(errno));
        printf("   status: 0x%x\n", clientParams.status);
        close(fd);
        return 1;
    }
    hClient = clientParams.hObjectNew;
    printf("✅ 成功分配 Client: 0x%x\n\n", hClient);
    
    // 2. 分配 Device
    printf(">>> 步骤 2: 分配 Device\n");
    NVOS64_PARAMETERS deviceParams = {0};
    deviceParams.hRoot = hClient;
    deviceParams.hObjectParent = hClient;
    deviceParams.hObjectNew = 0x12340000;
    deviceParams.hClass = NV01_DEVICE_0;
    deviceParams.pAllocParms = NULL;
    
    ret = nv_ioctl(fd, NV_ESC_RM_ALLOC, &deviceParams, sizeof(deviceParams));
    if (ret != 0 || deviceParams.status != 0) {
        printf("❌ 分配 Device 失败:\n");
        printf("   ioctl 返回: %d\n", ret);
        printf("   errno: %d (%s)\n", errno, strerror(errno));
        printf("   status: 0x%x\n", deviceParams.status);
        goto cleanup_client;
    }
    hDevice = deviceParams.hObjectNew;
    printf("✅ 成功分配 Device: 0x%x\n\n", hDevice);
    
    print_memory_info("初始状态 - 未分配任何显存");
    
    // 3. 场景1: 只有 VIRTUAL 标志
    printf("\n>>> 场景 1: 只有 VIRTUAL 标志 - 不应占用数据显存\n");
    NV_MEMORY_ALLOCATION_PARAMS virtParams1 = {0};
    virtParams1.owner = hClient;
    virtParams1.flags = NVOS32_ALLOC_FLAGS_VIRTUAL |
                        NVOS32_ALLOC_FLAGS_ALIGNMENT_FORCE;
    virtParams1.attr = NVOS32_ATTR_LOCATION_VIDMEM;
    virtParams1.size = 1024 * 1024 * 1024;  // 1GB
    virtParams1.alignment = 4096;
    virtParams1.hVASpace = 0;
    
    NVOS64_PARAMETERS virtAlloc1 = {0};
    virtAlloc1.hRoot = hClient;
    virtAlloc1.hObjectParent = hDevice;
    virtAlloc1.hObjectNew = 0x87654321;
    virtAlloc1.hClass = NV50_MEMORY_VIRTUAL;
    virtAlloc1.pAllocParms = &virtParams1;
    
    ret = nv_ioctl(fd, NV_ESC_RM_ALLOC, &virtAlloc1, sizeof(virtAlloc1));
    if (ret != 0 || virtAlloc1.status != 0) {
        printf("❌ 场景1 分配失败:\n");
        printf("   ioctl 返回: %d\n", ret);
        printf("   errno: %d (%s)\n", errno, strerror(errno));
        printf("   status: 0x%x\n", virtAlloc1.status);
    } else {
        printf("✅ 场景1 成功: 0x%x (大小: %llu MB)\n", 
               virtAlloc1.hObjectNew, 
               (unsigned long long)(virtParams1.size / 1024 / 1024));
        print_memory_info("场景1 后 - nvidia-smi 应基本不变");
        
        // 释放
        NVOS00_PARAMETERS freeParams1 = {0};
        freeParams1.hRoot = hClient;
        freeParams1.hObjectOld = virtAlloc1.hObjectNew;
        nv_ioctl(fd, NV_ESC_RM_FREE, &freeParams1, sizeof(freeParams1));
    }
    
    // 4. 场景2: VIRTUAL + LAZY
    printf("\n>>> 场景 2: VIRTUAL + LAZY - 完全不占用显存\n");
    NV_MEMORY_ALLOCATION_PARAMS virtParams2 = {0};
    virtParams2.owner = hClient;
    virtParams2.flags = NVOS32_ALLOC_FLAGS_VIRTUAL |
                        NVOS32_ALLOC_FLAGS_LAZY |
                        NVOS32_ALLOC_FLAGS_ALIGNMENT_FORCE;
    virtParams2.attr = NVOS32_ATTR_LOCATION_VIDMEM;
    virtParams2.size = 1024 * 1024 * 1024;  // 1GB
    virtParams2.alignment = 4096;
    virtParams2.hVASpace = 0;
    
    NVOS64_PARAMETERS virtAlloc2 = {0};
    virtAlloc2.hRoot = hClient;
    virtAlloc2.hObjectParent = hDevice;
    virtAlloc2.hObjectNew = 0x98765432;
    virtAlloc2.hClass = NV50_MEMORY_VIRTUAL;
    virtAlloc2.pAllocParms = &virtParams2;
    
    ret = nv_ioctl(fd, NV_ESC_RM_ALLOC, &virtAlloc2, sizeof(virtAlloc2));
    if (ret != 0 || virtAlloc2.status != 0) {
        printf("❌ 场景2 分配失败:\n");
        printf("   ioctl 返回: %d\n", ret);
        printf("   errno: %d (%s)\n", errno, strerror(errno));
        printf("   status: 0x%x\n", virtAlloc2.status);
    } else {
        printf("✅ 场景2 成功: 0x%x (大小: %llu MB)\n", 
               virtAlloc2.hObjectNew, 
               (unsigned long long)(virtParams2.size / 1024 / 1024));
        print_memory_info("场景2 后 - nvidia-smi 应完全不变");
        
        // 释放
        NVOS00_PARAMETERS freeParams2 = {0};
        freeParams2.hRoot = hClient;
        freeParams2.hObjectOld = virtAlloc2.hObjectNew;
        nv_ioctl(fd, NV_ESC_RM_FREE, &freeParams2, sizeof(freeParams2));
    }
    
    // 5. 场景3: 物理显存分配（对比）
    printf("\n>>> 场景 3: 物理显存分配 - 应立即占用显存\n");
    NVOS32_PARAMETERS physParams = {0};
    physParams.hRoot = hClient;
    physParams.hObjectParent = hDevice;
    physParams.function = NVOS32_FUNCTION_ALLOC_SIZE;
    physParams.data.AllocSize.owner = hClient;
    physParams.data.AllocSize.hMemory = 0;
    physParams.data.AllocSize.flags = 0;  // 无 VIRTUAL
    physParams.data.AllocSize.attr = NVOS32_ATTR_LOCATION_VIDMEM;
    physParams.data.AllocSize.size = 256 * 1024 * 1024;  // 256MB
    physParams.data.AllocSize.alignment = 4096;
    
    ret = nv_ioctl(fd, NV_ESC_RM_VID_HEAP_CONTROL, &physParams, sizeof(physParams));
    if (ret != 0 || physParams.status != 0) {
        printf("❌ 场景3 分配失败:\n");
        printf("   ioctl 返回: %d\n", ret);
        printf("   errno: %d (%s)\n", errno, strerror(errno));
        printf("   status: 0x%x\n", physParams.status);
    } else {
        NvU32 hPhysMem = physParams.data.AllocSize.hMemory;
        printf("✅ 场景3 成功: 0x%x (大小: %llu MB)\n", 
               hPhysMem, 
               (unsigned long long)(physParams.data.AllocSize.size / 1024 / 1024));
        print_memory_info("场景3 后 - nvidia-smi 应增加约 256MB");
        
        // 释放
        NVOS32_PARAMETERS freePhysParams = {0};
        freePhysParams.hRoot = hClient;
        freePhysParams.hObjectParent = hDevice;
        freePhysParams.function = NVOS32_FUNCTION_FREE;
        freePhysParams.data.Free.owner = hClient;
        freePhysParams.data.Free.hMemory = hPhysMem;
        freePhysParams.data.Free.flags = NVOS32_ALLOC_FLAGS_MEMORY_HANDLE_PROVIDED;
        nv_ioctl(fd, NV_ESC_RM_VID_HEAP_CONTROL, &freePhysParams, sizeof(freePhysParams));
        printf("已释放物理显存\n");
    }
    
    // 清理
    {
        NVOS00_PARAMETERS freeDevice = {0};
        freeDevice.hRoot = hClient;
        freeDevice.hObjectOld = hDevice;
        nv_ioctl(fd, NV_ESC_RM_FREE, &freeDevice, sizeof(freeDevice));
    }
    
cleanup_client:
    {
        NVOS00_PARAMETERS freeClient = {0};
        freeClient.hRoot = hClient;
        freeClient.hObjectOld = hClient;
        nv_ioctl(fd, NV_ESC_RM_FREE, &freeClient, sizeof(freeClient));
    }
    
    close(fd);
    printf("\n=== 测试完成，所有资源已清理 ===\n");
    return 0;
}
