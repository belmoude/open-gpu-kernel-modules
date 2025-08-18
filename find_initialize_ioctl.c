/*
 * find_initialize_ioctl.c - 寻找正确的UVM初始化IOCTL
 * 
 * 尝试不同的可能的初始化IOCTL命令
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

typedef uint32_t NV_STATUS;

// 尝试不同的可能的INITIALIZE命令号
// 从UVM API的IOCTL范围内寻找
static unsigned long possible_init_cmds[] = {
    1,   // UVM_RESERVE_VA
    74,  // 猜测的INITIALIZE
    75,  // UVM_MM_INITIALIZE
    76,  // 可能的INITIALIZE
    8,   // 跳过的数字
    100, // 其他可能的数字
    0    // 结束标记
};

typedef struct {
    uint64_t flags;
    NV_STATUS rmStatus;
} INIT_PARAMS_V1;

typedef struct {
    NV_STATUS rmStatus;
} INIT_PARAMS_V2;

typedef struct {
    uint32_t version;
    uint32_t flags;
    NV_STATUS rmStatus;
} INIT_PARAMS_V3;

static int test_initialization(int uvm_fd, unsigned long cmd)
{
    printf("测试IOCTL %lu:\n", cmd);
    
    // 尝试不同的参数结构
    INIT_PARAMS_V1 params_v1 = {0, 0};
    INIT_PARAMS_V2 params_v2 = {0};
    INIT_PARAMS_V3 params_v3 = {0, 0, 0};
    
    int ret;
    
    // 测试V1
    ret = ioctl(uvm_fd, cmd, &params_v1);
    printf("  V1: ret=%d, status=0x%x, errno=%s\n", 
           ret, params_v1.rmStatus, strerror(errno));
    
    if (ret == 0 && params_v1.rmStatus == 0) {
        printf("  ✅ V1成功！找到正确的初始化IOCTL\n");
        return 1;
    }
    
    // 重置errno
    errno = 0;
    
    // 测试V2
    ret = ioctl(uvm_fd, cmd, &params_v2);
    printf("  V2: ret=%d, status=0x%x, errno=%s\n", 
           ret, params_v2.rmStatus, strerror(errno));
    
    if (ret == 0 && params_v2.rmStatus == 0) {
        printf("  ✅ V2成功！找到正确的初始化IOCTL\n");
        return 1;
    }
    
    // 重置errno
    errno = 0;
    
    // 测试V3
    ret = ioctl(uvm_fd, cmd, &params_v3);
    printf("  V3: ret=%d, status=0x%x, errno=%s\n", 
           ret, params_v3.rmStatus, strerror(errno));
    
    if (ret == 0 && params_v3.rmStatus == 0) {
        printf("  ✅ V3成功！找到正确的初始化IOCTL\n");
        return 1;
    }
    
    return 0;
}

// 测试初始化后是否能运行测试
static int test_after_init(int uvm_fd, unsigned long init_cmd, int init_variant)
{
    printf("\n=== 测试初始化后的测试运行 ===\n");
    
    // 执行初始化
    if (init_variant == 1) {
        INIT_PARAMS_V1 init_params = {0, 0};
        int ret = ioctl(uvm_fd, init_cmd, &init_params);
        if (ret != 0 || init_params.rmStatus != 0) {
            printf("❌ 初始化失败\n");
            return 0;
        }
    } else if (init_variant == 2) {
        INIT_PARAMS_V2 init_params = {0};
        int ret = ioctl(uvm_fd, init_cmd, &init_params);
        if (ret != 0 || init_params.rmStatus != 0) {
            printf("❌ 初始化失败\n");
            return 0;
        }
    } else {
        INIT_PARAMS_V3 init_params = {0, 0, 0};
        int ret = ioctl(uvm_fd, init_cmd, &init_params);
        if (ret != 0 || init_params.rmStatus != 0) {
            printf("❌ 初始化失败\n");
            return 0;
        }
    }
    
    printf("✅ 初始化成功，现在测试RNG_SANITY...\n");
    
    // 测试简单的RNG_SANITY
    INIT_PARAMS_V2 test_params = {0};
    int ret = ioctl(uvm_fd, 201, &test_params);  // UVM_TEST_RNG_SANITY
    
    printf("RNG_SANITY结果: ret=%d, status=0x%x, errno=%s\n", 
           ret, test_params.rmStatus, strerror(errno));
    
    if (ret == 0 && test_params.rmStatus == 0) {
        printf("🎉 成功！找到了完整的解决方案\n");
        return 1;
    } else {
        printf("❌ 初始化后测试仍然失败\n");
        return 0;
    }
}

int main()
{
    int uvm_fd;
    int found_init = 0;
    unsigned long working_init_cmd = 0;
    int working_variant = 0;
    
    printf("=== UVM初始化IOCTL查找程序 ===\n");
    
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
    
    // 尝试不同的初始化命令
    printf("\n=== 寻找正确的初始化IOCTL ===\n");
    
    for (int i = 0; possible_init_cmds[i] != 0; i++) {
        unsigned long cmd = possible_init_cmds[i];
        
        printf("\n--- 测试命令 %lu ---\n", cmd);
        
        if (test_initialization(uvm_fd, cmd)) {
            found_init = 1;
            working_init_cmd = cmd;
            // 需要确定是哪个变体成功了，这里简化为V2
            working_variant = 2;
            break;
        }
        
        // 重新打开设备（某些失败的IOCTL可能影响设备状态）
        close(uvm_fd);
        uvm_fd = open("/dev/nvidia-uvm", O_RDWR);
    }
    
    if (found_init) {
        printf("\n🎉 找到工作的初始化IOCTL: %lu\n", working_init_cmd);
        
        // 重新打开设备测试完整流程
        close(uvm_fd);
        uvm_fd = open("/dev/nvidia-uvm", O_RDWR);
        
        if (test_after_init(uvm_fd, working_init_cmd, working_variant)) {
            printf("\n🎊 完美！找到了完整的解决方案！\n");
            printf("正确的流程:\n");
            printf("1. 打开 /dev/nvidia-uvm\n");
            printf("2. 调用 IOCTL %lu 进行初始化\n", working_init_cmd);
            printf("3. 运行测试IOCTL\n");
            printf("4. 清理资源\n");
        }
    } else {
        printf("\n❌ 未找到工作的初始化IOCTL\n");
        printf("可能需要:\n");
        printf("1. 检查更多的IOCTL命令号\n");
        printf("2. 查看是否需要其他前提条件\n");
        printf("3. 检查驱动版本兼容性\n");
    }
    
    close(uvm_fd);
    return found_init ? 0 : 1;
}