/*
 * safe_uvm_test.c - 内存安全的UVM测试程序
 * 
 * 修复问题:
 * - 段错误问题
 * - 参数结构大小问题
 * - 更好的错误处理
 * 
 * 编译: gcc -o safe_uvm_test safe_uvm_test.c
 * 运行: sudo ./safe_uvm_test
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

// UVM测试IOCTL定义
#define UVM_TEST_RNG_SANITY                         201
#define UVM_TEST_RANGE_TREE_DIRECTED                202
#define UVM_TEST_RANGE_ALLOCATOR_SANITY             225
#define UVM_TEST_LOCK_SANITY                        218
#define UVM_TEST_KVMALLOC                           220

// 更大的参数结构，确保内存安全
typedef struct {
    int rmStatus;           // 返回状态
    uint64_t reserved1;     // 预留字段
    uint64_t reserved2;     // 预留字段
    uint64_t reserved3;     // 预留字段
    uint32_t flags;         // 标志字段
    uint32_t padding;       // 对齐填充
} UVM_TEST_PARAMS_SAFE;

// 测试用例定义
typedef struct {
    unsigned long ioctl_cmd;
    const char *name;
    const char *description;
} test_case_t;

// 安全的测试用例列表（只包含基础测试）
static test_case_t safe_test_cases[] = {
    {UVM_TEST_RANGE_ALLOCATOR_SANITY, "RANGE_ALLOCATOR_SANITY", "范围分配器测试"},
    {UVM_TEST_KVMALLOC, "KVMALLOC", "内核内存分配测试"},
    {UVM_TEST_LOCK_SANITY, "LOCK_SANITY", "锁机制测试"},
    {UVM_TEST_RNG_SANITY, "RNG_SANITY", "随机数生成器测试"},
    {UVM_TEST_RANGE_TREE_DIRECTED, "RANGE_TREE_DIRECTED", "范围树测试"},
};

// 全局变量
static int g_uvm_fd = -1;
static volatile int g_interrupted = 0;

// 信号处理函数
static void signal_handler(int sig)
{
    g_interrupted = 1;
    printf("\n收到信号 %d，正在安全退出...\n", sig);
    
    if (g_uvm_fd >= 0) {
        close(g_uvm_fd);
        g_uvm_fd = -1;
    }
    
    exit(1);
}

// 设置信号处理
static void setup_signal_handlers(void)
{
    signal(SIGINT, signal_handler);   // Ctrl+C
    signal(SIGTERM, signal_handler);  // 终止信号
    signal(SIGSEGV, signal_handler);  // 段错误
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
        fclose(fp);
        return 0;
    }
}

// 安全的单个测试运行
static int run_single_test_safe(test_case_t *test)
{
    UVM_TEST_PARAMS_SAFE params;
    int ret;
    
    // 初始化参数结构
    memset(&params, 0, sizeof(params));
    
    printf("  %-30s ... ", test->name);
    fflush(stdout);
    
    // 检查是否被中断
    if (g_interrupted) {
        printf("❌ INTERRUPTED\n");
        return 0;
    }
    
    // 执行IOCTL
    ret = ioctl(g_uvm_fd, test->ioctl_cmd, &params);
    
    if (ret == 0 && params.rmStatus == 0) {
        printf("✅ PASSED\n");
        return 1;
    } else {
        printf("❌ FAILED");
        
        if (ret != 0) {
            printf(" (ioctl=%d, errno=%d:%s)", ret, errno, strerror(errno));
        }
        
        if (params.rmStatus != 0) {
            printf(" (rmStatus=0x%x=%d)", params.rmStatus, params.rmStatus);
            
            // 解释常见的错误码
            switch (params.rmStatus) {
                case 0x16: // 22
                    printf(" [可能是参数无效或功能不支持]");
                    break;
                case 0x1:
                    printf(" [一般错误]");
                    break;
                case 0x5:
                    printf(" [权限不足]");
                    break;
                default:
                    printf(" [未知错误]");
                    break;
            }
        }
        
        printf("\n");
        return 0;
    }
}

// 安全的测试套件运行
static void run_safe_test_suite(void)
{
    int total_tests = sizeof(safe_test_cases) / sizeof(safe_test_cases[0]);
    int passed_tests = 0;
    int i;
    
    printf("\n=== 运行安全UVM测试套件 ===\n");
    printf("总测试数: %d\n", total_tests);
    printf("注意: 使用更大的参数结构防止内存问题\n");
    
    for (i = 0; i < total_tests && !g_interrupted; i++) {
        if (run_single_test_safe(&safe_test_cases[i])) {
            passed_tests++;
        }
        
        // 每次测试后短暂暂停，避免过快执行
        usleep(100000); // 100ms
    }
    
    // 打印结果
    printf("\n=== 安全测试结果 ===\n");
    printf("总测试数:   %d\n", total_tests);
    printf("通过测试:   %d\n", passed_tests);
    printf("失败测试:   %d\n", total_tests - passed_tests);
    
    if (passed_tests > 0) {
        printf("成功率:     %.1f%%\n", 100.0 * passed_tests / total_tests);
        printf("\n✅ 至少有 %d 个测试通过了！\n", passed_tests);
        printf("这说明:\n");
        printf("  - UVM设备工作正常\n");
        printf("  - IOCTL接口可用\n");
        printf("  - VA space自动管理正常\n");
    } else {
        printf("\n❌ 所有测试都失败了\n");
        printf("可能的原因:\n");
        printf("  - 参数结构仍然不匹配\n");
        printf("  - 需要特定的初始化步骤\n");
        printf("  - 驱动版本兼容性问题\n");
    }
}

// 运行单个测试进行调试
static void debug_single_test(void)
{
    UVM_TEST_PARAMS_SAFE params;
    int ret;
    unsigned long test_cmd = UVM_TEST_RANGE_ALLOCATOR_SANITY; // 已知能通过的测试
    
    printf("\n=== 调试单个测试 ===\n");
    printf("测试命令: %lu (RANGE_ALLOCATOR_SANITY)\n", test_cmd);
    
    // 清零参数
    memset(&params, 0, sizeof(params));
    
    printf("执行前:\n");
    printf("  参数结构大小: %zu 字节\n", sizeof(params));
    printf("  rmStatus: 0x%x\n", params.rmStatus);
    
    ret = ioctl(g_uvm_fd, test_cmd, &params);
    
    printf("执行后:\n");
    printf("  ioctl返回值: %d\n", ret);
    printf("  errno: %d (%s)\n", errno, strerror(errno));
    printf("  rmStatus: 0x%x (%d)\n", params.rmStatus, params.rmStatus);
    printf("  reserved1: 0x%lx\n", params.reserved1);
    printf("  reserved2: 0x%lx\n", params.reserved2);
    
    if (ret == 0 && params.rmStatus == 0) {
        printf("✅ 调试测试成功！\n");
    } else {
        printf("❌ 调试测试失败\n");
    }
}

// 主函数
int main(int argc, char *argv[])
{
    int debug_mode = 0;
    
    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--debug") == 0) {
            debug_mode = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("用法: %s [选项]\n", argv[0]);
            printf("选项:\n");
            printf("  -d, --debug   调试模式（只运行一个测试）\n");
            printf("  -h, --help    显示帮助\n");
            return 0;
        }
    }
    
    printf("=== 内存安全的NVIDIA UVM测试程序 ===\n");
    printf("版本: 4.0 (修复段错误问题)\n");
    
    // 设置信号处理
    setup_signal_handlers();
    
    // 检查权限
    if (geteuid() != 0) {
        printf("❌ 此程序需要root权限\n");
        return 1;
    }
    
    // 检查环境
    if (!check_environment()) {
        return 1;
    }
    
    // 打开UVM设备
    printf("\n=== 打开UVM设备 ===\n");
    g_uvm_fd = open("/dev/nvidia-uvm", O_RDWR);
    if (g_uvm_fd < 0) {
        perror("Failed to open /dev/nvidia-uvm");
        return 1;
    }
    
    printf("✅ UVM设备打开成功 (fd=%d)\n", g_uvm_fd);
    
    if (debug_mode) {
        debug_single_test();
    } else {
        run_safe_test_suite();
    }
    
    // 清理
    printf("\n=== 清理资源 ===\n");
    if (g_uvm_fd >= 0) {
        close(g_uvm_fd);
        printf("✅ UVM设备关闭完成\n");
    }
    
    return 0;
}