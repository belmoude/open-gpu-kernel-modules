/*
 * debug_param_types.c - 调试参数类型和结构
 * 
 * 用于找出正确的参数结构定义
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

// 正确的NV_STATUS定义
typedef uint32_t NV_STATUS;

// 测试不同的参数结构
typedef struct {
    NV_STATUS rmStatus;
} TEST_PARAMS_V1;

typedef struct {
    uint32_t padding;
    NV_STATUS rmStatus;
} TEST_PARAMS_V2;

typedef struct {
    uint64_t padding;
    NV_STATUS rmStatus;
} TEST_PARAMS_V3;

typedef struct {
    NV_STATUS rmStatus;
    uint32_t padding;
} TEST_PARAMS_V4;

// 尝试不同大小的参数结构
typedef struct {
    char data[64];  // 64字节缓冲区
} TEST_PARAMS_LARGE;

static int test_with_different_params(int uvm_fd, unsigned long ioctl_cmd, const char *test_name)
{
    printf("测试 %s (cmd=%lu):\n", test_name, ioctl_cmd);
    
    // 测试1: 最简单的结构
    {
        TEST_PARAMS_V1 params = {0};
        int ret = ioctl(uvm_fd, ioctl_cmd, &params);
        printf("  V1 (4字节): ret=%d, status=0x%x, errno=%s\n", 
               ret, params.rmStatus, strerror(errno));
    }
    
    // 测试2: 带前置padding
    {
        TEST_PARAMS_V2 params = {0};
        int ret = ioctl(uvm_fd, ioctl_cmd, &params);
        printf("  V2 (8字节): ret=%d, status=0x%x, errno=%s\n", 
               ret, params.rmStatus, strerror(errno));
    }
    
    // 测试3: 带64位padding
    {
        TEST_PARAMS_V3 params = {0};
        int ret = ioctl(uvm_fd, ioctl_cmd, &params);
        printf("  V3 (12字节): ret=%d, status=0x%x, errno=%s\n", 
               ret, params.rmStatus, strerror(errno));
    }
    
    // 测试4: 后置padding
    {
        TEST_PARAMS_V4 params = {0};
        int ret = ioctl(uvm_fd, ioctl_cmd, &params);
        printf("  V4 (8字节): ret=%d, status=0x%x, errno=%s\n", 
               ret, params.rmStatus, strerror(errno));
    }
    
    // 测试5: 大缓冲区
    {
        TEST_PARAMS_LARGE params;
        memset(&params, 0, sizeof(params));
        int ret = ioctl(uvm_fd, ioctl_cmd, &params);
        NV_STATUS *status = (NV_STATUS*)&params.data[0];
        printf("  Large (64字节): ret=%d, status=0x%x, errno=%s\n", 
               ret, *status, strerror(errno));
        
        if (ret == 0 && *status == 0) {
            printf("  🎉 Large参数结构成功！\n");
            return 1;
        }
    }
    
    printf("\n");
    return 0;
}

int main()
{
    int uvm_fd;
    
    printf("=== UVM参数结构调试程序 ===\n");
    
    // 打开UVM设备
    uvm_fd = open("/dev/nvidia-uvm", O_RDWR);
    if (uvm_fd < 0) {
        perror("Failed to open UVM device");
        return 1;
    }
    
    printf("✅ UVM设备打开成功\n\n");
    
    // 测试几个不同的IOCTL命令
    printf("=== 测试不同的参数结构 ===\n");
    
    // 测试已知能工作的RANGE_ALLOCATOR_SANITY
    printf("1. 测试RANGE_ALLOCATOR_SANITY (已知能通过):\n");
    {
        struct {
            uint32_t verbose;
            uint32_t seed;
            uint32_t iters;
            NV_STATUS rmStatus;
        } params = {0, 12345, 100, 0};
        
        int ret = ioctl(uvm_fd, 225, &params);  // UVM_TEST_RANGE_ALLOCATOR_SANITY
        printf("  正确结构: ret=%d, status=0x%x, errno=%s\n", 
               ret, params.rmStatus, strerror(errno));
        
        if (ret == 0 && params.rmStatus == 0) {
            printf("  ✅ 确认：这个测试能正常工作\n");
        }
    }
    
    printf("\n2. 测试简单的RNG_SANITY:\n");
    test_with_different_params(uvm_fd, 201, "RNG_SANITY");
    
    printf("3. 测试LOCK_SANITY:\n");
    test_with_different_params(uvm_fd, 218, "LOCK_SANITY");
    
    close(uvm_fd);
    
    printf("=== 调试结论 ===\n");
    printf("请查看上面的输出，找出哪种参数结构格式能成功\n");
    printf("如果Large参数结构成功，说明需要更大的缓冲区\n");
    printf("如果都失败，可能需要检查其他问题\n");
    
    return 0;
}