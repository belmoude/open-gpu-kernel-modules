/*
 * simple_uvm_test.c - 最简单的UVM测试程序
 * 
 * 编译: gcc -o simple_uvm_test simple_uvm_test.c
 * 运行: sudo ./simple_uvm_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>

// UVM IOCTL基础定义 - Linux下就是简单的数字
#define UVM_IOCTL_BASE(i)                           (i)
#define UVM_TEST_IOCTL_BASE(i)                      UVM_IOCTL_BASE(200 + i)

// 测试命令定义 - 正确的命令号
#define UVM_TEST_RNG_SANITY                         201  // UVM_TEST_IOCTL_BASE(1)
#define UVM_TEST_RANGE_TREE_DIRECTED                202  // UVM_TEST_IOCTL_BASE(2)
#define UVM_TEST_RANGE_TREE_RANDOM                  203  // UVM_TEST_IOCTL_BASE(3)
#define UVM_TEST_RM_MEM_SANITY                      205  // UVM_TEST_IOCTL_BASE(5)
#define UVM_TEST_GPU_SEMAPHORE_SANITY               206  // UVM_TEST_IOCTL_BASE(6)
#define UVM_TEST_TRACKER_SANITY                     212  // UVM_TEST_IOCTL_BASE(12)
#define UVM_TEST_PUSH_SANITY                        213  // UVM_TEST_IOCTL_BASE(13)
#define UVM_TEST_CHANNEL_SANITY                     214  // UVM_TEST_IOCTL_BASE(14)
#define UVM_TEST_CE_SANITY                          216  // UVM_TEST_IOCTL_BASE(16)
#define UVM_TEST_LOCK_SANITY                        218  // UVM_TEST_IOCTL_BASE(18)
#define UVM_TEST_PERF_UTILS_SANITY                  219  // UVM_TEST_IOCTL_BASE(19)
#define UVM_TEST_KVMALLOC                           220  // UVM_TEST_IOCTL_BASE(20)
#define UVM_TEST_PERF_EVENTS_SANITY                 223  // UVM_TEST_IOCTL_BASE(23)
#define UVM_TEST_PERF_MODULE_SANITY                 224  // UVM_TEST_IOCTL_BASE(24)
#define UVM_TEST_RANGE_ALLOCATOR_SANITY             225  // UVM_TEST_IOCTL_BASE(25)
#define UVM_TEST_FAULT_BUFFER_FLUSH                 227  // UVM_TEST_IOCTL_BASE(27)
#define UVM_TEST_SEC2_SANITY                        295  // UVM_TEST_IOCTL_BASE(95)
#define UVM_TEST_SEC2_CPU_GPU_ROUNDTRIP             299  // UVM_TEST_IOCTL_BASE(99)

// 简化的参数结构
typedef struct {
    int rmStatus;  // 返回状态
} simple_test_params_t;

// 测试用例定义
typedef struct {
    unsigned long ioctl_cmd;
    const char *name;
    const char *description;
} test_case_t;

// 测试用例列表
static test_case_t test_cases[] = {
    {UVM_TEST_RNG_SANITY, "RNG_SANITY", "Random number generator sanity test"},
    {UVM_TEST_RANGE_TREE_DIRECTED, "RANGE_TREE_DIRECTED", "Range tree directed test"},
    {UVM_TEST_RM_MEM_SANITY, "RM_MEM_SANITY", "RM memory sanity test"},
    {UVM_TEST_GPU_SEMAPHORE_SANITY, "GPU_SEMAPHORE_SANITY", "GPU semaphore sanity test"},
    {UVM_TEST_TRACKER_SANITY, "TRACKER_SANITY", "Tracker sanity test"},
    {UVM_TEST_PUSH_SANITY, "PUSH_SANITY", "Push sanity test"},
    {UVM_TEST_CHANNEL_SANITY, "CHANNEL_SANITY", "Channel sanity test"},
    {UVM_TEST_CE_SANITY, "CE_SANITY", "Copy Engine sanity test"},
    {UVM_TEST_LOCK_SANITY, "LOCK_SANITY", "Lock sanity test"},
    {UVM_TEST_PERF_UTILS_SANITY, "PERF_UTILS_SANITY", "Performance utils sanity test"},
    {UVM_TEST_KVMALLOC, "KVMALLOC", "Kernel memory allocation test"},
    {UVM_TEST_PERF_EVENTS_SANITY, "PERF_EVENTS_SANITY", "Performance events sanity test"},
    {UVM_TEST_PERF_MODULE_SANITY, "PERF_MODULE_SANITY", "Performance module sanity test"},
    {UVM_TEST_RANGE_ALLOCATOR_SANITY, "RANGE_ALLOCATOR_SANITY", "Range allocator sanity test"},
    {UVM_TEST_FAULT_BUFFER_FLUSH, "FAULT_BUFFER_FLUSH", "Fault buffer flush test"},
    {UVM_TEST_SEC2_SANITY, "SEC2_SANITY", "SEC2 engine sanity test (Confidential Computing)"},
    {UVM_TEST_SEC2_CPU_GPU_ROUNDTRIP, "SEC2_CPU_GPU_ROUNDTRIP", "SEC2 CPU-GPU roundtrip test (Confidential Computing)"},
};

static int check_uvm_environment(void)
{
    FILE *fp;
    char buffer[16];
    
    printf("=== Checking UVM Test Environment ===\n");
    
    // 检查UVM设备
    if (access("/dev/nvidia-uvm", F_OK) != 0) {
        printf("✗ /dev/nvidia-uvm not found\n");
        printf("  Make sure NVIDIA UVM driver is loaded\n");
        return 0;
    }
    printf("✓ UVM device found\n");
    
    // 检查测试是否启用
    fp = fopen("/sys/module/nvidia_uvm/parameters/uvm_enable_builtin_tests", "r");
    if (!fp) {
        printf("✗ UVM module not loaded or parameters not accessible\n");
        return 0;
    }
    
    if (fgets(buffer, sizeof(buffer), fp) && buffer[0] == '1') {
        printf("✓ UVM builtin tests are enabled\n");
        fclose(fp);
        return 1;
    } else {
        printf("✗ UVM builtin tests are NOT enabled\n");
        printf("  Run: sudo modprobe nvidia-uvm uvm_enable_builtin_tests=1\n");
        fclose(fp);
        return 0;
    }
}

static int run_single_test(int uvm_fd, test_case_t *test)
{
    simple_test_params_t params = {0};
    int ret;
    
    printf("  %-25s ... ", test->name);
    fflush(stdout);
    
    ret = ioctl(uvm_fd, test->ioctl_cmd, &params);
    
    if (ret == 0 && params.rmStatus == 0) {
        printf("PASSED\n");
        return 1;
    } else {
        printf("FAILED (ret=%d, status=0x%x)\n", ret, params.rmStatus);
        if (ret != 0) {
            printf("    System error: %s\n", strerror(errno));
        }
        return 0;
    }
}

int main(int argc, char *argv[])
{
    int uvm_fd;
    int total_tests = sizeof(test_cases) / sizeof(test_cases[0]);
    int passed_tests = 0;
    int i;
    
    printf("=== Simple NVIDIA UVM Test Runner ===\n");
    printf("Version: 1.0\n");
    printf("Total tests: %d\n\n", total_tests);
    
    // 检查环境
    if (!check_uvm_environment()) {
        printf("\nEnvironment check failed. Please fix the issues above.\n");
        return 1;
    }
    
    // 打开UVM设备
    uvm_fd = open("/dev/nvidia-uvm", O_RDWR);
    if (uvm_fd < 0) {
        perror("Failed to open /dev/nvidia-uvm");
        printf("Make sure you're running as root and UVM driver is loaded\n");
        return 1;
    }
    
    printf("\n=== Running Tests ===\n");
    
    // 运行所有测试
    for (i = 0; i < total_tests; i++) {
        if (run_single_test(uvm_fd, &test_cases[i])) {
            passed_tests++;
        }
    }
    
    close(uvm_fd);
    
    // 打印结果
    printf("\n=== Test Results Summary ===\n");
    printf("Total tests:  %d\n", total_tests);
    printf("Passed tests: %d\n", passed_tests);
    printf("Failed tests: %d\n", total_tests - passed_tests);
    printf("Success rate: %.1f%%\n", 
           total_tests > 0 ? (100.0 * passed_tests / total_tests) : 0.0);
    
    if (passed_tests == total_tests) {
        printf("\n🎉 All tests passed!\n");
        return 0;
    } else {
        printf("\n❌ Some tests failed.\n");
        printf("This might be normal if:\n");
        printf("- Some GPU features are not available\n");
        printf("- Confidential Computing is not supported\n");
        printf("- Running in a virtual environment\n");
        return 1;
    }
}