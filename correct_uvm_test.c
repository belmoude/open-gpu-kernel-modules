/*
 * correct_uvm_test.c - 正确的UVM测试程序
 * 
 * 重要发现：
 * - VA space在打开/dev/nvidia-uvm时自动创建
 * - 不需要显式的CREATE_VA_SPACE IOCTL
 * - 每个文件描述符都有自己的VA space
 * 
 * 编译: gcc -o correct_uvm_test correct_uvm_test.c
 * 运行: sudo ./correct_uvm_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

// UVM测试IOCTL定义（正确的命令号）
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

// 简化的参数结构
typedef struct {
    int rmStatus;  // 返回状态
} UVM_TEST_PARAMS;

// 测试用例定义
typedef struct {
    unsigned long ioctl_cmd;
    const char *name;
    const char *description;
    const char *category;
} test_case_t;

// 完整的测试用例列表
static test_case_t test_cases[] = {
    // 基础数据结构测试
    {UVM_TEST_RNG_SANITY, "RNG_SANITY", "随机数生成器完整性测试", "基础"},
    {UVM_TEST_RANGE_TREE_DIRECTED, "RANGE_TREE_DIRECTED", "范围树有向测试", "基础"},
    {UVM_TEST_RANGE_ALLOCATOR_SANITY, "RANGE_ALLOCATOR_SANITY", "范围分配器测试", "基础"},
    {UVM_TEST_LOCK_SANITY, "LOCK_SANITY", "锁机制完整性测试", "基础"},
    {UVM_TEST_KVMALLOC, "KVMALLOC", "内核内存分配测试", "基础"},
    
    // 内存管理测试
    {UVM_TEST_RM_MEM_SANITY, "RM_MEM_SANITY", "RM内存管理测试", "内存"},
    
    // GPU硬件测试
    {UVM_TEST_GPU_SEMAPHORE_SANITY, "GPU_SEMAPHORE_SANITY", "GPU信号量测试", "GPU"},
    {UVM_TEST_CHANNEL_SANITY, "CHANNEL_SANITY", "GPU通道管理测试", "GPU"},
    {UVM_TEST_CE_SANITY, "CE_SANITY", "拷贝引擎测试", "GPU"},
    {UVM_TEST_FAULT_BUFFER_FLUSH, "FAULT_BUFFER_FLUSH", "故障缓冲区测试", "GPU"},
    
    // 同步机制测试
    {UVM_TEST_TRACKER_SANITY, "TRACKER_SANITY", "操作跟踪器测试", "同步"},
    {UVM_TEST_PUSH_SANITY, "PUSH_SANITY", "Push机制测试", "同步"},
    
    // 性能测试
    {UVM_TEST_PERF_UTILS_SANITY, "PERF_UTILS_SANITY", "性能工具测试", "性能"},
    {UVM_TEST_PERF_EVENTS_SANITY, "PERF_EVENTS_SANITY", "性能事件测试", "性能"},
    {UVM_TEST_PERF_MODULE_SANITY, "PERF_MODULE_SANITY", "性能模块测试", "性能"},
    
    // 机密计算测试
    {UVM_TEST_SEC2_SANITY, "SEC2_SANITY", "SEC2引擎测试（机密计算）", "机密计算"},
    {UVM_TEST_SEC2_CPU_GPU_ROUNDTRIP, "SEC2_CPU_GPU_ROUNDTRIP", "SEC2往返测试（机密计算）", "机密计算"},
};

// 检查环境
static int check_environment(void)
{
    printf("=== 检查UVM测试环境 ===\n");
    
    // 检查UVM设备
    if (access("/dev/nvidia-uvm", F_OK) != 0) {
        printf("❌ /dev/nvidia-uvm 设备未找到\n");
        printf("   请确保NVIDIA UVM驱动已加载\n");
        return 0;
    }
    printf("✅ UVM设备文件存在\n");
    
    // 检查测试启用状态
    FILE *fp = fopen("/sys/module/nvidia_uvm/parameters/uvm_enable_builtin_tests", "r");
    if (!fp) {
        printf("❌ 无法读取UVM模块参数\n");
        printf("   请确保UVM模块已加载\n");
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
static int run_single_test(int uvm_fd, test_case_t *test)
{
    UVM_TEST_PARAMS params = {0};
    int ret;
    
    printf("  %-30s ... ", test->name);
    fflush(stdout);
    
    ret = ioctl(uvm_fd, test->ioctl_cmd, &params);
    
    if (ret == 0 && params.rmStatus == 0) {
        printf("✅ PASSED\n");
        return 1;
    } else {
        printf("❌ FAILED");
        if (ret != 0) {
            printf(" (ioctl=%d, errno=%s)", ret, strerror(errno));
        }
        if (params.rmStatus != 0) {
            printf(" (status=0x%x)", params.rmStatus);
        }
        printf("\n");
        return 0;
    }
}

// 按类别运行测试
static void run_tests_by_category(int uvm_fd)
{
    int total_tests = sizeof(test_cases) / sizeof(test_cases[0]);
    int passed_tests = 0;
    const char *current_category = "";
    int i;
    
    printf("\n=== 运行UVM测试（自动VA Space管理） ===\n");
    printf("注意: VA space在打开设备时自动创建，无需手动管理\n");
    
    for (i = 0; i < total_tests; i++) {
        // 如果类别改变，打印类别标题
        if (strcmp(current_category, test_cases[i].category) != 0) {
            current_category = test_cases[i].category;
            printf("\n--- %s测试 ---\n", current_category);
        }
        
        if (run_single_test(uvm_fd, &test_cases[i])) {
            passed_tests++;
        }
    }
    
    // 打印结果总结
    printf("\n=== 测试结果总结 ===\n");
    printf("总测试数:   %d\n", total_tests);
    printf("通过测试:   %d\n", passed_tests);
    printf("失败测试:   %d\n", total_tests - passed_tests);
    printf("成功率:     %.1f%%\n", 
           total_tests > 0 ? (100.0 * passed_tests / total_tests) : 0.0);
    
    // 分析结果
    if (passed_tests == total_tests) {
        printf("\n🎉 所有测试通过！UVM功能完全正常\n");
    } else if (passed_tests > total_tests / 2) {
        printf("\n✅ 大部分测试通过，UVM基本功能正常\n");
        printf("失败的测试可能因为:\n");
        printf("  - 硬件功能不支持（如机密计算需要H100等GPU）\n");
        printf("  - 虚拟环境限制（某些功能在VM中不可用）\n");
        printf("  - 特定GPU功能缺失\n");
    } else if (passed_tests > 0) {
        printf("\n⚠️  部分测试通过，可能存在配置问题\n");
        printf("建议检查:\n");
        printf("  - NVIDIA驱动版本是否匹配\n");
        printf("  - GPU硬件是否支持UVM\n");
        printf("  - 系统配置是否正确\n");
    } else {
        printf("\n❌ 所有测试失败，存在严重问题\n");
        printf("可能的原因:\n");
        printf("  - IOCTL命令号仍然不正确\n");
        printf("  - UVM驱动版本不兼容\n");
        printf("  - 系统环境配置错误\n");
        printf("  - 权限或安全策略限制\n");
    }
}

// 显示帮助信息
static void show_help(void)
{
    printf("correct_uvm_test - 正确的NVIDIA UVM测试程序\n");
    printf("\n");
    printf("关键发现:\n");
    printf("  - VA space在打开/dev/nvidia-uvm时自动创建\n");
    printf("  - 不需要显式的CREATE_VA_SPACE IOCTL\n");
    printf("  - 每个文件描述符都有独立的VA space\n");
    printf("\n");
    printf("用法: sudo %s [选项]\n", program_invocation_short_name);
    printf("\n");
    printf("选项:\n");
    printf("  -h, --help    显示此帮助信息\n");
    printf("  -v, --verbose 详细输出\n");
    printf("\n");
    printf("示例:\n");
    printf("  sudo %s              # 运行所有测试\n", program_invocation_short_name);
    printf("  sudo %s -v           # 详细模式运行\n", program_invocation_short_name);
}

// 主函数
int main(int argc, char *argv[])
{
    int uvm_fd;
    int verbose = 0;
    
    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            show_help();
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        }
    }
    
    printf("=== 正确的NVIDIA UVM测试程序 ===\n");
    printf("版本: 3.0 (修正VA space理解)\n");
    
    // 检查权限
    if (geteuid() != 0) {
        printf("❌ 此程序需要root权限\n");
        printf("   运行: sudo %s\n", argv[0]);
        return 1;
    }
    
    // 检查环境
    if (!check_environment()) {
        printf("\n环境检查失败，请修复上述问题后重试\n");
        return 1;
    }
    
    // 打开UVM设备（这会自动创建VA space）
    printf("\n=== 打开UVM设备（自动创建VA Space） ===\n");
    uvm_fd = open("/dev/nvidia-uvm", O_RDWR);
    if (uvm_fd < 0) {
        perror("Failed to open /dev/nvidia-uvm");
        printf("请确保:\n");
        printf("1. 以root权限运行\n");
        printf("2. UVM驱动已正确加载\n");
        return 1;
    }
    
    printf("✅ UVM设备打开成功 (VA space自动创建)\n");
    
    if (verbose) {
        printf("   文件描述符: %d\n", uvm_fd);
        printf("   VA space已自动关联到此文件描述符\n");
    }
    
    // 运行测试
    run_tests_by_category(uvm_fd);
    
    // 关闭设备（自动清理VA space）
    printf("\n=== 关闭UVM设备（自动清理VA Space） ===\n");
    close(uvm_fd);
    printf("✅ UVM设备关闭完成\n");
    
    return 0;
}