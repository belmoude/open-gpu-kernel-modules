#!/bin/bash

#******************************************************************************
# UVM测试运行器 - 修复版本
# 添加了详细的错误诊断和处理
#******************************************************************************

set -e

# 脚本配置
SCRIPT_NAME=$(basename "$0")
UVM_DEVICE="/dev/nvidia-uvm"
VERBOSE=0
LIST_ONLY=0
CONTINUE_ON_ERROR=0
SPECIFIC_TEST=""
FILTER_PATTERN=""
DEBUG_MODE=0

# 测试统计
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0

# 简化的测试用例列表（前20个用于快速测试）
declare -a UVM_TESTS=(
    "290:GET_USER_SPACE_END_ADDRESS:Get user space end address test:0"
    "296:CGROUP_ACCOUNTING_SUPPORTED:CGroup accounting supported test:0"
    "201:RNG_SANITY:Random number generator sanity test:0"
    "218:LOCK_SANITY:Lock sanity test:0"
    "220:KVMALLOC:Kernel memory allocation test:0"
    "230:MEM_SANITY:Memory sanity test:0"
    "223:PERF_EVENTS_SANITY:Performance events sanity test:0"
    "224:PERF_MODULE_SANITY:Performance module sanity test:0"
    "225:RANGE_ALLOCATOR_SANITY:Range allocator sanity test:0"
    "245:NV_KTHREAD_Q:NV kernel thread queue test:0"
    "200:GET_GPU_REF_COUNT:Get GPU reference count:1"
    "205:RM_MEM_SANITY:RM memory sanity test:1"
    "206:GPU_SEMAPHORE_SANITY:GPU semaphore sanity test:1"
    "212:TRACKER_SANITY:Tracker sanity test:1"
    "213:PUSH_SANITY:Push sanity test:1"
)

# 打印使用说明
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
    echo "  --debug                 Enable debug mode with detailed error info"
    echo "  --quick                 Run only first 10 tests for quick validation"
}

# 检查UVM模块状态
check_uvm_module() {
    if [[ ! -c "$UVM_DEVICE" ]]; then
        echo "Error: UVM device $UVM_DEVICE not found."
        echo "Make sure the nvidia-uvm module is loaded."
        return 1
    fi
    
    if [[ ! -r "$UVM_DEVICE" || ! -w "$UVM_DEVICE" ]]; then
        echo "Error: No read/write access to $UVM_DEVICE."
        echo "Try running as root or check device permissions."
        echo "Current permissions: $(ls -la $UVM_DEVICE 2>/dev/null || echo 'unknown')"
        return 1
    fi
    
    return 0
}

# 检查UVM测试是否启用
check_uvm_tests_enabled() {
    if [[ -f /sys/module/nvidia_uvm/parameters/uvm_enable_builtin_tests ]]; then
        local enabled=$(cat /sys/module/nvidia_uvm/parameters/uvm_enable_builtin_tests 2>/dev/null || echo "unknown")
        if [[ "$enabled" != "1" ]]; then
            echo "Warning: UVM builtin tests may not be enabled (current: $enabled)"
            echo "Try: sudo modprobe -r nvidia_uvm && sudo modprobe nvidia-uvm uvm_enable_builtin_tests=1"
            return 1
        else
            echo "UVM builtin tests are enabled."
            return 0
        fi
    else
        echo "Warning: Cannot check UVM test status"
        return 1
    fi
}

# 列出所有测试
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

# 检查是否应该运行测试
should_run_test() {
    local test_name="$1"
    
    # 检查特定测试过滤器
    if [[ -n "$SPECIFIC_TEST" ]]; then
        [[ "$test_name" == "$SPECIFIC_TEST" ]] && return 0 || return 1
    fi
    
    # 检查模式过滤器
    if [[ -n "$FILTER_PATTERN" ]]; then
        echo "$test_name" | grep -E "$FILTER_PATTERN" >/dev/null 2>&1 && return 0 || return 1
    fi
    
    return 0  # 默认运行
}

# 运行单个测试
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
    
    # 创建详细的Python测试脚本
    local python_script=$(mktemp)
    cat > "$python_script" << EOF
#!/usr/bin/env python3
import os
import sys
import fcntl
import errno

try:
    with open('$UVM_DEVICE', 'rb+') as f:
        params = bytearray(1024)
        result = fcntl.ioctl(f, $cmd_id, params)
        sys.exit(0)
except OSError as e:
    if $DEBUG_MODE:
        print(f"Debug: OSError {e.errno}: {e.strerror}", file=sys.stderr)
    sys.exit(e.errno)
except Exception as e:
    if $DEBUG_MODE:
        print(f"Debug: Exception: {e}", file=sys.stderr)
    sys.exit(1)
EOF
    
    # 执行测试并捕获详细错误信息
    local error_output
    error_output=$(python3 "$python_script" 2>&1)
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
        
        # 详细的错误分析
        if [[ "$VERBOSE" == "1" ]] || [[ "$DEBUG_MODE" == "1" ]]; then
            echo "  Result: Test failed (exit code: $exit_code)"
            
            case $exit_code in
                22)  # EINVAL
                    echo "  Error: Invalid argument - tests may not be enabled"
                    echo "  Try: sudo modprobe -r nvidia_uvm && sudo modprobe nvidia-uvm uvm_enable_builtin_tests=1"
                    ;;
                25)  # ENOTTY
                    echo "  Error: Inappropriate ioctl for device"
                    ;;
                1)   # EPERM
                    echo "  Error: Operation not permitted - try running as root"
                    ;;
                19)  # ENODEV
                    echo "  Error: No such device"
                    ;;
                *)
                    echo "  Error: Unknown error (code: $exit_code)"
                    if [[ -n "$error_output" ]]; then
                        echo "  Debug info: $error_output"
                    fi
                    ;;
            esac
        else
            # 简单错误信息
            case $exit_code in
                22) echo "  Error: Tests not enabled" ;;
                1)  echo "  Error: Permission denied" ;;
                *) echo "  Error: Code $exit_code" ;;
            esac
        fi
        
        FAILED_TESTS=$((FAILED_TESTS + 1))
        rm -f "$python_script"
        return 1
    fi
}

# 运行所有测试
run_all_tests() {
    local start_time=$(date +%s)
    
    echo "Starting UVM test execution..."
    
    # 统计要运行的测试
    for test_def in "${UVM_TESTS[@]}"; do
        IFS=':' read -r cmd_id name description requires_gpu <<< "$test_def"
        if should_run_test "$name"; then
            TOTAL_TESTS=$((TOTAL_TESTS + 1))
        fi
    done
    
    echo "Total tests to run: $TOTAL_TESTS"
    echo ""
    
    # 运行每个测试
    local test_count=0
    for test_def in "${UVM_TESTS[@]}"; do
        IFS=':' read -r cmd_id name description requires_gpu <<< "$test_def"
        
        if ! should_run_test "$name"; then
            SKIPPED_TESTS=$((SKIPPED_TESTS + 1))
            continue
        fi
        
        test_count=$((test_count + 1))
        
        if ! run_single_test "$cmd_id" "$name" "$description" "$requires_gpu"; then
            if [[ "$CONTINUE_ON_ERROR" != "1" ]]; then
                echo ""
                echo "Stopping test execution due to failure."
                echo "Use --continue to run all tests regardless of failures."
                echo "Use --debug for detailed error information."
                break
            fi
        fi
        
        # 小延迟
        sleep 0.01
        
        # 如果是快速模式，只运行前10个测试
        if [[ "$QUICK_MODE" == "1" ]] && [[ $test_count -ge 10 ]]; then
            echo ""
            echo "Quick mode: Stopping after 10 tests"
            break
        fi
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
    
    # 提供具体的建议
    if [[ $FAILED_TESTS -gt 0 ]]; then
        echo ""
        if [[ $PASSED_TESTS -eq 0 ]]; then
            echo "All tests failed. Most likely causes:"
            echo "1. UVM tests not enabled: sudo modprobe nvidia-uvm uvm_enable_builtin_tests=1"
            echo "2. Permission issue: run as root"
            echo "3. Driver compatibility issue"
        else
            echo "Some tests failed. This could be due to:"
            echo "- Missing GPU hardware for GPU-dependent tests"
            echo "- Specific feature compatibility issues"
        fi
        echo ""
        echo "For detailed diagnosis, run:"
        echo "$0 --debug --verbose --test RNG_SANITY"
    fi
}

# 解析命令行参数
QUICK_MODE=0
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
        --debug)
            DEBUG_MODE=1
            VERBOSE=1
            shift
            ;;
        --quick)
            QUICK_MODE=1
            shift
            ;;
        *)
            echo "Unknown option: $1"
            print_usage
            exit 1
            ;;
    esac
done

# 主执行流程
echo "UVM Test Runner - Enhanced Version"
echo "================================="
echo ""

# 列出测试（如果请求）
if [[ "$LIST_ONLY" == "1" ]]; then
    list_all_tests
    exit 0
fi

# 检查前置条件
echo "Checking prerequisites..."

if ! check_uvm_module; then
    echo ""
    echo "Diagnostic suggestions:"
    echo "1. Load UVM module: sudo modprobe nvidia-uvm uvm_enable_builtin_tests=1"
    echo "2. Check device permissions: ls -la /dev/nvidia-uvm"
    echo "3. Run as root: sudo $0"
    exit 1
fi

# 检查测试是否启用
check_uvm_tests_enabled

# 检查Python
if ! command -v python3 >/dev/null 2>&1; then
    echo "Error: python3 is required but not installed."
    exit 1
fi

echo "All prerequisites met."
echo ""

# 运行测试
run_all_tests

exit $([[ $FAILED_TESTS -eq 0 ]] && echo 0 || echo 1)