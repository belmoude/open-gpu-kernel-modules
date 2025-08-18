/*
 * comprehensive_uvm_test.c - 完整的UVM测试程序
 * 
 * 特点:
 * - 包含所有测试用例的正确参数结构
 * - 自动处理不同类型的参数
 * - 内存安全和错误处理
 * - 详细的结果分析
 * 
 * 编译: gcc -o comprehensive_uvm_test comprehensive_uvm_test.c
 * 运行: sudo ./comprehensive_uvm_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>

// UVM测试IOCTL命令定义
#define UVM_TEST_RNG_SANITY                         201
#define UVM_TEST_RANGE_TREE_DIRECTED                202
#define UVM_TEST_RANGE_TREE_RANDOM                  203
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

// 参数结构定义（按实际头文件）

// 类型1: 只有rmStatus的简单结构
typedef struct {
    int rmStatus;  // NV_STATUS
} UVM_TEST_SIMPLE_PARAMS;

// 类型2: 带输入参数的PUSH测试
typedef struct {
    uint8_t skipTimestampTest;  // NvBool (In)
    int rmStatus;               // NV_STATUS (Out)
} UVM_TEST_PUSH_SANITY_PARAMS;

// 类型3: 带输入参数的CE测试
typedef struct {
    uint8_t skipTimestampTest;  // NvBool (In)
    int rmStatus;               // NV_STATUS (Out)
} UVM_TEST_CE_SANITY_PARAMS;

// 类型4: 范围分配器测试
typedef struct {
    uint32_t verbose;    // NvU32 (In)
    uint32_t seed;       // NvU32 (In)
    uint32_t iters;      // NvU32 (In)
    int rmStatus;        // NV_STATUS (Out)
} UVM_TEST_RANGE_ALLOCATOR_SANITY_PARAMS;

// 类型5: 范围树随机测试
typedef struct {
    uint32_t seed;                     // NvU32 (In)
    uint64_t main_iterations;          // NvU64 (In)
    uint32_t verbose;                  // NvU32 (In)
    uint32_t high_probability;         // NvU32 (In)
    uint32_t add_remove_shrink_group_probability; // NvU32 (In)
    uint32_t shrink_probability;       // NvU32 (In)
    uint32_t collision_checks;         // NvU32 (In)
    uint32_t iterator_checks;          // NvU32 (In)
    uint64_t max_end;                  // NvU64 (In)
    int rmStatus;                      // NV_STATUS (Out)
} UVM_TEST_RANGE_TREE_RANDOM_PARAMS;

// 类型6: 性能模块测试
typedef struct {
    uint64_t range_address;  // NvU64 (In)
    uint32_t range_size;     // NvU32 (In)
    int rmStatus;            // NV_STATUS (Out)
} UVM_TEST_PERF_MODULE_SANITY_PARAMS;

// 类型7: 故障缓冲区测试
typedef struct {
    uint64_t iterations;     // NvU64 (In)
    int rmStatus;            // NV_STATUS (Out)
} UVM_TEST_FAULT_BUFFER_FLUSH_PARAMS;

// 测试用例定义
typedef struct {
    unsigned long ioctl_cmd;
    const char *name;
    const char *description;
    const char *category;
    int param_type;  // 参数类型
    int expected_to_fail;  // 是否预期失败
} test_case_t;

// 完整的测试用例列表
static test_case_t all_test_cases[] = {
    // 基础测试 - 简单参数
    {UVM_TEST_RNG_SANITY, "RNG_SANITY", "随机数生成器测试", "基础", 1, 0},
    {UVM_TEST_RANGE_TREE_DIRECTED, "RANGE_TREE_DIRECTED", "范围树有向测试", "基础", 1, 0},
    {UVM_TEST_LOCK_SANITY, "LOCK_SANITY", "锁机制测试", "基础", 1, 0},
    {UVM_TEST_KVMALLOC, "KVMALLOC", "内核内存分配测试", "基础", 1, 0},
    
    // 内存管理测试
    {UVM_TEST_RM_MEM_SANITY, "RM_MEM_SANITY", "RM内存管理测试", "内存", 1, 0},
    
    // GPU硬件测试
    {UVM_TEST_GPU_SEMAPHORE_SANITY, "GPU_SEMAPHORE_SANITY", "GPU信号量测试", "GPU", 1, 0},
    {UVM_TEST_CHANNEL_SANITY, "CHANNEL_SANITY", "GPU通道测试", "GPU", 1, 0},
    
    // 同步机制测试
    {UVM_TEST_TRACKER_SANITY, "TRACKER_SANITY", "跟踪器测试", "同步", 1, 0},
    
    // 性能测试
    {UVM_TEST_PERF_UTILS_SANITY, "PERF_UTILS_SANITY", "性能工具测试", "性能", 1, 0},
    {UVM_TEST_PERF_EVENTS_SANITY, "PERF_EVENTS_SANITY", "性能事件测试", "性能", 1, 0},
    
    // 带输入参数的测试
    {UVM_TEST_PUSH_SANITY, "PUSH_SANITY", "Push机制测试", "同步", 2, 0},
    {UVM_TEST_CE_SANITY, "CE_SANITY", "拷贝引擎测试", "GPU", 3, 0},
    {UVM_TEST_RANGE_ALLOCATOR_SANITY, "RANGE_ALLOCATOR_SANITY", "范围分配器测试", "基础", 4, 0},
    {UVM_TEST_PERF_MODULE_SANITY, "PERF_MODULE_SANITY", "性能模块测试", "性能", 6, 1}, // 需要地址参数，可能失败
    {UVM_TEST_FAULT_BUFFER_FLUSH, "FAULT_BUFFER_FLUSH", "故障缓冲区测试", "GPU", 7, 1}, // 可能失败
    
    // 机密计算测试
    {UVM_TEST_SEC2_SANITY, "SEC2_SANITY", "SEC2引擎测试", "机密计算", 1, 1}, // 需要特殊硬件
    {UVM_TEST_SEC2_CPU_GPU_ROUNDTRIP, "SEC2_CPU_GPU_ROUNDTRIP", "SEC2往返测试", "机密计算", 1, 1}, // 需要特殊硬件
};

static int g_uvm_fd = -1;

// 信号处理
static void cleanup_on_signal(int sig)
{
    printf("\n收到信号 %d，清理资源...\n", sig);
    if (g_uvm_fd >= 0) {
        close(g_uvm_fd);
    }
    exit(1);
}

// 运行不同类型的测试
static int run_test_by_type(test_case_t *test)
{
    int ret;
    
    printf("  %-35s ... ", test->name);
    fflush(stdout);
    
    switch (test->param_type) {
        case 1: // 简单参数
        {
            UVM_TEST_SIMPLE_PARAMS params = {0};
            ret = ioctl(g_uvm_fd, test->ioctl_cmd, &params);
            
            if (ret == 0 && params.rmStatus == 0) {
                printf("✅ PASSED\n");
                return 1;
            } else {
                printf("❌ FAILED (ret=%d, status=0x%x)\n", ret, params.rmStatus);
                return 0;
            }
        }
        
        case 2: // PUSH测试参数
        {
            UVM_TEST_PUSH_SANITY_PARAMS params = {0};
            params.skipTimestampTest = 0;  // 不跳过时间戳测试
            ret = ioctl(g_uvm_fd, test->ioctl_cmd, &params);
            
            if (ret == 0 && params.rmStatus == 0) {
                printf("✅ PASSED\n");
                return 1;
            } else {
                printf("❌ FAILED (ret=%d, status=0x%x)\n", ret, params.rmStatus);
                return 0;
            }
        }
        
        case 3: // CE测试参数
        {
            UVM_TEST_CE_SANITY_PARAMS params = {0};
            params.skipTimestampTest = 0;  // 不跳过时间戳测试
            ret = ioctl(g_uvm_fd, test->ioctl_cmd, &params);
            
            if (ret == 0 && params.rmStatus == 0) {
                printf("✅ PASSED\n");
                return 1;
            } else {
                printf("❌ FAILED (ret=%d, status=0x%x)\n", ret, params.rmStatus);
                return 0;
            }
        }
        
        case 4: // 范围分配器测试参数
        {
            UVM_TEST_RANGE_ALLOCATOR_SANITY_PARAMS params = {0};
            params.verbose = 0;    // 不详细输出
            params.seed = 12345;   // 随机种子
            params.iters = 100;    // 迭代次数
            ret = ioctl(g_uvm_fd, test->ioctl_cmd, &params);
            
            if (ret == 0 && params.rmStatus == 0) {
                printf("✅ PASSED\n");
                return 1;
            } else {
                printf("❌ FAILED (ret=%d, status=0x%x)\n", ret, params.rmStatus);
                return 0;
            }
        }
        
        case 6: // 性能模块测试参数
        {
            UVM_TEST_PERF_MODULE_SANITY_PARAMS params = {0};
            params.range_address = 0x10000000;  // 示例地址
            params.range_size = 0x1000;         // 4KB
            ret = ioctl(g_uvm_fd, test->ioctl_cmd, &params);
            
            if (ret == 0 && params.rmStatus == 0) {
                printf("✅ PASSED\n");
                return 1;
            } else {
                printf("❌ FAILED (ret=%d, status=0x%x)\n", ret, params.rmStatus);
                return 0;
            }
        }
        
        case 7: // 故障缓冲区测试参数
        {
            UVM_TEST_FAULT_BUFFER_FLUSH_PARAMS params = {0};
            params.iterations = 10;  // 迭代次数
            ret = ioctl(g_uvm_fd, test->ioctl_cmd, &params);
            
            if (ret == 0 && params.rmStatus == 0) {
                printf("✅ PASSED\n");
                return 1;
            } else {
                printf("❌ FAILED (ret=%d, status=0x%x)\n", ret, params.rmStatus);
                return 0;
            }
        }
        
        default:
            printf("❌ UNKNOWN PARAM TYPE\n");
            return 0;
    }
}

// 检查环境
static int check_environment(void)
{
    printf("=== 检查UVM测试环境 ===\n");
    
    if (access("/dev/nvidia-uvm", F_OK) != 0) {
        printf("❌ /dev/nvidia-uvm 设备未找到\n");
        return 0;
    }
    printf("✅ UVM设备文件存在\n");
    
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
        fclose(fp);
        return 0;
    }
}

// 按类别运行测试
static void run_comprehensive_tests(void)
{
    int total_tests = sizeof(all_test_cases) / sizeof(all_test_cases[0]);
    int passed_tests = 0;
    int expected_failures = 0;
    const char *current_category = "";
    int i;
    
    printf("\n=== 运行完整UVM测试套件 ===\n");
    printf("总测试数: %d\n", total_tests);
    printf("注意: 使用每个测试的正确参数结构\n");
    
    for (i = 0; i < total_tests; i++) {
        test_case_t *test = &all_test_cases[i];
        
        // 打印类别标题
        if (strcmp(current_category, test->category) != 0) {
            current_category = test->category;
            printf("\n--- %s测试 ---\n", current_category);
        }
        
        // 运行测试
        int result = run_test_by_type(test);
        
        if (result) {
            passed_tests++;
        } else if (test->expected_to_fail) {
            expected_failures++;
            printf("    (预期失败: %s)\n", 
                   strcmp(test->category, "机密计算") == 0 ? "需要特殊硬件" : "需要特定条件");
        }
        
        // 短暂暂停
        usleep(100000); // 100ms
    }
    
    // 详细结果分析
    printf("\n=== 详细测试结果分析 ===\n");
    printf("总测试数:     %d\n", total_tests);
    printf("通过测试:     %d\n", passed_tests);
    printf("失败测试:     %d\n", total_tests - passed_tests);
    printf("预期失败:     %d\n", expected_failures);
    printf("意外失败:     %d\n", total_tests - passed_tests - expected_failures);
    
    float success_rate = total_tests > 0 ? (100.0 * passed_tests / total_tests) : 0.0;
    printf("总成功率:     %.1f%%\n", success_rate);
    
    int core_tests = total_tests - expected_failures;
    if (core_tests > 0) {
        float core_success_rate = 100.0 * passed_tests / core_tests;
        printf("核心功能成功率: %.1f%%\n", core_success_rate);
    }
    
    // 结果评估
    printf("\n=== 结果评估 ===\n");
    
    if (passed_tests == 0) {
        printf("❌ 所有测试失败 - 存在严重问题\n");
        printf("可能原因:\n");
        printf("  - IOCTL接口不兼容\n");
        printf("  - 驱动版本问题\n");
        printf("  - 系统配置错误\n");
    } else if (passed_tests >= core_tests * 0.8) {
        printf("🎉 优秀！大部分核心功能正常\n");
        printf("UVM驱动工作状态: 良好\n");
    } else if (passed_tests >= core_tests * 0.5) {
        printf("✅ 良好！基本功能正常\n");
        printf("UVM驱动工作状态: 基本正常\n");
    } else {
        printf("⚠️  部分功能异常\n");
        printf("UVM驱动工作状态: 需要检查\n");
    }
    
    // 失败分析
    if (total_tests - passed_tests - expected_failures > 0) {
        printf("\n意外失败的测试需要进一步调查\n");
        printf("建议:\n");
        printf("  - 检查内核日志: dmesg | grep -i uvm\n");
        printf("  - 检查GPU状态: nvidia-smi\n");
        printf("  - 验证驱动版本兼容性\n");
    }
}

// 主函数
int main(int argc, char *argv[])
{
    printf("=== 完整的NVIDIA UVM测试程序 ===\n");
    printf("版本: 6.0 (完整参数结构支持)\n");
    
    // 设置信号处理
    signal(SIGINT, cleanup_on_signal);
    signal(SIGTERM, cleanup_on_signal);
    
    // 检查权限
    if (geteuid() != 0) {
        printf("❌ 需要root权限运行\n");
        printf("   运行: sudo %s\n", argv[0]);
        return 1;
    }
    
    // 检查环境
    if (!check_environment()) {
        return 1;
    }
    
    // 打开UVM设备
    printf("\n=== 初始化UVM连接 ===\n");
    g_uvm_fd = open("/dev/nvidia-uvm", O_RDWR);
    if (g_uvm_fd < 0) {
        perror("Failed to open /dev/nvidia-uvm");
        return 1;
    }
    
    printf("✅ UVM设备连接成功\n");
    printf("   文件描述符: %d\n", g_uvm_fd);
    printf("   VA space已自动创建\n");
    
    // 运行完整测试套件
    run_comprehensive_tests();
    
    // 清理
    printf("\n=== 清理资源 ===\n");
    close(g_uvm_fd);
    printf("✅ UVM设备连接关闭\n");
    printf("✅ VA space自动清理完成\n");
    
    return 0;
}