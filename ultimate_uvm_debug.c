/*
 * ultimate_uvm_debug.c - 终极UVM调试程序
 * 
 * 这个程序会:
 * 1. 测试不同的参数结构格式
 * 2. 尝试不同的数据类型定义
 * 3. 检查内存对齐问题
 * 4. 提供详细的诊断信息
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

// 尝试不同的NV_STATUS定义
typedef uint32_t NV_STATUS_U32;
typedef int32_t  NV_STATUS_I32;
typedef uint64_t NV_STATUS_U64;

// 不同的参数结构变体
typedef struct {
    NV_STATUS_U32 rmStatus;
} __attribute__((packed)) PARAMS_U32_PACKED;

typedef struct {
    NV_STATUS_U32 rmStatus;
} PARAMS_U32_ALIGNED;

typedef struct {
    NV_STATUS_I32 rmStatus;
} PARAMS_I32;

typedef struct {
    NV_STATUS_U64 rmStatus;
} PARAMS_U64;

// 大缓冲区用于测试
typedef struct {
    char buffer[256];
} PARAMS_LARGE_BUFFER;

// 测试函数
static void test_param_variant(int uvm_fd, unsigned long ioctl_cmd, const char *variant_name, 
                              void *params, size_t param_size, uint32_t *status_ptr)
{
    int ret;
    
    printf("    %-20s (大小:%2zu) ... ", variant_name, param_size);
    fflush(stdout);
    
    // 清零参数
    memset(params, 0, param_size);
    
    // 执行IOCTL
    ret = ioctl(uvm_fd, ioctl_cmd, params);
    
    // 获取状态
    uint32_t status = status_ptr ? *status_ptr : 0;
    
    if (ret == 0 && status == 0) {
        printf("✅ SUCCESS!\n");
    } else {
        printf("❌ ret=%d, status=0x%x, errno=%s\n", ret, status, strerror(errno));
    }
}

static void comprehensive_test(int uvm_fd, unsigned long ioctl_cmd, const char *test_name)
{
    printf("\n=== 测试 %s (IOCTL %lu) ===\n", test_name, ioctl_cmd);
    
    // 测试不同的参数结构
    PARAMS_U32_PACKED   params_u32_packed;
    PARAMS_U32_ALIGNED  params_u32_aligned;
    PARAMS_I32          params_i32;
    PARAMS_U64          params_u64;
    PARAMS_LARGE_BUFFER params_large;
    
    test_param_variant(uvm_fd, ioctl_cmd, "U32_PACKED", 
                      &params_u32_packed, sizeof(params_u32_packed), &params_u32_packed.rmStatus);
    
    test_param_variant(uvm_fd, ioctl_cmd, "U32_ALIGNED", 
                      &params_u32_aligned, sizeof(params_u32_aligned), &params_u32_aligned.rmStatus);
    
    test_param_variant(uvm_fd, ioctl_cmd, "I32", 
                      &params_i32, sizeof(params_i32), (uint32_t*)&params_i32.rmStatus);
    
    test_param_variant(uvm_fd, ioctl_cmd, "U64", 
                      &params_u64, sizeof(params_u64), (uint32_t*)&params_u64.rmStatus);
    
    test_param_variant(uvm_fd, ioctl_cmd, "LARGE_BUFFER", 
                      &params_large, sizeof(params_large), (uint32_t*)&params_large.buffer[0]);
}

// 测试已知工作的RANGE_ALLOCATOR_SANITY
static void test_known_working(int uvm_fd)
{
    printf("\n=== 测试已知工作的RANGE_ALLOCATOR_SANITY ===\n");
    
    typedef struct {
        uint32_t verbose;
        uint32_t seed;
        uint32_t iters;
        NV_STATUS_U32 rmStatus;
    } RANGE_ALLOC_PARAMS;
    
    RANGE_ALLOC_PARAMS params = {0, 12345, 100, 0};
    
    printf("参数结构大小: %zu 字节\n", sizeof(params));
    printf("字段偏移:\n");
    printf("  verbose: %zu\n", offsetof(RANGE_ALLOC_PARAMS, verbose));
    printf("  seed: %zu\n", offsetof(RANGE_ALLOC_PARAMS, seed));
    printf("  iters: %zu\n", offsetof(RANGE_ALLOC_PARAMS, iters));
    printf("  rmStatus: %zu\n", offsetof(RANGE_ALLOC_PARAMS, rmStatus));
    
    int ret = ioctl(uvm_fd, 225, &params);
    printf("结果: ret=%d, status=0x%x, errno=%s\n", 
           ret, params.rmStatus, strerror(errno));
    
    if (ret == 0 && params.rmStatus == 0) {
        printf("✅ 确认：RANGE_ALLOCATOR_SANITY 工作正常\n");
        printf("这说明IOCTL接口本身是正常的\n");
    } else {
        printf("❌ 奇怪：之前能工作的测试现在失败了\n");
    }
}

int main()
{
    int uvm_fd;
    
    printf("=== UVM参数结构终极调试程序 ===\n");
    printf("目标: 找出所有测试失败的根本原因\n");
    
    // 检查权限
    if (geteuid() != 0) {
        printf("❌ 需要root权限\n");
        return 1;
    }
    
    // 打开UVM设备
    uvm_fd = open("/dev/nvidia-uvm", O_RDWR);
    if (uvm_fd < 0) {
        perror("Failed to open UVM device");
        return 1;
    }
    
    printf("✅ UVM设备打开成功\n");
    
    // 显示数据类型大小
    printf("\n=== 数据类型大小信息 ===\n");
    printf("sizeof(int): %zu\n", sizeof(int));
    printf("sizeof(uint32_t): %zu\n", sizeof(uint32_t));
    printf("sizeof(uint64_t): %zu\n", sizeof(uint64_t));
    printf("sizeof(NV_STATUS_U32): %zu\n", sizeof(NV_STATUS_U32));
    
    // 测试已知工作的用例
    test_known_working(uvm_fd);
    
    // 测试简单的用例
    comprehensive_test(uvm_fd, 201, "RNG_SANITY");
    comprehensive_test(uvm_fd, 218, "LOCK_SANITY");
    comprehensive_test(uvm_fd, 220, "KVMALLOC");
    
    close(uvm_fd);
    
    printf("\n=== 调试结论 ===\n");
    printf("1. 如果RANGE_ALLOCATOR_SANITY仍然工作，说明IOCTL基础设施正常\n");
    printf("2. 如果某个参数变体成功，说明找到了正确的结构格式\n");
    printf("3. 如果都失败，可能需要检查其他因素（如VA space状态）\n");
    
    return 0;
}