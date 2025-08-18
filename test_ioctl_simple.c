/*
 * test_ioctl_simple.c - 最简单的UVM IOCTL测试
 * 
 * 这个程序只测试最基本的IOCTL调用，用于验证命令号是否正确
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>

// 最简单的IOCTL定义
#define UVM_TEST_RNG_SANITY                         201

// 简单的参数结构
typedef struct {
    int rmStatus;  // 返回状态
} test_params_t;

int main()
{
    int uvm_fd;
    test_params_t params = {0};
    int ret;
    
    printf("=== 简单UVM IOCTL测试 ===\n");
    
    // 打开UVM设备
    uvm_fd = open("/dev/nvidia-uvm", O_RDWR);
    if (uvm_fd < 0) {
        perror("Failed to open /dev/nvidia-uvm");
        return 1;
    }
    
    printf("✅ UVM设备打开成功\n");
    
    // 测试最简单的IOCTL调用
    printf("测试IOCTL命令 %d (RNG_SANITY)...\n", UVM_TEST_RNG_SANITY);
    
    ret = ioctl(uvm_fd, UVM_TEST_RNG_SANITY, &params);
    
    printf("IOCTL返回值: %d\n", ret);
    printf("errno: %d (%s)\n", errno, strerror(errno));
    printf("rmStatus: 0x%x\n", params.rmStatus);
    
    if (ret == 0 && params.rmStatus == 0) {
        printf("✅ 测试成功！\n");
    } else {
        printf("❌ 测试失败\n");
        
        if (ret != 0) {
            printf("可能的原因:\n");
            if (errno == 22) {
                printf("  - IOCTL命令号错误\n");
                printf("  - 参数结构不匹配\n");
                printf("  - UVM测试未正确启用\n");
            } else if (errno == 25) {
                printf("  - 设备不支持此操作\n");
            } else {
                printf("  - 其他系统错误\n");
            }
        }
    }
    
    close(uvm_fd);
    return (ret == 0 && params.rmStatus == 0) ? 0 : 1;
}