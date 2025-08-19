# UVM Test Runner

A comprehensive user-space test suite for the NVIDIA UVM (Unified Virtual Memory) driver. This tool allows you to execute all available UVM test cases to verify driver functionality and system compatibility.

## Overview

The NVIDIA UVM driver includes built-in test cases that can be accessed through ioctl calls. This test runner provides an easy-to-use interface to execute all 90+ available test cases, with features like filtering, verbose output, and detailed reporting.

## Files

- `uvm_test_runner.c` - C implementation of the test runner
- `run_uvm_tests.sh` - Shell script implementation (easier to use)
- `Makefile.test` - Makefile for building the C version
- `README_UVM_TESTS.md` - This documentation file

## Prerequisites

### System Requirements

1. **NVIDIA GPU**: Some tests require NVIDIA GPU hardware
2. **NVIDIA UVM Module**: The nvidia-uvm kernel module must be loaded with test support enabled
3. **Permissions**: Root access or appropriate device permissions
4. **Python 3**: Required for the shell script version (for reliable ioctl calls)

### Loading the UVM Module with Test Support

The UVM tests are disabled by default for security reasons. To enable them:

```bash
# Load the module with test support enabled
sudo modprobe nvidia-uvm uvm_enable_builtin_tests=1

# Verify the module is loaded
lsmod | grep nvidia_uvm

# Check that the device file exists
ls -l /dev/nvidia-uvm
```

### Permissions

Make sure you have access to the UVM device:

```bash
# Option 1: Run as root
sudo ./run_uvm_tests.sh

# Option 2: Add your user to the appropriate group (if configured)
sudo usermod -a -G nvidia $USER
# Then log out and back in

# Option 3: Temporarily change device permissions (not recommended for production)
sudo chmod 666 /dev/nvidia-uvm
```

## Usage

### Shell Script Version (Recommended)

The shell script is easier to use and doesn't require compilation:

```bash
# Run all tests
./run_uvm_tests.sh

# List all available tests
./run_uvm_tests.sh --list

# Run a specific test
./run_uvm_tests.sh --test RNG_SANITY

# Run all sanity tests
./run_uvm_tests.sh --filter ".*SANITY.*"

# Run with verbose output and continue on errors
./run_uvm_tests.sh --verbose --continue

# Show help
./run_uvm_tests.sh --help
```

### C Version

First, compile the C version:

```bash
# Build the test runner
make -f Makefile.test

# Optionally install it system-wide
make -f Makefile.test install
```

Then use it similarly to the shell script:

```bash
# Run all tests
./uvm_test_runner

# List all available tests
./uvm_test_runner --list

# Run a specific test
./uvm_test_runner --test RNG_SANITY

# Run with filtering
./uvm_test_runner --filter ".*SANITY.*"
```

## Available Test Cases

The test suite includes 90+ test cases covering various aspects of the UVM driver:

### Core Functionality Tests
- **RNG_SANITY**: Random number generator sanity test
- **RANGE_TREE_DIRECTED/RANDOM**: Range tree data structure tests
- **VA_RANGE_***: Virtual address range management tests
- **VA_BLOCK_***: Virtual address block management tests

### Memory Management Tests
- **PMM_***: Physical Memory Manager tests
- **PMA_***: Physical Memory Allocator tests
- **MEM_SANITY**: General memory management tests
- **KVMALLOC**: Kernel memory allocation tests

### GPU-Specific Tests (require GPU hardware)
- **GPU_SEMAPHORE_SANITY**: GPU semaphore functionality
- **CHANNEL_***: GPU channel management tests
- **CE_SANITY**: Copy Engine tests
- **TRACKER_SANITY**: GPU operation tracking tests
- **PUSH_SANITY**: GPU push buffer tests

### Performance and Tools Tests
- **PERF_***: Performance monitoring and utilities
- **TOOLS_***: UVM tools interface tests
- **THREAD_CONTEXT_***: Thread context management tests

### Advanced Features Tests
- **ACCESS_COUNTERS_***: Memory access counter tests (newer GPUs)
- **NVLINK_PEER_***: NVLink peer access tests (multi-GPU systems)
- **SEC2_***: SEC2 engine tests (newer architectures)
- **FAULT_BUFFER_***: GPU fault handling tests

## Command Line Options

### Shell Script Options
```
-h, --help              Show help message
-l, --list              List all available test cases
-t, --test <name>       Run specific test case by name
-v, --verbose           Enable verbose output
-c, --continue          Continue running tests after failures
-f, --filter <pattern>  Run tests matching pattern (grep regex)
```

### C Program Options
```
-h, --help              Show help message
-l, --list              List all available test cases
-t, --test <name>       Run specific test case by name
-v, --verbose           Enable verbose output
-c, --continue          Continue running tests after failures
--filter <pattern>      Run tests matching pattern (POSIX regex)
```

## Understanding Test Results

### Test Status
- **[PASS]**: Test completed successfully
- **[FAIL]**: Test failed (see error details)
- **Skipped**: Test was not run due to filtering

### Common Failure Reasons

1. **Missing GPU Hardware**: GPU-dependent tests will fail on systems without NVIDIA GPUs
2. **Module Not Loaded**: UVM module not loaded or tests not enabled
3. **Permission Denied**: Insufficient permissions to access `/dev/nvidia-uvm`
4. **Hardware Issues**: Actual hardware or driver problems
5. **Resource Constraints**: Insufficient memory or other system resources

### GPU-Dependent Tests

Tests marked with `[GPU]` in the list require NVIDIA GPU hardware. On systems without GPUs, these tests will fail, which is expected behavior.

## Examples

### Basic Usage
```bash
# Quick test run - just the basic tests
./run_uvm_tests.sh --filter "RNG_SANITY|LOCK_SANITY|KVMALLOC"

# Full test suite with detailed output
./run_uvm_tests.sh --verbose --continue

# Check what tests are available
./run_uvm_tests.sh --list | grep -v GPU
```

### Debugging Failed Tests
```bash
# Run a specific failing test with verbose output
./run_uvm_tests.sh --test CHANNEL_SANITY --verbose

# Run all sanity tests to identify problematic areas
./run_uvm_tests.sh --filter ".*SANITY.*" --verbose --continue
```

### System Validation
```bash
# Quick system check (non-GPU tests only)
./run_uvm_tests.sh --filter "RNG_SANITY|LOCK_SANITY|KVMALLOC|MEM_SANITY|PERF_UTILS_SANITY"

# Full validation including GPU tests
sudo ./run_uvm_tests.sh --continue
```

## Troubleshooting

### UVM Module Issues
```bash
# Check if module is loaded
lsmod | grep nvidia

# Check module parameters
cat /sys/module/nvidia_uvm/parameters/uvm_enable_builtin_tests

# Reload module with test support
sudo modprobe -r nvidia_uvm
sudo modprobe nvidia_uvm uvm_enable_builtin_tests=1
```

### Permission Issues
```bash
# Check device permissions
ls -l /dev/nvidia-uvm

# Check if you're in the right group
groups

# Temporarily fix permissions (as root)
sudo chmod 666 /dev/nvidia-uvm
```

### GPU Detection Issues
```bash
# Check for NVIDIA devices
ls -l /dev/nvidia*

# Check GPU status
nvidia-smi  # If available

# Check driver status
cat /proc/driver/nvidia/version
```

## Integration with CI/CD

The test runner returns appropriate exit codes for integration with automated testing:

```bash
# Exit code 0: All tests passed
# Exit code 1: Some tests failed

# Example CI usage
if ./run_uvm_tests.sh --continue; then
    echo "All UVM tests passed"
else
    echo "Some UVM tests failed - check logs"
    exit 1
fi
```

## Safety and Security Notes

- UVM tests are disabled by default for security reasons
- Only enable tests in development/testing environments
- Tests may allocate significant GPU memory
- Some tests may temporarily affect system performance
- Always run tests in a controlled environment

## Contributing

To add new test cases or improve the runner:

1. Check `kernel-open/nvidia-uvm/uvm_test_ioctl.h` for new test definitions
2. Update the test arrays in both the C and shell versions
3. Add appropriate descriptions and GPU requirements
4. Test the changes on systems with and without GPUs

## License

This test runner is provided under the same license terms as the NVIDIA UVM driver code.