/*
 * test_with_va_space.c - 带VA Space的UVM测试
 * 
 * 许多UVM测试需要先创建VA space上下文
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

// UVM IOCTL定义
#define UVM_INITIALIZE                              1
#define UVM_DEINITIALIZE                            2
#define UVM_CREATE_VA_SPACE                         39
#define UVM_DESTROY_VA_SPACE                        40

// 测试IOCTL
#define UVM_TEST_RNG_SANITY                         201

// 参数结构定义
typedef struct {
    int rmStatus;
} UVM_INITIALIZE_PARAMS;

typedef struct {
    int rmStatus;
} UVM_DEINITIALIZE_PARAMS;

typedef struct {
    uint64_t vaSpace;  // OUT: VA space handle
    int rmStatus;      // OUT: status
} UVM_CREATE_VA_SPACE_PARAMS;

typedef struct {
    uint64_t vaSpace;  // IN: VA space handle
    int rmStatus;      // OUT: status
} UVM_DESTROY_VA_SPACE_PARAMS;

typedef struct {
    int rmStatus;
} UVM_TEST_RNG_SANITY_PARAMS;

int main()
{
    int uvm_fd;
    int ret;
    
    UVM_INITIALIZE_PARAMS init_params = {0};
    UVM_CREATE_VA_SPACE_PARAMS va_space_params = {0};
    UVM_TEST_RNG_SANITY_PARAMS test_params = {0};
    UVM_DESTROY_VA_SPACE_PARAMS destroy_params = {0};
    UVM_DEINITIALIZE_PARAMS deinit_params = {0};
    
    printf("=== UVM测试（带VA Space） ===\n");
    
    // 打开UVM设备
    uvm_fd = open("/dev/nvidia-uvm", O_RDWR);
    if (uvm_fd < 0) {
        perror("Failed to open /dev/nvidia-uvm");
        return 1;
    }
    
    printf("✅ UVM设备打开成功\n");
    
    // 步骤1: 初始化UVM
    printf("1. 初始化UVM...\n");
    ret = ioctl(uvm_fd, UVM_INITIALIZE, &init_params);
    printf("   返回值: %d, errno: %d (%s), rmStatus: 0x%x\n", 
           ret, errno, strerror(errno), init_params.rmStatus);
    
    if (ret != 0 || init_params.rmStatus != 0) {
        printf("❌ UVM初始化失败\n");
        goto cleanup;
    }
    printf("✅ UVM初始化成功\n");
    
    // 步骤2: 创建VA Space
    printf("2. 创建VA Space...\n");
    ret = ioctl(uvm_fd, UVM_CREATE_VA_SPACE, &va_space_params);
    printf("   返回值: %d, errno: %d (%s), rmStatus: 0x%x, vaSpace: 0x%lx\n", 
           ret, errno, strerror(errno), va_space_params.rmStatus, va_space_params.vaSpace);
    
    if (ret != 0 || va_space_params.rmStatus != 0) {
        printf("❌ VA Space创建失败\n");
        goto cleanup;
    }
    printf("✅ VA Space创建成功\n");
    
    // 步骤3: 运行测试
    printf("3. 运行RNG测试...\n");
    ret = ioctl(uvm_fd, UVM_TEST_RNG_SANITY, &test_params);
    printf("   返回值: %d, errno: %d (%s), rmStatus: 0x%x\n", 
           ret, errno, strerror(errno), test_params.rmStatus);
    
    if (ret == 0 && test_params.rmStatus == 0) {
        printf("✅ RNG测试成功！\n");
    } else {
        printf("❌ RNG测试失败\n");
    }
    
    // 步骤4: 清理VA Space
    printf("4. 销毁VA Space...\n");
    destroy_params.vaSpace = va_space_params.vaSpace;
    ret = ioctl(uvm_fd, UVM_DESTROY_VA_SPACE, &destroy_params);
    printf("   返回值: %d, errno: %d (%s), rmStatus: 0x%x\n", 
           ret, errno, strerror(errno), destroy_params.rmStatus);
    
cleanup:
    // 步骤5: 反初始化UVM
    printf("5. 反初始化UVM...\n");
    ret = ioctl(uvm_fd, UVM_DEINITIALIZE, &deinit_params);
    printf("   返回值: %d, errno: %d (%s), rmStatus: 0x%x\n", 
           ret, errno, strerror(errno), deinit_params.rmStatus);
    
    close(uvm_fd);
    
    printf("\n=== 调试完成 ===\n");
    printf("如果所有步骤都成功，说明UVM IOCTL工作正常\n");
    printf("如果某个步骤失败，请查看上面的错误信息\n");
    
    return 0;
}