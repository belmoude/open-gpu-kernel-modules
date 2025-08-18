/*******************************************************************************
    UVM Test Runner - User-space test program for NVIDIA UVM driver
    
    This program executes all available UVM test cases through ioctl calls.
    
    Usage: ./uvm_test_runner [options]
    Options:
        -h, --help          Show this help message
        -l, --list          List all available test cases
        -t, --test <name>   Run specific test case by name
        -v, --verbose       Enable verbose output
        -c, --continue      Continue running tests after failures
        --filter <pattern>  Run tests matching pattern (regex)
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <getopt.h>
#include <regex.h>
#include <time.h>
#include <sys/stat.h>

// UVM ioctl definitions
#define UVM_IOCTL_BASE(i) i

// Test command definitions (from uvm_test_ioctl.h)
#define UVM_TEST_IOCTL_BASE(i) (200 + i)

// All UVM test commands
typedef struct {
    unsigned int cmd;
    const char *name;
    const char *description;
    int requires_gpu;
} uvm_test_case_t;

// Test case registry - extracted from uvm_test_ioctl.h
static const uvm_test_case_t uvm_tests[] = {
    {UVM_TEST_IOCTL_BASE(0),  "GET_GPU_REF_COUNT",            "Get GPU reference count",                    1},
    {UVM_TEST_IOCTL_BASE(1),  "RNG_SANITY",                   "Random number generator sanity test",       0},
    {UVM_TEST_IOCTL_BASE(2),  "RANGE_TREE_DIRECTED",          "Directed range tree test",                  0},
    {UVM_TEST_IOCTL_BASE(3),  "RANGE_TREE_RANDOM",            "Random range tree test",                    0},
    {UVM_TEST_IOCTL_BASE(4),  "VA_RANGE_INFO",                "VA range information test",                 0},
    {UVM_TEST_IOCTL_BASE(5),  "RM_MEM_SANITY",                "RM memory sanity test",                     1},
    {UVM_TEST_IOCTL_BASE(6),  "GPU_SEMAPHORE_SANITY",         "GPU semaphore sanity test",                 1},
    {UVM_TEST_IOCTL_BASE(7),  "PEER_REF_COUNT",               "Peer reference count test",                 1},
    {UVM_TEST_IOCTL_BASE(8),  "VA_RANGE_SPLIT",               "VA range split test",                       0},
    {UVM_TEST_IOCTL_BASE(9),  "VA_RANGE_INJECT_SPLIT_ERROR",  "VA range split error injection test",      0},
    {UVM_TEST_IOCTL_BASE(10), "PAGE_TREE",                    "Page tree test",                            1},
    {UVM_TEST_IOCTL_BASE(11), "CHANGE_PTE_MAPPING",           "Change PTE mapping test",                   1},
    {UVM_TEST_IOCTL_BASE(12), "TRACKER_SANITY",               "Tracker sanity test",                       1},
    {UVM_TEST_IOCTL_BASE(13), "PUSH_SANITY",                  "Push sanity test",                          1},
    {UVM_TEST_IOCTL_BASE(14), "CHANNEL_SANITY",               "Channel sanity test",                       1},
    {UVM_TEST_IOCTL_BASE(15), "CHANNEL_STRESS",               "Channel stress test",                       1},
    {UVM_TEST_IOCTL_BASE(16), "CE_SANITY",                    "Copy engine sanity test",                   1},
    {UVM_TEST_IOCTL_BASE(17), "VA_BLOCK_INFO",                "VA block information test",                 0},
    {UVM_TEST_IOCTL_BASE(18), "LOCK_SANITY",                  "Lock sanity test",                          0},
    {UVM_TEST_IOCTL_BASE(19), "PERF_UTILS_SANITY",            "Performance utils sanity test",             0},
    {UVM_TEST_IOCTL_BASE(20), "KVMALLOC",                     "Kernel memory allocation test",             0},
    {UVM_TEST_IOCTL_BASE(21), "PMM_QUERY",                    "Physical memory manager query test",        1},
    {UVM_TEST_IOCTL_BASE(22), "PMM_CHECK_LEAK",               "PMM leak check test",                       1},
    {UVM_TEST_IOCTL_BASE(23), "PERF_EVENTS_SANITY",           "Performance events sanity test",            0},
    {UVM_TEST_IOCTL_BASE(24), "PERF_MODULE_SANITY",           "Performance module sanity test",            0},
    {UVM_TEST_IOCTL_BASE(25), "RANGE_ALLOCATOR_SANITY",       "Range allocator sanity test",               0},
    {UVM_TEST_IOCTL_BASE(26), "GET_RM_PTES",                  "Get RM PTEs test",                          1},
    {UVM_TEST_IOCTL_BASE(27), "FAULT_BUFFER_FLUSH",           "Fault buffer flush test",                   1},
    {UVM_TEST_IOCTL_BASE(28), "INJECT_TOOLS_EVENT",           "Inject tools event test",                   0},
    {UVM_TEST_IOCTL_BASE(29), "INCREMENT_TOOLS_COUNTER",      "Increment tools counter test",              0},
    {UVM_TEST_IOCTL_BASE(30), "MEM_SANITY",                   "Memory sanity test",                        0},
    {UVM_TEST_IOCTL_BASE(32), "MAKE_CHANNEL_STOPS_IMMEDIATE", "Make channel stops immediate test",         1},
    {UVM_TEST_IOCTL_BASE(33), "VA_BLOCK_INJECT_ERROR",        "VA block error injection test",             0},
    {UVM_TEST_IOCTL_BASE(34), "PEER_IDENTITY_MAPPINGS",       "Peer identity mappings test",               1},
    {UVM_TEST_IOCTL_BASE(35), "VA_RESIDENCY_INFO",            "VA residency information test",             0},
    {UVM_TEST_IOCTL_BASE(36), "PMM_ASYNC_ALLOC",              "PMM async allocation test",                 1},
    {UVM_TEST_IOCTL_BASE(37), "SET_PREFETCH_FILTERING",       "Set prefetch filtering test",               0},
    {UVM_TEST_IOCTL_BASE(40), "PMM_SANITY",                   "PMM sanity test",                           1},
    {UVM_TEST_IOCTL_BASE(41), "INVALIDATE_TLB",               "TLB invalidation test",                     1},
    {UVM_TEST_IOCTL_BASE(42), "VA_BLOCK",                     "VA block test",                             0},
    {UVM_TEST_IOCTL_BASE(43), "EVICT_CHUNK",                  "Evict chunk test",                          1},
    {UVM_TEST_IOCTL_BASE(44), "FLUSH_DEFERRED_WORK",          "Flush deferred work test",                  0},
    {UVM_TEST_IOCTL_BASE(45), "NV_KTHREAD_Q",                 "NV kernel thread queue test",               0},
    {UVM_TEST_IOCTL_BASE(46), "SET_PAGE_PREFETCH_POLICY",     "Set page prefetch policy test",             0},
    {UVM_TEST_IOCTL_BASE(47), "RANGE_GROUP_TREE",             "Range group tree test",                     0},
    {UVM_TEST_IOCTL_BASE(48), "RANGE_GROUP_RANGE_INFO",       "Range group range info test",               0},
    {UVM_TEST_IOCTL_BASE(49), "RANGE_GROUP_RANGE_COUNT",      "Range group range count test",              0},
    {UVM_TEST_IOCTL_BASE(50), "GET_PREFETCH_FAULTS_REENABLE_LAPSE", "Get prefetch faults reenable lapse", 0},
    {UVM_TEST_IOCTL_BASE(51), "SET_PREFETCH_FAULTS_REENABLE_LAPSE", "Set prefetch faults reenable lapse", 0},
    {UVM_TEST_IOCTL_BASE(52), "GET_KERNEL_VIRTUAL_ADDRESS",   "Get kernel virtual address test",           0},
    {UVM_TEST_IOCTL_BASE(53), "PMA_ALLOC_FREE",               "PMA allocation/free test",                  1},
    {UVM_TEST_IOCTL_BASE(54), "PMM_ALLOC_FREE_ROOT",          "PMM alloc/free root test",                  1},
    {UVM_TEST_IOCTL_BASE(55), "PMM_INJECT_PMA_EVICT_ERROR",   "PMM inject PMA evict error test",           1},
    {UVM_TEST_IOCTL_BASE(56), "RECONFIGURE_ACCESS_COUNTERS",  "Reconfigure access counters test",          1},
    {UVM_TEST_IOCTL_BASE(57), "RESET_ACCESS_COUNTERS",        "Reset access counters test",                1},
    {UVM_TEST_IOCTL_BASE(58), "SET_IGNORE_ACCESS_COUNTERS",   "Set ignore access counters test",           1},
    {UVM_TEST_IOCTL_BASE(59), "CHECK_CHANNEL_VA_SPACE",       "Check channel VA space test",               1},
    {UVM_TEST_IOCTL_BASE(60), "ENABLE_NVLINK_PEER_ACCESS",    "Enable NVLink peer access test",            1},
    {UVM_TEST_IOCTL_BASE(61), "DISABLE_NVLINK_PEER_ACCESS",   "Disable NVLink peer access test",           1},
    {UVM_TEST_IOCTL_BASE(62), "GET_PAGE_THRASHING_POLICY",    "Get page thrashing policy test",            0},
    {UVM_TEST_IOCTL_BASE(63), "SET_PAGE_THRASHING_POLICY",    "Set page thrashing policy test",            0},
    {UVM_TEST_IOCTL_BASE(64), "PMM_SYSMEM",                   "PMM system memory test",                    0},
    {UVM_TEST_IOCTL_BASE(65), "PMM_REVERSE_MAP",              "PMM reverse mapping test",                  1},
    {UVM_TEST_IOCTL_BASE(66), "PMM_INDIRECT_PEERS",           "PMM indirect peers test",                   1},
    {UVM_TEST_IOCTL_BASE(67), "VA_SPACE_MM_RETAIN",           "VA space MM retain test",                   0},
    {UVM_TEST_IOCTL_BASE(69), "PMM_CHUNK_WITH_ELEVATED_PAGE", "PMM chunk with elevated page test",         1},
    {UVM_TEST_IOCTL_BASE(70), "GET_GPU_TIME",                 "Get GPU time test",                         1},
    {UVM_TEST_IOCTL_BASE(71), "ACCESS_COUNTERS_ENABLED_BY_DEFAULT", "Access counters enabled by default", 1},
    {UVM_TEST_IOCTL_BASE(72), "VA_SPACE_INJECT_ERROR",        "VA space error injection test",             0},
    {UVM_TEST_IOCTL_BASE(73), "PMM_RELEASE_FREE_ROOT_CHUNKS", "PMM release free root chunks test",         1},
    {UVM_TEST_IOCTL_BASE(74), "DRAIN_REPLAYABLE_FAULTS",      "Drain replayable faults test",              1},
    {UVM_TEST_IOCTL_BASE(75), "PMA_GET_BATCH_SIZE",           "PMA get batch size test",                   1},
    {UVM_TEST_IOCTL_BASE(76), "PMM_QUERY_PMA_STATS",          "PMM query PMA stats test",                  1},
    {UVM_TEST_IOCTL_BASE(78), "NUMA_CHECK_AFFINITY",          "NUMA check affinity test",                  0},
    {UVM_TEST_IOCTL_BASE(79), "VA_SPACE_ADD_DUMMY_THREAD_CONTEXTS", "VA space add dummy thread contexts", 0},
    {UVM_TEST_IOCTL_BASE(80), "VA_SPACE_REMOVE_DUMMY_THREAD_CONTEXTS", "VA space remove dummy thread contexts", 0},
    {UVM_TEST_IOCTL_BASE(81), "THREAD_CONTEXT_SANITY",        "Thread context sanity test",                0},
    {UVM_TEST_IOCTL_BASE(82), "THREAD_CONTEXT_PERF",          "Thread context performance test",           0},
    {UVM_TEST_IOCTL_BASE(83), "GET_PAGEABLE_MEM_ACCESS_TYPE", "Get pageable memory access type test",      0},
    {UVM_TEST_IOCTL_BASE(84), "TOOLS_FLUSH_REPLAY_EVENTS",    "Tools flush replay events test",            0},
    {UVM_TEST_IOCTL_BASE(85), "REGISTER_UNLOAD_STATE_BUFFER", "Register unload state buffer test",         0},
    {UVM_TEST_IOCTL_BASE(86), "RB_TREE_DIRECTED",             "Red-black tree directed test",              0},
    {UVM_TEST_IOCTL_BASE(87), "RB_TREE_RANDOM",               "Red-black tree random test",                0},
    {UVM_TEST_IOCTL_BASE(88), "HOST_SANITY",                  "Host sanity test",                          1},
    {UVM_TEST_IOCTL_BASE(89), "VA_SPACE_MM_OR_CURRENT_RETAIN", "VA space MM or current retain test",       0},
    {UVM_TEST_IOCTL_BASE(90), "GET_USER_SPACE_END_ADDRESS",   "Get user space end address test",           0},
    {UVM_TEST_IOCTL_BASE(91), "GET_CPU_CHUNK_ALLOC_SIZES",    "Get CPU chunk allocation sizes test",       0},
    {UVM_TEST_IOCTL_BASE(93), "VA_RANGE_INJECT_ADD_GPU_VA_SPACE_ERROR", "VA range inject add GPU VA space error", 1},
    {UVM_TEST_IOCTL_BASE(94), "DESTROY_GPU_VA_SPACE_DELAY",   "Destroy GPU VA space delay test",           1},
    {UVM_TEST_IOCTL_BASE(95), "SEC2_SANITY",                  "SEC2 sanity test",                          1},
    {UVM_TEST_IOCTL_BASE(96), "CGROUP_ACCOUNTING_SUPPORTED",  "CGroup accounting supported test",           0},
    {UVM_TEST_IOCTL_BASE(98), "SPLIT_INVALIDATE_DELAY",       "Split invalidate delay test",               0},
    {UVM_TEST_IOCTL_BASE(99), "SEC2_CPU_GPU_ROUNDTRIP",       "SEC2 CPU-GPU roundtrip test",               1},
    {UVM_TEST_IOCTL_BASE(100), "CPU_CHUNK_API",               "CPU chunk API test",                        0},
    {UVM_TEST_IOCTL_BASE(101), "FORCE_CPU_TO_CPU_COPY_WITH_CE", "Force CPU to CPU copy with CE test",      1},
    {UVM_TEST_IOCTL_BASE(102), "VA_SPACE_ALLOW_MOVABLE_ALLOCATIONS", "VA space allow movable allocations", 0},
    {UVM_TEST_IOCTL_BASE(103), "SKIP_MIGRATE_VMA",            "Skip migrate VMA test",                     0},
};

#define NUM_UVM_TESTS (sizeof(uvm_tests) / sizeof(uvm_tests[0]))

// Global configuration
static struct {
    int verbose;
    int list_only;
    int continue_on_error;
    char *specific_test;
    char *filter_pattern;
    regex_t filter_regex;
    int use_filter;
} config = {0};

// Statistics
static struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
    int skipped_tests;
    time_t start_time;
    time_t end_time;
} stats = {0};

// UVM device file descriptor
static int uvm_fd = -1;

// Function prototypes
static void print_usage(const char *prog_name);
static int parse_arguments(int argc, char *argv[]);
static int open_uvm_device(void);
static void close_uvm_device(void);
static int check_uvm_module_loaded(void);
static int check_gpu_available(void);
static void list_all_tests(void);
static int should_run_test(const uvm_test_case_t *test);
static int run_single_test(const uvm_test_case_t *test);
static int run_all_tests(void);
static void print_test_summary(void);
static const char* get_test_result_string(int result);

int main(int argc, char *argv[])
{
    int ret = 0;
    
    printf("UVM Test Runner - NVIDIA UVM Driver Test Suite\n");
    printf("==============================================\n\n");
    
    // Parse command line arguments
    if (parse_arguments(argc, argv) != 0) {
        return 1;
    }
    
    // List tests if requested
    if (config.list_only) {
        list_all_tests();
        return 0;
    }
    
    // Check if UVM module is loaded
    if (check_uvm_module_loaded() != 0) {
        fprintf(stderr, "Error: UVM module is not loaded or tests are not enabled.\n");
        fprintf(stderr, "Make sure to load the module with: modprobe nvidia-uvm uvm_enable_builtin_tests=1\n");
        return 1;
    }
    
    // Open UVM device
    if (open_uvm_device() != 0) {
        return 1;
    }
    
    // Check GPU availability
    int gpu_available = check_gpu_available();
    if (gpu_available < 0) {
        printf("Warning: Could not determine GPU availability. Some tests may fail.\n");
    } else if (gpu_available == 0) {
        printf("Warning: No GPU detected. GPU-dependent tests will be skipped.\n");
    } else {
        printf("GPU detected. All tests can be executed.\n");
    }
    printf("\n");
    
    // Initialize statistics
    stats.start_time = time(NULL);
    
    // Run tests
    if (config.specific_test) {
        // Run specific test
        const uvm_test_case_t *test = NULL;
        for (size_t i = 0; i < NUM_UVM_TESTS; i++) {
            if (strcmp(uvm_tests[i].name, config.specific_test) == 0) {
                test = &uvm_tests[i];
                break;
            }
        }
        
        if (!test) {
            fprintf(stderr, "Error: Test '%s' not found.\n", config.specific_test);
            ret = 1;
        } else {
            stats.total_tests = 1;
            ret = run_single_test(test);
            if (ret == 0) stats.passed_tests = 1;
            else stats.failed_tests = 1;
        }
    } else {
        // Run all tests (or filtered tests)
        ret = run_all_tests();
    }
    
    stats.end_time = time(NULL);
    
    // Print summary
    print_test_summary();
    
    // Cleanup
    close_uvm_device();
    
    if (config.use_filter) {
        regfree(&config.filter_regex);
    }
    
    return ret;
}

static void print_usage(const char *prog_name)
{
    printf("Usage: %s [options]\n", prog_name);
    printf("\nOptions:\n");
    printf("  -h, --help              Show this help message\n");
    printf("  -l, --list              List all available test cases\n");
    printf("  -t, --test <name>       Run specific test case by name\n");
    printf("  -v, --verbose           Enable verbose output\n");
    printf("  -c, --continue          Continue running tests after failures\n");
    printf("  --filter <pattern>      Run tests matching pattern (regex)\n");
    printf("\nExamples:\n");
    printf("  %s                      # Run all tests\n", prog_name);
    printf("  %s -l                   # List all available tests\n", prog_name);
    printf("  %s -t RNG_SANITY        # Run specific test\n", prog_name);
    printf("  %s --filter \".*SANITY.*\" # Run all sanity tests\n", prog_name);
    printf("  %s -v -c                # Run all tests with verbose output, continue on errors\n", prog_name);
}

static int parse_arguments(int argc, char *argv[])
{
    int c;
    struct option long_options[] = {
        {"help",     no_argument,       0, 'h'},
        {"list",     no_argument,       0, 'l'},
        {"test",     required_argument, 0, 't'},
        {"verbose",  no_argument,       0, 'v'},
        {"continue", no_argument,       0, 'c'},
        {"filter",   required_argument, 0, 'f'},
        {0, 0, 0, 0}
    };
    
    while ((c = getopt_long(argc, argv, "hlt:vcf:", long_options, NULL)) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                exit(0);
                break;
            case 'l':
                config.list_only = 1;
                break;
            case 't':
                config.specific_test = optarg;
                break;
            case 'v':
                config.verbose = 1;
                break;
            case 'c':
                config.continue_on_error = 1;
                break;
            case 'f':
                config.filter_pattern = optarg;
                if (regcomp(&config.filter_regex, config.filter_pattern, REG_EXTENDED) != 0) {
                    fprintf(stderr, "Error: Invalid regex pattern '%s'\n", config.filter_pattern);
                    return -1;
                }
                config.use_filter = 1;
                break;
            case '?':
                return -1;
            default:
                abort();
        }
    }
    
    return 0;
}

static int open_uvm_device(void)
{
    uvm_fd = open("/dev/nvidia-uvm", O_RDWR);
    if (uvm_fd < 0) {
        perror("Error opening /dev/nvidia-uvm");
        fprintf(stderr, "Make sure the nvidia-uvm module is loaded.\n");
        return -1;
    }
    
    if (config.verbose) {
        printf("Successfully opened UVM device.\n");
    }
    
    return 0;
}

static void close_uvm_device(void)
{
    if (uvm_fd >= 0) {
        close(uvm_fd);
        uvm_fd = -1;
    }
}

static int check_uvm_module_loaded(void)
{
    struct stat st;
    
    // Check if device file exists
    if (stat("/dev/nvidia-uvm", &st) != 0) {
        return -1;
    }
    
    // Try to open and perform a simple operation
    int fd = open("/dev/nvidia-uvm", O_RDWR);
    if (fd < 0) {
        return -1;
    }
    
    close(fd);
    return 0;
}

static int check_gpu_available(void)
{
    // Simple check - look for NVIDIA GPU devices
    struct stat st;
    
    for (int i = 0; i < 8; i++) {
        char dev_path[64];
        snprintf(dev_path, sizeof(dev_path), "/dev/nvidia%d", i);
        if (stat(dev_path, &st) == 0) {
            return 1;  // GPU found
        }
    }
    
    return 0;  // No GPU found
}

static void list_all_tests(void)
{
    printf("Available UVM Test Cases (%zu total):\n", NUM_UVM_TESTS);
    printf("=====================================\n\n");
    
    for (size_t i = 0; i < NUM_UVM_TESTS; i++) {
        const uvm_test_case_t *test = &uvm_tests[i];
        printf("%-35s (ID: %3u) %s %s\n", 
               test->name, 
               test->cmd,
               test->requires_gpu ? "[GPU]" : "     ",
               test->description);
    }
    
    printf("\nLegend:\n");
    printf("  [GPU] - Test requires GPU hardware\n");
}

static int should_run_test(const uvm_test_case_t *test)
{
    // Apply filter if specified
    if (config.use_filter) {
        if (regexec(&config.filter_regex, test->name, 0, NULL, 0) != 0) {
            return 0;  // Doesn't match filter
        }
    }
    
    return 1;  // Should run this test
}

static int run_single_test(const uvm_test_case_t *test)
{
    int ret;
    char params[1024] = {0};  // Generic parameter buffer
    
    printf("Running test: %-35s ", test->name);
    fflush(stdout);
    
    if (config.verbose) {
        printf("\n  Description: %s\n", test->description);
        printf("  Command ID: %u\n", test->cmd);
        printf("  Requires GPU: %s\n", test->requires_gpu ? "Yes" : "No");
        printf("  Executing... ");
        fflush(stdout);
    }
    
    // Execute the test via ioctl
    ret = ioctl(uvm_fd, test->cmd, params);
    
    if (ret == 0) {
        printf("[PASS]\n");
        if (config.verbose) {
            printf("  Result: Test completed successfully\n");
        }
        return 0;
    } else {
        printf("[FAIL]\n");
        if (config.verbose) {
            printf("  Result: Test failed with error %d (%s)\n", errno, strerror(errno));
        } else {
            printf("  Error: %s\n", strerror(errno));
        }
        return -1;
    }
}

static int run_all_tests(void)
{
    int overall_result = 0;
    
    printf("Starting UVM test execution...\n");
    printf("Total tests to run: ");
    
    // Count tests that will be run
    for (size_t i = 0; i < NUM_UVM_TESTS; i++) {
        if (should_run_test(&uvm_tests[i])) {
            stats.total_tests++;
        }
    }
    
    printf("%d\n\n", stats.total_tests);
    
    // Run each test
    for (size_t i = 0; i < NUM_UVM_TESTS; i++) {
        const uvm_test_case_t *test = &uvm_tests[i];
        
        if (!should_run_test(test)) {
            stats.skipped_tests++;
            continue;
        }
        
        int result = run_single_test(test);
        
        if (result == 0) {
            stats.passed_tests++;
        } else {
            stats.failed_tests++;
            overall_result = -1;
            
            if (!config.continue_on_error) {
                printf("\nStopping test execution due to failure.\n");
                printf("Use --continue to run all tests regardless of failures.\n");
                break;
            }
        }
        
        // Add small delay between tests
        usleep(10000);  // 10ms
    }
    
    return overall_result;
}

static void print_test_summary(void)
{
    double duration = difftime(stats.end_time, stats.start_time);
    
    printf("\n");
    printf("Test Execution Summary\n");
    printf("=====================\n");
    printf("Total tests:     %d\n", stats.total_tests);
    printf("Passed:          %d\n", stats.passed_tests);
    printf("Failed:          %d\n", stats.failed_tests);
    printf("Skipped:         %d\n", stats.skipped_tests);
    printf("Success rate:    %.1f%%\n", 
           stats.total_tests > 0 ? (100.0 * stats.passed_tests / stats.total_tests) : 0.0);
    printf("Execution time:  %.1f seconds\n", duration);
    
    if (stats.failed_tests > 0) {
        printf("\nSome tests failed. This could be due to:\n");
        printf("- Missing GPU hardware for GPU-dependent tests\n");
        printf("- UVM module not loaded with test support enabled\n");
        printf("- Insufficient permissions\n");
        printf("- Hardware or driver issues\n");
        printf("\nTry running with --verbose for more detailed error information.\n");
    }
}

static const char* get_test_result_string(int result)
{
    return result == 0 ? "PASS" : "FAIL";
}