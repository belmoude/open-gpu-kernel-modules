#!/bin/bash

#******************************************************************************
# UVM Test Runner Script - Demo Mode
# 
# This version includes a demo mode that simulates test execution even when
# the UVM device is not available, so you can see the complete test flow.
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
DEMO_MODE=0

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
    echo "  -d, --demo              Enable demo mode (simulate test execution)"
    echo ""
    echo "Examples:"
    echo "  $SCRIPT_NAME                      # Run all tests"
    echo "  $SCRIPT_NAME -l                   # List all available tests"
    echo "  $SCRIPT_NAME -t RNG_SANITY        # Run specific test"
    echo "  $SCRIPT_NAME -f \".*SANITY.*\"       # Run all sanity tests"
    echo "  $SCRIPT_NAME -d -v                # Demo mode with verbose output"
}

# Function to check if UVM module is loaded and tests are enabled
check_uvm_module() {
    if [[ "$DEMO_MODE" == "1" ]]; then
        echo "Demo mode: Skipping UVM device check"
        return 0
    fi
    
    if [[ ! -c "$UVM_DEVICE" ]]; then
        echo "Error: UVM device $UVM_DEVICE not found."
        echo "Make sure the nvidia-uvm module is loaded."
        echo "Or use --demo mode to simulate test execution."
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
    if [[ "$DEMO_MODE" == "1" ]]; then
        return 1  # Simulate no GPU in demo mode
    fi
    
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

# Function to simulate or run a single test
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
    
    if [[ "$DEMO_MODE" == "1" ]]; then
        # Demo mode - simulate test results
        sleep 0.1  # Simulate execution time
        
        # Simulate different outcomes based on test characteristics
        if [[ "$requires_gpu" == "1" ]]; then
            # GPU tests fail in demo mode
            echo "[FAIL]"
            if [[ "$VERBOSE" == "1" ]]; then
                echo "  Result: Test failed (No GPU in demo mode)"
            else
                echo "  Error: No GPU hardware available"
            fi
            ((FAILED_TESTS++))
            return 1
        else
            # Non-GPU tests pass in demo mode
            echo "[PASS]"
            if [[ "$VERBOSE" == "1" ]]; then
                echo "  Result: Test completed successfully (simulated)"
            fi
            ((PASSED_TESTS++))
            return 0
        fi
    else
        # Real mode - try to execute actual test
        local python_script=$(mktemp)
        cat > "$python_script" << EOF
#!/usr/bin/env python3
import os
import sys
import fcntl
import struct

try:
    with open('$UVM_DEVICE', 'rb+') as f:
        params = bytearray(1024)
        result = fcntl.ioctl(f, $cmd_id, params)
        sys.exit(0)
except Exception as e:
    sys.exit(1)
EOF
        
        if python3 "$python_script" 2>/dev/null; then
            echo "[PASS]"
            if [[ "$VERBOSE" == "1" ]]; then
                echo "  Result: Test completed successfully"
            fi
            ((PASSED_TESTS++))
            rm -f "$python_script"
            return 0
        else
            echo "[FAIL]"
            if [[ "$VERBOSE" == "1" ]]; then
                echo "  Result: Test failed"
            fi
            ((FAILED_TESTS++))
            rm -f "$python_script"
            return 1
        fi
    fi
}

# Function to run all tests
run_all_tests() {
    local start_time=$(date +%s)
    
    echo "Starting UVM test execution..."
    if [[ "$DEMO_MODE" == "1" ]]; then
        echo "Running in DEMO mode - simulating test execution"
    fi
    
    # Count tests that will be run
    for test_def in "${UVM_TESTS[@]}"; do
        IFS=':' read -r cmd_id name description requires_gpu <<< "$test_def"
        if should_run_test "$name"; then
            ((TOTAL_TESTS++))
        fi
    done
    
    echo "Total tests to run: $TOTAL_TESTS"
    echo ""
    
    # Run each test
    for test_def in "${UVM_TESTS[@]}"; do
        IFS=':' read -r cmd_id name description requires_gpu <<< "$test_def"
        
        if ! should_run_test "$name"; then
            ((SKIPPED_TESTS++))
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
    if [[ "$DEMO_MODE" == "1" ]]; then
        echo "Mode:            DEMO (simulated execution)"
    else
        echo "Mode:            REAL (actual hardware)"
    fi
    echo "Total tests:     $TOTAL_TESTS"
    echo "Passed:          $PASSED_TESTS"
    echo "Failed:          $FAILED_TESTS"
    echo "Skipped:         $SKIPPED_TESTS"
    if [[ $TOTAL_TESTS -gt 0 ]]; then
        local success_rate=$(( (PASSED_TESTS * 100) / TOTAL_TESTS ))
        echo "Success rate:    ${success_rate}%"
    fi
    echo "Execution time:  ${duration} seconds"
    
    if [[ "$DEMO_MODE" == "1" ]]; then
        echo ""
        echo "Demo Mode Results:"
        echo "- Non-GPU tests: PASS (simulated success)"
        echo "- GPU tests: FAIL (no GPU hardware in demo)"
        echo ""
        echo "To run real tests, ensure:"
        echo "1. NVIDIA GPU hardware is present"
        echo "2. NVIDIA drivers are installed"
        echo "3. UVM module loaded: modprobe nvidia-uvm uvm_enable_builtin_tests=1"
    elif [[ $FAILED_TESTS -gt 0 ]]; then
        echo ""
        echo "Some tests failed. This could be due to:"
        echo "- Missing GPU hardware for GPU-dependent tests"
        echo "- UVM module not loaded with test support enabled"
        echo "- Insufficient permissions"
        echo "- Hardware or driver issues"
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
        -d|--demo)
            DEMO_MODE=1
            shift
            ;;
        *)
            echo "Unknown option: $1"
            print_usage
            exit 1
            ;;
    esac
done

# Main execution
echo "UVM Test Runner - NVIDIA UVM Driver Test Suite"
echo "=============================================="
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

# Check Python availability (only for real mode)
if [[ "$DEMO_MODE" != "1" ]] && ! command -v python3 >/dev/null 2>&1; then
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