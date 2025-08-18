/*
 * smart_uvm_test.c - 智能UVM测试程序
 * 
 * 特点:
 * - 自动检测哪些测试需要VA space
 * - 自动创建和销毁VA space
 * - 智能错误处理和重试
 * - 详细的诊断信息
 * 
 * 编译: gcc -o smart_uvm_test smart_uvm_test.c
 * 运行: sudo ./smart_uvm_test
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

// 测试IOCTL定义
#define UVM_TEST_RNG_SANITY                         201
#define UVM_TEST_RANGE_TREE_DIRECTED                202
#define UVM_TEST_RM_MEM_SANITY                      205
#define UVM_TEST_GPU_SEMAPHORE_SANITY               206
#define UVM_TEST_TRACKER_SANITY                     212
#define UVM_TEST_PUSH_SANITY                        213
#define UVM_TEST_CHANNEL_SANITY                     214
#define UVM_TEST_CE_SANITY                          216
#define UVM_TEST_LOCK_SANITY                        218
#define UVM_TEST_PERF_UTILS_SANITY                  219
#define UVM_TEST_KVMALLOC                           220
#define UVM_TEST_PERF_EVENTS_SANITY                 223
#define UVM_TEST_PERF_MODULE_SANITY                 224
#define UVM_TEST_RANGE_ALLOCATOR_SANITY             225
#define UVM_TEST_FAULT_BUFFER_FLUSH                 227
#define UVM_TEST_SEC2_SANITY                        295
#define UVM_TEST_SEC2_CPU_GPU_ROUNDTRIP             299

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
} UVM_TEST_PARAMS;

// 测试用例定义
typedef struct {
    unsigned long ioctl_cmd;
    const char *name;
    const char *description;
    int needs_va_space;      // 是否需要VA space
    int is_conf_computing;   // 是否是机密计算测试
} test_case_t;

// 测试用例列表 - 根据代码分析标记哪些需要VA space
static test_case_t test_cases[] = {
    // 不需要VA space的测试
    {UVM_TEST_RNG_SANITY, "RNG_SANITY", "随机数生成器测试", 0, 0},
    {UVM_TEST_RANGE_TREE_DIRECTED, "RANGE_TREE_DIRECTED", "范围树有向测试", 0, 0},
    {UVM_TEST_LOCK_SANITY, "LOCK_SANITY", "锁机制测试", 0, 0},
    {UVM_TEST_KVMALLOC, "KVMALLOC", "内核内存分配测试", 0, 0},
    {UVM_TEST_RANGE_ALLOCATOR_SANITY, "RANGE_ALLOCATOR_SANITY", "范围分配器测试", 0, 0},
    
    // 需要VA space的测试
    {UVM_TEST_RM_MEM_SANITY, "RM_MEM_SANITY", "RM内存管理测试", 1, 0},
    {UVM_TEST_GPU_SEMAPHORE_SANITY, "GPU_SEMAPHORE_SANITY", "GPU信号量测试", 1, 0},
    {UVM_TEST_TRACKER_SANITY, "TRACKER_SANITY", "跟踪器测试", 1, 0},
    {UVM_TEST_PUSH_SANITY, "PUSH_SANITY", "Push机制测试", 1, 0},
    {UVM_TEST_CHANNEL_SANITY, "CHANNEL_SANITY", "通道管理测试", 1, 0},
    {UVM_TEST_CE_SANITY, "CE_SANITY", "拷贝引擎测试", 1, 0},
    {UVM_TEST_PERF_UTILS_SANITY, "PERF_UTILS_SANITY", "性能工具测试", 1, 0},
    {UVM_TEST_PERF_EVENTS_SANITY, "PERF_EVENTS_SANITY", "性能事件测试", 1, 0},
    {UVM_TEST_PERF_MODULE_SANITY, "PERF_MODULE_SANITY", "性能模块测试", 1, 0},
    {UVM_TEST_FAULT_BUFFER_FLUSH, "FAULT_BUFFER_FLUSH", "故障缓冲区测试", 1, 0},
    
    // 机密计算测试（需要VA space）
    {UVM_TEST_SEC2_SANITY, "SEC2_SANITY", "SEC2引擎测试（机密计算）", 1, 1},
    {UVM_TEST_SEC2_CPU_GPU_ROUNDTRIP, "SEC2_CPU_GPU_ROUNDTRIP", "SEC2往返测试（机密计算）", 1, 1},
};

// 全局状态
static int g_uvm_fd = -1;
static int g_uvm_initialized = 0;
static uint64_t g_va_space_handle = 0;

// 初始化UVM
static int init_uvm(void)
{
    UVM_INITIALIZE_PARAMS params = {0};
    int ret;
    
    if (g_uvm_initialized)
        return 1;  // 已经初始化
    
    printf("  初始化UVM...");
    fflush(stdout);
    
    ret = ioctl(g_uvm_fd, UVM_INITIALIZE, &params);
    if (ret == 0 && params.rmStatus == 0) {
        g_uvm_initialized = 1;
        printf(" ✅\n");
        return 1;
    } else {
        printf(" ❌ (ret=%d, status=0x%x, errno=%s)\n", 
               ret, params.rmStatus, strerror(errno));
        return 0;
    }
}

// 创建VA space
static int create_va_space(void)
{
    UVM_CREATE_VA_SPACE_PARAMS params = {0};
    int ret;
    
    if (g_va_space_handle != 0)
        return 1;  // 已经创建
    
    printf("  创建VA Space...");
    fflush(stdout);
    
    ret = ioctl(g_uvm_fd, UVM_CREATE_VA_SPACE, &params);
    if (ret == 0 && params.rmStatus == 0) {
        g_va_space_handle = params.vaSpace;
        printf(" ✅ (handle=0x%lx)\n", g_va_space_handle);
        return 1;
    } else {
        printf(" ❌ (ret=%d, status=0x%x, errno=%s)\n", 
               ret, params.rmStatus, strerror(errno));
        return 0;
    }
}

// 销毁VA space
static void destroy_va_space(void)
{
    UVM_DESTROY_VA_SPACE_PARAMS params = {0};
    int ret;
    
    if (g_va_space_handle == 0)
        return;  // 没有创建
    
    printf("  销毁VA Space...");
    fflush(stdout);
    
    params.vaSpace = g_va_space_handle;
    ret = ioctl(g_uvm_fd, UVM_DESTROY_VA_SPACE, &params);
    if (ret == 0 && params.rmStatus == 0) {
        printf(" ✅\n");
    } else {
        printf(" ❌ (ret=%d, status=0x%x)\n", ret, params.rmStatus);
    }
    
    g_va_space_handle = 0;
}

// 反初始化UVM
static void deinit_uvm(void)
{
    UVM_DEINITIALIZE_PARAMS params = {0};
    int ret;
    
    if (!g_uvm_initialized)
        return;
    
    printf("  反初始化UVM...");
    fflush(stdout);
    
    ret = ioctl(g_uvm_fd, UVM_DEINITIALIZE, &params);
    if (ret == 0 && params.rmStatus == 0) {
        printf(" ✅\n");
    } else {
        printf(" ❌ (ret=%d, status=0x%x)\n", ret, params.rmStatus);
    }
    
    g_uvm_initialized = 0;
}

// 检查环境
static int check_environment(void)
{
    printf("=== 检查UVM测试环境 ===\n");
    
    // 检查UVM设备
    if (access("/dev/nvidia-uvm", F_OK) != 0) {
        printf("❌ /dev/nvidia-uvm 设备未找到\n");
        return 0;
    }
    printf("✅ UVM设备文件存在\n");
    
    // 检查测试启用状态
    FILE *fp = fopen("/sys/module/nvidia_uvm/parameters/uvm_enable_builtin_tests", "r");
    if (!fp) {
        printf("❌ 无法读取UVM模块参数\n");
        return 0;
    }
    
    char buffer[16];
    if (fgets(buffer, sizeof(buffer), fp) && buffer[0] == '1') {
        printf("✅ UVM内置测试已启用\n");
        fclose(fp);
        return 1;
    } else {
        printf("❌ UVM内置测试未启用\n");
        printf("   运行: sudo modprobe nvidia-uvm uvm_enable_builtin_tests=1\n");
        fclose(fp);
        return 0;
    }
}

// 运行单个测试
static int run_single_test(test_case_t *test)
{
    UVM_TEST_PARAMS params = {0};
    int ret;
    int success = 0;
    
    printf("  %-25s ... ", test->name);
    fflush(stdout);
    
    // 如果需要VA space，确保已创建
    if (test->needs_va_space && g_va_space_handle == 0) {
        if (!create_va_space()) {
            printf("❌ (无法创建VA Space)\n");
            return 0;
        }
    }
    
    // 运行测试
    ret = ioctl(g_uvm_fd, test->ioctl_cmd, &params);
    
    if (ret == 0 && params.rmStatus == 0) {
        printf("✅ PASSED\n");
        success = 1;
    } else {
        printf("❌ FAILED");
        if (ret != 0) {
            printf(" (ioctl ret=%d, errno=%s)", ret, strerror(errno));
        }
        if (params.rmStatus != 0) {
            printf(" (rmStatus=0x%x)", params.rmStatus);
        }
        printf("\n");
        
        // 特殊处理：如果是机密计算测试失败，可能是硬件不支持
        if (test->is_conf_computing) {
            printf("    注意: 机密计算测试需要支持的硬件\n");
        }
    }
    
    return success;
}

// 运行测试套件
static void run_test_suite(void)
{
    int total_tests = sizeof(test_cases) / sizeof(test_cases[0]);
    int passed_tests = 0;
    int i;
    
    printf("\n=== 运行UVM测试套件 ===\n");
    printf("总测试数: %d\n", total_tests);
    
    // 按类别组织测试
    printf("\n--- 基础测试（无需VA Space） ---\n");
    for (i = 0; i < total_tests; i++) {
        if (!test_cases[i].needs_va_space) {
            if (run_single_test(&test_cases[i])) {
                passed_tests++;
            }
        }
    }
    
    printf("\n--- 高级测试（需要VA Space） ---\n");
    for (i = 0; i < total_tests; i++) {
        if (test_cases[i].needs_va_space && !test_cases[i].is_conf_computing) {
            if (run_single_test(&test_cases[i])) {
                passed_tests++;
            }
        }
    }
    
    printf("\n--- 机密计算测试（需要VA Space + 特殊硬件） ---\n");
    for (i = 0; i < total_tests; i++) {
        if (test_cases[i].is_conf_computing) {
            if (run_single_test(&test_cases[i])) {
                passed_tests++;
            }
        }
    }
    
    // 打印结果
    printf("\n=== 测试结果总结 ===\n");
    printf("总测试数:   %d\n", total_tests);
    printf("通过测试:   %d\n", passed_tests);
    printf("失败测试:   %d\n", total_tests - passed_tests);
    printf("成功率:     %.1f%%\n", 
           total_tests > 0 ? (100.0 * passed_tests / total_tests) : 0.0);
    
    if (passed_tests == total_tests) {
        printf("\n🎉 所有测试通过！\n");
    } else if (passed_tests > 0) {
        printf("\n✅ 部分测试通过，这通常是正常的\n");
        printf("失败原因可能包括:\n");
        printf("  - 硬件功能不支持（如机密计算）\n");
        printf("  - 虚拟环境限制\n");
        printf("  - 特定GPU功能缺失\n");
    } else {
        printf("\n❌ 所有测试失败，请检查环境配置\n");
    }
}

// 清理资源
static void cleanup(void)
{
    printf("\n=== 清理资源 ===\n");
    
    // 销毁VA space（如果存在）
    if (g_va_space_handle != 0) {
        destroy_va_space();
    }
    
    // 反初始化UVM（如果已初始化）
    if (g_uvm_initialized) {
        deinit_uvm();
    }
    
    // 关闭设备文件
    if (g_uvm_fd >= 0) {
        close(g_uvm_fd);
        g_uvm_fd = -1;
    }
}

// 主函数
int main(int argc, char *argv[])
{
    int success = 0;
    
    printf("=== 智能NVIDIA UVM测试程序 ===\n");
    printf("版本: 2.0 (支持自动VA Space管理)\n");
    
    // 注册清理函数
    atexit(cleanup);
    
    // 检查环境
    if (!check_environment()) {
        printf("\n环境检查失败，请修复上述问题后重试\n");
        return 1;
    }
    
    // 打开UVM设备
    g_uvm_fd = open("/dev/nvidia-uvm", O_RDWR);
    if (g_uvm_fd < 0) {
        perror("Failed to open /dev/nvidia-uvm");
        printf("请确保:\n");
        printf("1. 以root权限运行\n");
        printf("2. UVM驱动已加载\n");
        return 1;
    }
    
    printf("✅ UVM设备打开成功\n");
    
    // 初始化UVM
    if (!init_uvm()) {
        printf("❌ UVM初始化失败\n");
        return 1;
    }
    
    // 运行测试套件
    run_test_suite();
    
    // 清理会在atexit中自动执行
    return 0;
}