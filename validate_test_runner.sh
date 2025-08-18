#!/bin/bash

#******************************************************************************
# UVM Test Runner Validation Script
# 
# This script validates that the test runner is working correctly by testing
# its basic functionality without requiring the actual UVM module to be loaded.
#******************************************************************************

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_RUNNER="$SCRIPT_DIR/run_uvm_tests.sh"
C_RUNNER="$SCRIPT_DIR/uvm_test_runner"

echo "UVM Test Runner Validation"
echo "========================="
echo ""

# Test 1: Check if shell script exists and is executable
echo "Test 1: Checking shell script..."
if [[ -f "$TEST_RUNNER" && -x "$TEST_RUNNER" ]]; then
    echo "  ✓ Shell script exists and is executable"
else
    echo "  ✗ Shell script not found or not executable"
    exit 1
fi

# Test 2: Test help output
echo ""
echo "Test 2: Testing help output..."
if "$TEST_RUNNER" --help >/dev/null 2>&1; then
    echo "  ✓ Help option works"
else
    echo "  ✗ Help option failed"
    exit 1
fi

# Test 3: Test list functionality
echo ""
echo "Test 3: Testing list functionality..."
if "$TEST_RUNNER" --list | grep -q "GET_GPU_REF_COUNT"; then
    echo "  ✓ List option works and shows expected tests"
else
    echo "  ✗ List option failed or missing expected tests"
    exit 1
fi

# Test 4: Count number of tests
echo ""
echo "Test 4: Counting available tests..."
TEST_COUNT=$("$TEST_RUNNER" --list | grep -c "^[A-Z]")
if [[ $TEST_COUNT -ge 80 ]]; then
    echo "  ✓ Found $TEST_COUNT tests (expected 80+)"
else
    echo "  ✗ Only found $TEST_COUNT tests (expected 80+)"
    exit 1
fi

# Test 5: Check if C version can be built
echo ""
echo "Test 5: Testing C version build..."
if [[ -f "$SCRIPT_DIR/Makefile.test" ]]; then
    if make -f "$SCRIPT_DIR/Makefile.test" -C "$SCRIPT_DIR" >/dev/null 2>&1; then
        echo "  ✓ C version builds successfully"
        
        # Test C version help
        if [[ -f "$C_RUNNER" ]]; then
            if "$C_RUNNER" --help >/dev/null 2>&1; then
                echo "  ✓ C version help works"
            else
                echo "  ⚠ C version built but help failed"
            fi
        fi
    else
        echo "  ⚠ C version failed to build (may need gcc installed)"
    fi
else
    echo "  ⚠ Makefile not found"
fi

# Test 6: Test filtering functionality
echo ""
echo "Test 6: Testing filter functionality..."
SANITY_COUNT=$("$TEST_RUNNER" --list | grep -c "SANITY")
FILTERED_COUNT=$("$TEST_RUNNER" --filter ".*SANITY.*" --list 2>/dev/null | grep -c "SANITY" || echo "0")

if [[ $FILTERED_COUNT -gt 0 ]]; then
    echo "  ✓ Filter functionality works (found $FILTERED_COUNT SANITY tests)"
else
    echo "  ⚠ Filter functionality may not work as expected"
fi

# Test 7: Check Python dependency
echo ""
echo "Test 7: Checking Python dependency..."
if command -v python3 >/dev/null 2>&1; then
    echo "  ✓ Python 3 is available"
else
    echo "  ⚠ Python 3 not found - shell script may not work for actual test execution"
fi

# Test 8: Check for UVM device (informational)
echo ""
echo "Test 8: Checking UVM device availability..."
if [[ -c "/dev/nvidia-uvm" ]]; then
    echo "  ✓ UVM device exists at /dev/nvidia-uvm"
    if [[ -r "/dev/nvidia-uvm" && -w "/dev/nvidia-uvm" ]]; then
        echo "  ✓ UVM device is accessible"
    else
        echo "  ⚠ UVM device exists but not accessible (may need root or proper permissions)"
    fi
else
    echo "  ⚠ UVM device not found - tests cannot run without UVM module loaded"
fi

# Test 9: Check for GPU devices (informational)
echo ""
echo "Test 9: Checking GPU device availability..."
GPU_COUNT=0
for i in {0..7}; do
    if [[ -c "/dev/nvidia$i" ]]; then
        ((GPU_COUNT++))
    fi
done

if [[ $GPU_COUNT -gt 0 ]]; then
    echo "  ✓ Found $GPU_COUNT GPU device(s)"
else
    echo "  ⚠ No GPU devices found - GPU-dependent tests will fail"
fi

echo ""
echo "Validation Summary:"
echo "=================="
echo "✓ = Test passed"
echo "⚠ = Warning (may affect functionality)"
echo "✗ = Test failed"
echo ""

# Final recommendations
echo "Recommendations:"
echo "================"

if [[ ! -c "/dev/nvidia-uvm" ]]; then
    echo "- Load the UVM module: sudo modprobe nvidia-uvm uvm_enable_builtin_tests=1"
fi

if [[ -c "/dev/nvidia-uvm" && ! -r "/dev/nvidia-uvm" ]]; then
    echo "- Run tests as root: sudo ./run_uvm_tests.sh"
    echo "- Or fix permissions: sudo chmod 666 /dev/nvidia-uvm"
fi

if ! command -v python3 >/dev/null 2>&1; then
    echo "- Install Python 3 for full functionality"
fi

if [[ $GPU_COUNT -eq 0 ]]; then
    echo "- GPU-dependent tests will fail on this system (expected if no NVIDIA GPU)"
fi

echo ""
echo "The test runner appears to be properly set up!"
echo "Run './run_uvm_tests.sh --help' for usage information."