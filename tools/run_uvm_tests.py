#!/usr/bin/env python3
import argparse
import errno
import fcntl
import glob
import os
import re
import struct
import sys
from typing import List, Tuple


UVM_DEVICE = "/dev/nvidia-uvm"


def read_file_text(path: str) -> str:
    try:
        with open(path, "r", encoding="utf-8", errors="ignore") as f:
            return f.read()
    except FileNotFoundError:
        return ""


def builtin_tests_enabled() -> bool:
    # Module param paths vary: dashes become underscores in /sys/module
    candidates = [
        "/sys/module/nvidia_uvm/parameters/uvm_enable_builtin_tests",
        "/sys/module/nvidia-uvm/parameters/uvm_enable_builtin_tests",
    ]
    for p in candidates:
        try:
            val = read_file_text(p).strip()
            if val in ("1", "Y", "y", "true", "True"):
                return True
        except Exception:
            pass
    return False


def open_uvm() -> int:
    try:
        return os.open(UVM_DEVICE, os.O_RDWR | os.O_CLOEXEC)
    except FileNotFoundError:
        raise RuntimeError(f"{UVM_DEVICE} not found. Load the nvidia-uvm module first (modprobe nvidia-uvm).")
    except PermissionError:
        raise RuntimeError(f"Permission denied opening {UVM_DEVICE}. Try running as root or adjust permissions.")


def ioctl_mut(fd: int, cmd: int, buf: bytearray) -> int:
    # fcntl.ioctl returns integer result for non-mutating, but with a mutable buffer it returns None on success.
    try:
        fcntl.ioctl(fd, cmd, buf, True)
        return 0
    except OSError as e:
        return -e.errno


def uvm_initialize(fd: int) -> Tuple[int, int]:
    # UVM_INITIALIZE is 0x30000001 with params { u64 flags; u32 rmStatus; /* pad u32 */ }
    UVM_INITIALIZE = 0x30000001
    params = bytearray(16)
    struct.pack_into("<Q", params, 0, 0)  # flags = 0
    # rmStatus left zeroed; kernel fills it
    rc = ioctl_mut(fd, UVM_INITIALIZE, params)
    if rc == 0:
        rm_status = struct.unpack_from("<I", params, 8)[0]
    else:
        rm_status = rc
    return rc, rm_status


def parse_uvm_test_defines(header_text: str) -> List[Tuple[str, int]]:
    # Matches: #define UVM_TEST_FOO   UVM_TEST_IOCTL_BASE(n)
    tests: List[Tuple[str, int]] = []
    pattern = re.compile(r"^#\s*define\s+(UVM_TEST_[A-Z0-9_]+)\s+UVM_TEST_IOCTL_BASE\((\d+)\)", re.M)
    for name, num in pattern.findall(header_text):
        n = int(num)
        cmd = 200 + n  # on Linux, UVM_IOCTL_BASE(i) == i
        tests.append((name, cmd))
    # Deduplicate while preserving order
    seen = set()
    unique: List[Tuple[str, int]] = []
    for name, cmd in tests:
        if name in seen:
            continue
        seen.add(name)
        unique.append((name, cmd))
    return unique


def to_uuid_bytes_from_raw_hex(raw32hex: str) -> bytes:
    s = raw32hex.strip().lower().replace("0x", "")
    if len(s) != 32 or any(c not in "0123456789abcdef" for c in s):
        raise ValueError("GPU UUID raw hex must be 32 hex chars (no dashes, no GPU- prefix). Example: 00000000... (16 bytes)")
    return bytes.fromhex(s)


def uvm_register_gpu(fd: int, uuid_bytes: bytes) -> Tuple[int, int]:
    # UVM_REGISTER_GPU is 37 with params struct (see uvm_ioctl.h). We'll build a 64-byte buffer, zero-filled.
    UVM_REGISTER_GPU = 37
    params = bytearray(64)
    # struct layout (little-endian):
    # NvProcessorUuid (16 bytes)
    params[0:16] = uuid_bytes
    # numaEnabled (u32) at 16: zero (out)
    # numaNodeId (s32) at 20: zero (out)
    # rmCtrlFd (s32) at 24: -1 to indicate none
    struct.pack_into("<i", params, 24, -1)
    # hClient (u32) at 28: 0
    # hSmcPartRef (u32) at 32: 0
    # rmStatus (u32) at 36: out
    rc = ioctl_mut(fd, UVM_REGISTER_GPU, params)
    rm_status = struct.unpack_from("<I", params, 36)[0] if rc == 0 else rc
    return rc, rm_status


def discover_gpu_uuids_proc() -> List[str]:
    # Try to parse /proc/driver/nvidia/gpus/*/information for GPU UUID strings (GPU-XXXXXXXX-...)
    uuids: List[str] = []
    for info_path in glob.glob("/proc/driver/nvidia/gpus/*/information"):
        txt = read_file_text(info_path)
        m = re.search(r"GPU UUID\s*:\s*([A-Za-z0-9\-]+)", txt)
        if m:
            uuids.append(m.group(1))
    return uuids


def main() -> int:
    parser = argparse.ArgumentParser(description="Run NVIDIA UVM kernel built-in tests (developer only)")
    parser.add_argument("--header", default="kernel-open/nvidia-uvm/uvm_test_ioctl.h", help="Path to uvm_test_ioctl.h to enumerate tests")
    parser.add_argument("--run-all", action="store_true", help="Attempt to invoke all discovered UVM_TEST_* ioctls")
    parser.add_argument("--gpu-uuid-raw", action="append", default=[], help="Raw 32-hex-byte GPU UUID(s) to register (e.g., 0000...)")
    parser.add_argument("--skip-gpu-tests", action="store_true", help="Skip invoking tests likely requiring a registered GPU")
    parser.add_argument("--verbose", "-v", action="store_true")
    args = parser.parse_args()

    if not builtin_tests_enabled():
        print("[FATAL] Built-in tests are disabled. Reload the module with uvm_enable_builtin_tests=1:")
        print("        sudo modprobe -r nvidia_uvm || sudo rmmod nvidia-uvm")
        print("        sudo modprobe nvidia-uvm uvm_enable_builtin_tests=1")
        return 1

    fd = open_uvm()
    try:
        rc, rm = uvm_initialize(fd)
        if rc != 0 or rm != 0:
            print(f"[FATAL] UVM_INITIALIZE failed: ioctl_rc={rc}, rmStatus={rm}")
            return 2

        # Optionally register GPU(s)
        for raw in args.gpu_uuid_raw:
            try:
                uuid_b = to_uuid_bytes_from_raw_hex(raw)
            except Exception as e:
                print(f"[WARN] Skipping invalid --gpu-uuid-raw '{raw}': {e}")
                continue
            rcg, rmg = uvm_register_gpu(fd, uuid_b)
            if rcg == 0 and rmg == 0:
                print(f"[OK] Registered GPU {raw}")
            else:
                print(f"[WARN] UVM_REGISTER_GPU failed for {raw}: ioctl_rc={rcg}, rmStatus={rmg}")

        # Parse tests from header
        header_text = read_file_text(args.header)
        if not header_text:
            print(f"[FATAL] Cannot read header: {args.header}")
            return 3
        tests = parse_uvm_test_defines(header_text)
        if not tests:
            print(f"[FATAL] No UVM_TEST_* defines found in {args.header}")
            return 4

        # Heuristic for tests that likely require GPU (skip if requested)
        gpu_keywords = ("GPU", "CHANNEL", "PUSH", "CE_", "NVLINK", "ACCESS_COUNTERS", "PMA", "PMM", "TLB", "SEC2")

        total = 0
        ok = 0
        errs = 0
        skipped = 0

        if not args.run_all:
            print("[INFO] Dry-run mode. Use --run-all to actually invoke the test ioctls.")

        for name, cmd in tests:
            if args.skip_gpu_tests and any(k in name for k in gpu_keywords):
                skipped += 1
                if args.verbose:
                    print(f"[SKIP] {name} (cmd={cmd})")
                continue

            total += 1
            buf = bytearray(4096)  # Oversized buffer; kernel will copy only sizeof(params)
            if args.run_all:
                rc = ioctl_mut(fd, cmd, buf)
                if rc == 0:
                    ok += 1
                    if args.verbose:
                        print(f"[OK] {name} (cmd={cmd})")
                else:
                    errs += 1
                    print(f"[ERR] {name} (cmd={cmd}) -> ioctl rc {rc} ({errno.errorcode.get(-rc, 'UNKNOWN')})")
            else:
                if args.verbose:
                    print(f"[FOUND] {name} (cmd={cmd})")

        print(f"\nSummary: discovered={len(tests)}, attempted={total}, ok={ok}, errors={errs}, skipped={skipped}")
        if not args.gpu_uuid_raw:
            proc_uuids = discover_gpu_uuids_proc()
            if proc_uuids:
                print("Note: Found GPU UUID strings in /proc (convert to raw 32-hex for --gpu-uuid-raw):")
                for u in proc_uuids:
                    print(f"  {u}")
            else:
                print("Note: To run GPU-dependent tests, supply --gpu-uuid-raw <32hex>. Use nvidia-smi or /proc to discover UUIDs.")

        # Exit with non-zero if any ioctl errors occurred when actually running
        if args.run_all and errs > 0:
            return 5
        return 0
    finally:
        try:
            os.close(fd)
        except Exception:
            pass


if __name__ == "__main__":
    sys.exit(main())

