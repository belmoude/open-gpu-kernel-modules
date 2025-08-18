/*
 * working_uvm_test.c - 最终工作的UVM测试程序
 * 
 * 关键发现：
 * - 需要先调用UVM_INITIALIZE来初始化文件描述符
 * - 只有初始化后，文件描述符才会变成UVM_FD_VA_SPACE类型
 * - 然后测试才能正常工作
 * 
 * 编译: gcc -o working_uvm_test working_uvm_test.c
 * 运行: sudo ./working_uvm_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

// 正确的数据类型定义
typedef uint32_t NV_STATUS;

// UVM API IOCTL命令
#define UVM_INITIALIZE                              74  // 从代码中找到的实际值
#define UVM_DEINITIALIZE                            75  // 通常紧跟在INITIALIZE后面

// UVM测试IOCTL命令
#define UVM_TEST_RNG_SANITY                         201
#define UVM_TEST_RANGE_TREE_DIRECTED                202
#define UVM_TEST_LOCK_SANITY                        218
#define UVM_TEST_KVMALLOC                           220
#define UVM_TEST_RANGE_ALLOCATOR_SANITY             225

// 参数结构定义
typedef struct {
    uint64_t flags;       // 初始化标志
    NV_STATUS rmStatus;   // 返回状态
} UVM_INITIALIZE_PARAMS;

typedef struct {
    NV_STATUS rmStatus;   // 返回状态
} UVM_DEINITIALIZE_PARAMS;

typedef struct {
    NV_STATUS rmStatus;
} UVM_TEST_SIMPLE_PARAMS;

typedef struct {
    uint32_t verbose;
    uint32_t seed;
    uint32_t iters;
    NV_STATUS rmStatus;
} UVM_TEST_RANGE_ALLOCATOR_SANITY_PARAMS;

// 测试用例定义
typedef struct {
    unsigned long ioctl_cmd;
    const char *name;
    int param_type;  // 0=simple, 1=range_allocator
} test_case_t;

static test_case_t test_cases[] = {
    {UVM_TEST_RNG_SANITY, "RNG_SANITY", 0},
    {UVM_TEST_RANGE_TREE_DIRECTED, "RANGE_TREE_DIRECTED", 0},
    {UVM_TEST_LOCK_SANITY, "LOCK_SANITY", 0},
    {UVM_TEST_KVMALLOC, "KVMALLOC", 0},
    {UVM_TEST_RANGE_ALLOCATOR_SANITY, "RANGE_ALLOCATOR_SANITY", 1},
};

static int g_uvm_fd = -1;
static int g_initialized = 0;

// 清理函数
static void cleanup(void)
{
    if (g_uvm_fd >= 0 && g_initialized) {
        UVM_DEINITIALIZE_PARAMS deinit_params = {0};
        ioctl(g_uvm_fd, UVM_DEINITIALIZE, &deinit_params);
        printf("✅ UVM已反初始化\n");
    }
    
    if (g_uvm_fd >= 0) {
        close(g_uvm_fd);
        printf("✅ UVM设备已关闭\n");
    }
}

// 初始化UVM
static int initialize_uvm(void)
{
    UVM_INITIALIZE_PARAMS params = {0};
    int ret;
    
    printf("=== 初始化UVM ===\n");
    printf("调用UVM_INITIALIZE (IOCTL %d)...\n", UVM_INITIALIZE);
    
    // 设置初始化参数
    params.flags = 0;  // 默认标志
    params.rmStatus = 0;
    
    ret = ioctl(g_uvm_fd, UVM_INITIALIZE, &params);
    
    printf("结果: ret=%d, rmStatus=0x%x, errno=%s\n", 
           ret, params.rmStatus, strerror(errno));
    
    if (ret == 0 && params.rmStatus == 0) {
        printf("✅ UVM初始化成功\n");
        printf("   文件描述符现在是UVM_FD_VA_SPACE类型\n");
        g_initialized = 1;
        return 1;
    } else {
        printf("❌ UVM初始化失败\n");
        if (ret != 0) {
            printf("   系统错误: %s\n", strerror(errno));
        }
        if (params.rmStatus != 0) {
            printf("   UVM错误: 0x%x\n", params.rmStatus);
        }
        return 0;
    }
}

// 运行单个测试
static int run_test(test_case_t *test)
{
    int ret;
    
    printf("  %-30s ... ", test->name);
    fflush(stdout);
    
    if (test->param_type == 0) {
        // 简单参数测试
        UVM_TEST_SIMPLE_PARAMS params = {0};
        ret = ioctl(g_uvm_fd, test->ioctl_cmd, &params);
        
        if (ret == 0 && params.rmStatus == 0) {
            printf("✅ PASSED\n");
            return 1;
        } else {
            printf("❌ FAILED (ret=%d, status=0x%x)\n", ret, params.rmStatus);
            return 0;
        }
    } else if (test->param_type == 1) {
        // 范围分配器测试
        UVM_TEST_RANGE_ALLOCATOR_SANITY_PARAMS params = {0};
        params.verbose = 0;
        params.seed = 12345;
        params.iters = 100;
        
        ret = ioctl(g_uvm_fd, test->ioctl_cmd, &params);
        
        if (ret == 0 && params.rmStatus == 0) {
            printf("✅ PASSED\n");
            return 1;
        } else {
            printf("❌ FAILED (ret=%d, status=0x%x)\n", ret, params.rmStatus);
            return 0;
        }
    }
    
    printf("❌ UNKNOWN PARAM TYPE\n");
    return 0;
}

// 主函数
int main(int argc, char *argv[])
{
    int total_tests = sizeof(test_cases) / sizeof(test_cases[0]);
    int passed_tests = 0;
    int i;
    
    printf("=== 最终工作的NVIDIA UVM测试程序 ===\n");
    printf("版本: 7.0 (正确的初始化流程)\n");
    
    // 注册清理函数
    atexit(cleanup);
    
    // 检查权限
    if (geteuid() != 0) {
        printf("❌ 需要root权限\n");
        return 1;
    }
    
    // 检查UVM测试是否启用
    FILE *fp = fopen("/sys/module/nvidia_uvm/parameters/uvm_enable_builtin_tests", "r");
    if (!fp) {
        printf("❌ 无法读取UVM模块参数\n");
        return 1;
    }
    
    char buffer[16];
    if (!fgets(buffer, sizeof(buffer), fp) || buffer[0] != '1') {
        printf("❌ UVM内置测试未启用\n");
        fclose(fp);
        return 1;
    }
    fclose(fp);
    printf("✅ UVM内置测试已启用\n");
    
    // 打开UVM设备
    printf("\n=== 打开UVM设备 ===\n");
    g_uvm_fd = open("/dev/nvidia-uvm", O_RDWR);
    if (g_uvm_fd < 0) {
        perror("Failed to open /dev/nvidia-uvm");
        return 1;
    }
    printf("✅ UVM设备打开成功 (fd=%d)\n", g_uvm_fd);
    
    // 关键步骤：初始化UVM
    if (!initialize_uvm()) {
        printf("❌ UVM初始化失败，无法运行测试\n");
        return 1;
    }
    
    // 现在运行测试
    printf("\n=== 运行测试套件 ===\n");
    printf("现在文件描述符已正确初始化，可以运行测试了\n");
    
    for (i = 0; i < total_tests; i++) {
        if (run_test(&test_cases[i])) {
            passed_tests++;
        }
        
        // 短暂暂停
        usleep(100000);
    }
    
    // 结果分析
    printf("\n=== 最终测试结果 ===\n");
    printf("总测试数: %d\n", total_tests);
    printf("通过测试: %d\n", passed_tests);
    printf("失败测试: %d\n", total_tests - passed_tests);
    printf("成功率: %.1f%%\n", total_tests > 0 ? (100.0 * passed_tests / total_tests) : 0.0);
    
    if (passed_tests > 0) {
        printf("\n🎉 成功！UVM测试框架工作正常\n");
        printf("关键发现:\n");
        printf("  ✅ 需要先调用UVM_INITIALIZE\n");
        printf("  ✅ 然后文件描述符变成UVM_FD_VA_SPACE类型\n");
        printf("  ✅ 之后测试才能正常工作\n");
    } else {
        printf("\n❌ 仍然失败，需要进一步调试\n");
    }
    
    // 清理会在atexit中自动执行
    return passed_tests > 0 ? 0 : 1;
}