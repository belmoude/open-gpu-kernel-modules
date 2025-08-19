#!/bin/bash

#******************************************************************************
# UVM Test Runner - 修复版本
# 解决了字符设备打开模式的问题，现在应该能正常工作
#******************************************************************************

set -e

# Script configuration
SCRIPT_NAME=$(basename "$0")
UVM_DEVICE="/dev/nvidia-uvm"
VERBOSE=0
LIST_ONLY=0
CONTINUE_ON_ERROR=0
SPECIFIC_TEST=""
FILTER_PATTERN=""

# Test statistics
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0

# Test case definitions (command_id:name:description:requires_gpu)
declare -a UVM_TESTS=(
    "200:GET_GPU_REF_COUNT:Get GPU reference count:1"
    "201:RNG_SANITY:Random number generator sanity test:0"
    "202:RANGE_TREE_DIRECTED:Directed range tree test:0"
    "203:RANGE_TREE_RANDOM:Random range tree test:0"
    "204:VA_RANGE_INFO:VA range information test:0"
    "205:RM_MEM_SANITY:RM memory sanity test:1"
    "206:GPU_SEMAPHORE_SANITY:GPU semaphore sanity test:1"
    "207:PEER_REF_COUNT:Peer reference count test:1"
    "208:VA_RANGE_SPLIT:VA range split test:0"
    "209:VA_RANGE_INJECT_SPLIT_ERROR:VA range split error injection test:0"
    "210:PAGE_TREE:Page tree test:1"
    "211:CHANGE_PTE_MAPPING:Change PTE mapping test:1"
    "212:TRACKER_SANITY:Tracker sanity test:1"
    "213:PUSH_SANITY:Push sanity test:1"
    "214:CHANNEL_SANITY:Channel sanity test:1"
    "215:CHANNEL_STRESS:Channel stress test:1"
    "216:CE_SANITY:Copy engine sanity test:1"
    "217:VA_BLOCK_INFO:VA block information test:0"
    "218:LOCK_SANITY:Lock sanity test:0"
    "219:PERF_UTILS_SANITY:Performance utils sanity test:0"
    "220:KVMALLOC:Kernel memory allocation test:0"
    "221:PMM_QUERY:Physical memory manager query test:1"
    "222:PMM_CHECK_LEAK:PMM leak check test:1"
    "223:PERF_EVENTS_SANITY:Performance events sanity test:0"
    "224:PERF_MODULE_SANITY:Performance module sanity test:0"
    "225:RANGE_ALLOCATOR_SANITY:Range allocator sanity test:0"
    "226:GET_RM_PTES:Get RM PTEs test:1"
    "227:FAULT_BUFFER_FLUSH:Fault buffer flush test:1"
    "228:INJECT_TOOLS_EVENT:Inject tools event test:0"
    "229:INCREMENT_TOOLS_COUNTER:Increment tools counter test:0"
    "230:MEM_SANITY:Memory sanity test:0"
    "232:MAKE_CHANNEL_STOPS_IMMEDIATE:Make channel stops immediate test:1"
    "233:VA_BLOCK_INJECT_ERROR:VA block error injection test:0"
    "234:PEER_IDENTITY_MAPPINGS:Peer identity mappings test:1"
    "235:VA_RESIDENCY_INFO:VA residency information test:0"
    "236:PMM_ASYNC_ALLOC:PMM async allocation test:1"
    "237:SET_PREFETCH_FILTERING:Set prefetch filtering test:0"
    "240:PMM_SANITY:PMM sanity test:1"
    "241:INVALIDATE_TLB:TLB invalidation test:1"
    "242:VA_BLOCK:VA block test:0"
    "243:EVICT_CHUNK:Evict chunk test:1"
    "244:FLUSH_DEFERRED_WORK:Flush deferred work test:0"
    "245:NV_KTHREAD_Q:NV kernel thread queue test:0"
    "246:SET_PAGE_PREFETCH_POLICY:Set page prefetch policy test:0"
    "247:RANGE_GROUP_TREE:Range group tree test:0"
    "248:RANGE_GROUP_RANGE_INFO:Range group range info test:0"
    "249:RANGE_GROUP_RANGE_COUNT:Range group range count test:0"
    "250:GET_PREFETCH_FAULTS_REENABLE_LAPSE:Get prefetch faults reenable lapse:0"
    "251:SET_PREFETCH_FAULTS_REENABLE_LAPSE:Set prefetch faults reenable lapse:0"
    "252:GET_KERNEL_VIRTUAL_ADDRESS:Get kernel virtual address test:0"
    "253:PMA_ALLOC_FREE:PMA allocation/free test:1"
    "254:PMM_ALLOC_FREE_ROOT:PMM alloc/free root test:1"
    "255:PMM_INJECT_PMA_EVICT_ERROR:PMM inject PMA evict error test:1"
    "256:RECONFIGURE_ACCESS_COUNTERS:Reconfigure access counters test:1"
    "257:RESET_ACCESS_COUNTERS:Reset access counters test:1"
    "258:SET_IGNORE_ACCESS_COUNTERS:Set ignore access counters test:1"
    "259:CHECK_CHANNEL_VA_SPACE:Check channel VA space test:1"
    "260:ENABLE_NVLINK_PEER_ACCESS:Enable NVLink peer access test:1"
    "261:DISABLE_NVLINK_PEER_ACCESS:Disable NVLink peer access test:1"
    "262:GET_PAGE_THRASHING_POLICY:Get page thrashing policy test:0"
    "263:SET_PAGE_THRASHING_POLICY:Set page thrashing policy test:0"
    "264:PMM_SYSMEM:PMM system memory test:0"
    "265:PMM_REVERSE_MAP:PMM reverse mapping test:1"
    "266:PMM_INDIRECT_PEERS:PMM indirect peers test:1"
    "267:VA_SPACE_MM_RETAIN:VA space MM retain test:0"
    "269:PMM_CHUNK_WITH_ELEVATED_PAGE:PMM chunk with elevated page test:1"
    "270:GET_GPU_TIME:Get GPU time test:1"
    "271:ACCESS_COUNTERS_ENABLED_BY_DEFAULT:Access counters enabled by default:1"
    "272:VA_SPACE_INJECT_ERROR:VA space error injection test:0"
    "273:PMM_RELEASE_FREE_ROOT_CHUNKS:PMM release free root chunks test:1"
    "274:DRAIN_REPLAYABLE_FAULTS:Drain replayable faults test:1"
    "275:PMA_GET_BATCH_SIZE:PMA get batch size test:1"
    "276:PMM_QUERY_PMA_STATS:PMM query PMA stats test:1"
    "278:NUMA_CHECK_AFFINITY:NUMA check affinity test:0"
    "279:VA_SPACE_ADD_DUMMY_THREAD_CONTEXTS:VA space add dummy thread contexts:0"
    "280:VA_SPACE_REMOVE_DUMMY_THREAD_CONTEXTS:VA space remove dummy thread contexts:0"
    "281:THREAD_CONTEXT_SANITY:Thread context sanity test:0"
    "282:THREAD_CONTEXT_PERF:Thread context performance test:0"
    "283:GET_PAGEABLE_MEM_ACCESS_TYPE:Get pageable memory access type test:0"
    "284:TOOLS_FLUSH_REPLAY_EVENTS:Tools flush replay events test:0"
    "285:REGISTER_UNLOAD_STATE_BUFFER:Register unload state buffer test:0"
    "286:RB_TREE_DIRECTED:Red-black tree directed test:0"
    "287:RB_TREE_RANDOM:Red-black tree random test:0"
    "288:HOST_SANITY:Host sanity test:1"
    "289:VA_SPACE_MM_OR_CURRENT_RETAIN:VA space MM or current retain test:0"
    "290:GET_USER_SPACE_END_ADDRESS:Get user space end address test:0"
    "291:GET_CPU_CHUNK_ALLOC_SIZES:Get CPU chunk allocation sizes test:0"
    "293:VA_RANGE_INJECT_ADD_GPU_VA_SPACE_ERROR:VA range inject add GPU VA space error:1"
    "294:DESTROY_GPU_VA_SPACE_DELAY:Destroy GPU VA space delay test:1"
    "295:SEC2_SANITY:SEC2 sanity test:1"
    "296:CGROUP_ACCOUNTING_SUPPORTED:CGroup accounting supported test:0"
    "298:SPLIT_INVALIDATE_DELAY:Split invalidate delay test:0"
    "299:SEC2_CPU_GPU_ROUNDTRIP:SEC2 CPU-GPU roundtrip test:1"
    "300:CPU_CHUNK_API:CPU chunk API test:0"
    "301:FORCE_CPU_TO_CPU_COPY_WITH_CE:Force CPU to CPU copy with CE test:1"
    "302:VA_SPACE_ALLOW_MOVABLE_ALLOCATIONS:VA space allow movable allocations:0"
    "303:SKIP_MIGRATE_VMA:Skip migrate VMA test:0"
)

# Function to print usage
print_usage() {
    echo "Usage: $SCRIPT_NAME [options]"
    echo ""
    echo "Options:"
    echo "  -h, --help              Show this help message"
    echo "  -l, --list              List all available test cases"
    echo "  -t, --test <name>       Run specific test case by name"
    echo "  -v, --verbose           Enable verbose output"
    echo "  -c, --continue          Continue running tests after failures"
    echo "  -f, --filter <pattern>  Run tests matching pattern (grep pattern)"
    echo ""
    echo "Examples:"
    echo "  $SCRIPT_NAME                      # Run all tests"
    echo "  $SCRIPT_NAME -l                   # List all available tests"
    echo "  $SCRIPT_NAME -t RNG_SANITY        # Run specific test"
    echo "  $SCRIPT_NAME -f \".*SANITY.*\"       # Run all sanity tests"
    echo "  $SCRIPT_NAME -v -c                # Run all tests with verbose output, continue on errors"
}

# Function to check if UVM module is loaded and tests are enabled
check_uvm_module() {
    if [[ ! -c "$UVM_DEVICE" ]]; then
        echo "Error: UVM device $UVM_DEVICE not found."
        echo "Make sure the nvidia-uvm module is loaded."
        return 1
    fi
    
    if [[ ! -r "$UVM_DEVICE" || ! -w "$UVM_DEVICE" ]]; then
        echo "Error: No read/write access to $UVM_DEVICE."
        echo "Try running as root or check device permissions."
        return 1
    fi
    
    return 0
}

# Function to check GPU availability
check_gpu_available() {
    for i in {0..7}; do
        if [[ -c "/dev/nvidia$i" ]]; then
            return 0  # GPU found
        fi
    done
    return 1  # No GPU found
}

# Function to list all tests
list_all_tests() {
    echo "Available UVM Test Cases (${#UVM_TESTS[@]} total):"
    echo "====================================="
    echo ""
    
    for test_def in "${UVM_TESTS[@]}"; do
        IFS=':' read -r cmd_id name description requires_gpu <<< "$test_def"
        gpu_marker=""
        if [[ "$requires_gpu" == "1" ]]; then
            gpu_marker="[GPU]"
        else
            gpu_marker="     "
        fi
        printf "%-35s (ID: %3s) %s %s\n" "$name" "$cmd_id" "$gpu_marker" "$description"
    done
    
    echo ""
    echo "Legend:"
    echo "  [GPU] - Test requires GPU hardware"
}

# Function to check if a test should be run based on filters
should_run_test() {
    local test_name="$1"
    
    # Check specific test filter
    if [[ -n "$SPECIFIC_TEST" ]]; then
        [[ "$test_name" == "$SPECIFIC_TEST" ]] && return 0 || return 1
    fi
    
    # Check pattern filter
    if [[ -n "$FILTER_PATTERN" ]]; then
        echo "$test_name" | grep -E "$FILTER_PATTERN" >/dev/null 2>&1 && return 0 || return 1
    fi
    
    return 0  # Run by default
}

# Function to run a single test using the FIXED method
run_single_test() {
    local cmd_id="$1"
    local test_name="$2"
    local description="$3"
    local requires_gpu="$4"
    
    printf "Running test: %-35s " "$test_name"
    
    if [[ "$VERBOSE" == "1" ]]; then
        echo ""
        echo "  Description: $description"
        echo "  Command ID: $cmd_id"
        echo "  Requires GPU: $([ "$requires_gpu" == "1" ] && echo "Yes" || echo "No")"
        echo -n "  Executing... "
    fi
    
    # 创建修复版本的Python脚本 - 这是关键修复！
    local python_script=$(mktemp)
    cat > "$python_script" << EOF
#!/usr/bin/env python3
import os
import sys
import fcntl
import array
import errno

try:
    # 修复：使用os.open()打开字符设备，避免seekable问题
    fd = os.open('$UVM_DEVICE', os.O_RDWR)
    try:
        # 修复：使用array而不是bytearray，更适合ioctl
        params = array.array('B', [0] * 1024)
        result = fcntl.ioctl(fd, $cmd_id, params)
        sys.exit(0)
    finally:
        os.close(fd)
except OSError as e:
    # 传递具体的错误码以便调试
    sys.exit(e.errno if e.errno < 128 else 1)
except Exception as e:
    sys.exit(1)
EOF
    
    # Execute the test and capture the exit code
    python3 "$python_script" 2>/dev/null
    local exit_code=$?
    
    if [[ $exit_code -eq 0 ]]; then
        echo "[PASS]"
        if [[ "$VERBOSE" == "1" ]]; then
            echo "  Result: Test completed successfully"
        fi
        PASSED_TESTS=$((PASSED_TESTS + 1))
        rm -f "$python_script"
        return 0
    else
        echo "[FAIL]"
        if [[ "$VERBOSE" == "1" ]]; then
            echo "  Result: Test failed (exit code: $exit_code)"
            case $exit_code in
                22) echo "  Error: Invalid argument (EINVAL) - tests may not be enabled" ;;
                25) echo "  Error: Inappropriate ioctl (ENOTTY) - unsupported command" ;;
                1)  echo "  Error: Operation not permitted (EPERM)" ;;
                19) echo "  Error: No such device (ENODEV)" ;;
                14) echo "  Error: Bad address (EFAULT) - memory access error" 
                    if [[ "$test_name" == "VA_RESIDENCY_INFO" ]]; then
                        echo "  Note: This test requires pre-configured VA space - failure is expected"
                    fi ;;
                *) echo "  Error: Unknown error code $exit_code" ;;
            esac
        else
            case $exit_code in
                22) echo "  Error: Tests not enabled" ;;
                25) echo "  Error: Unsupported command" ;;
                1)  echo "  Error: Permission denied" ;;
                14) echo "  Error: Memory access (expected for some tests)" ;;
                *) echo "  Error: Code $exit_code" ;;
            esac
        fi
        FAILED_TESTS=$((FAILED_TESTS + 1))
        rm -f "$python_script"
        return 1
    fi
}

# Function to run all tests
run_all_tests() {
    local start_time=$(date +%s)
    
    echo "Starting UVM test execution..."
    
    # Count tests that will be run
    for test_def in "${UVM_TESTS[@]}"; do
        IFS=':' read -r cmd_id name description requires_gpu <<< "$test_def"
        if should_run_test "$name"; then
            TOTAL_TESTS=$((TOTAL_TESTS + 1))
        fi
    done
    
    echo "Total tests to run: $TOTAL_TESTS"
    echo ""
    
    # Run each test
    for test_def in "${UVM_TESTS[@]}"; do
        IFS=':' read -r cmd_id name description requires_gpu <<< "$test_def"
        
        if ! should_run_test "$name"; then
            SKIPPED_TESTS=$((SKIPPED_TESTS + 1))
            continue
        fi
        
        if ! run_single_test "$cmd_id" "$name" "$description" "$requires_gpu"; then
            if [[ "$CONTINUE_ON_ERROR" != "1" ]]; then
                echo ""
                echo "Stopping test execution due to failure."
                echo "Use --continue to run all tests regardless of failures."
                break
            fi
        fi
        
        # Small delay between tests
        sleep 0.01
    done
    
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    echo ""
    echo "Test Execution Summary"
    echo "====================="
    echo "Total tests:     $TOTAL_TESTS"
    echo "Passed:          $PASSED_TESTS"
    echo "Failed:          $FAILED_TESTS"
    echo "Skipped:         $SKIPPED_TESTS"
    if [[ $TOTAL_TESTS -gt 0 ]]; then
        local success_rate=$(( (PASSED_TESTS * 100) / TOTAL_TESTS ))
        echo "Success rate:    ${success_rate}%"
    fi
    echo "Execution time:  ${duration} seconds"
    
    if [[ $FAILED_TESTS -gt 0 ]]; then
        echo ""
        if [[ $PASSED_TESTS -eq 0 ]]; then
            echo "All tests failed. Possible causes:"
            echo "- UVM tests not properly enabled"
            echo "- Permission issues"
            echo "- Driver compatibility problems"
        else
            echo "Some tests failed. This could be due to:"
            echo "- Missing GPU hardware for GPU-dependent tests (normal)"
            echo "- Specific feature compatibility issues"
            echo "- Hardware limitations"
        fi
        echo ""
        echo "For detailed diagnosis:"
        echo "- Run with --verbose for more error information"
        echo "- Test specific cases: $0 --test RNG_SANITY --verbose"
        echo "- Check GPU tests separately: $0 --filter \".*GPU.*\" --verbose"
    fi
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            print_usage
            exit 0
            ;;
        -l|--list)
            LIST_ONLY=1
            shift
            ;;
        -t|--test)
            SPECIFIC_TEST="$2"
            shift 2
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -c|--continue)
            CONTINUE_ON_ERROR=1
            shift
            ;;
        -f|--filter)
            FILTER_PATTERN="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            print_usage
            exit 1
            ;;
    esac
done

# Main execution
echo "UVM Test Runner - Fixed Version"
echo "==============================="
echo ""

# List tests if requested
if [[ "$LIST_ONLY" == "1" ]]; then
    list_all_tests
    exit 0
fi

# Check prerequisites
if ! check_uvm_module; then
    exit 1
fi

# Check Python availability
if ! command -v python3 >/dev/null 2>&1; then
    echo "Error: python3 is required but not installed."
    echo "Please install Python 3 to run this script."
    exit 1
fi

# Check GPU availability
if check_gpu_available; then
    echo "GPU detected. All tests can be executed."
else
    echo "Warning: No GPU detected. GPU-dependent tests may fail."
fi
echo ""

# Run tests
run_all_tests

exit $([[ $FAILED_TESTS -eq 0 ]] && echo 0 || echo 1)