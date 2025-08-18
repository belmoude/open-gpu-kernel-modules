#!/usr/bin/env python3
"""
run_uvm_tests.py

A user-space Python runner that discovers all UVM test classes and executes them
with a configurable simulator command template.

Discovery:
- Scans files with extensions: .sv, .svh, .v, .vh to find classes that extend uvm_test
- Also scans for uvm_component_utils/uvm_component_utils_begin macros as a fallback

Execution:
- Provide a simulator command template via --cmd-template with a {test} placeholder
  Examples:
    VCS:      --cmd-template "./simv +UVM_TESTNAME={test} -l logs/{test}.log"
    Questa:   --cmd-template "vsim -c work.tb_top +UVM_TESTNAME={test} -do 'run -all; quit -f' -l logs/{test}.log"
    Xcelium:  --cmd-template "xrun -R +UVM_TESTNAME={test} -l logs/{test}.log"

Features:
- Parallel execution with -j/--jobs
- Timeout per test
- Optional JUnit XML report for CI systems
- Filters: include by regex/name list, exclude by regex

Exit status:
- Returns non-zero if any test fails or if no tests are found (unless --allow-empty)
"""

import argparse
import concurrent.futures
import os
import re
import shlex
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List, Optional, Set, Tuple


UVM_TEST_CLASS_RE = re.compile(r"\bclass\s+(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s+extends\s+uvm_test\b")
UVM_UTILS_RE = re.compile(r"\buvm_component_utils(?:_begin)?\s*\(\s*(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*\)")


@dataclass
class TestResult:
    test_name: str
    passed: bool
    exit_code: int
    duration_s: float
    log_path: Optional[Path]
    reason: str = ""


def find_candidate_files(root: Path, exts: Tuple[str, ...]) -> Iterable[Path]:
    for dirpath, dirnames, filenames in os.walk(root):
        # Skip common build/output directories
        skip_dirs = {".git", "out", "build", "simv.daidir", "work", "xcelium.d", "incr"}
        dirnames[:] = [d for d in dirnames if d not in skip_dirs]
        for filename in filenames:
            lower = filename.lower()
            if any(lower.endswith(ext) for ext in exts):
                yield Path(dirpath) / filename


def discover_tests(root: Path, include_regex: Optional[re.Pattern], exclude_regex: Optional[re.Pattern],
                   exts: Tuple[str, ...]) -> List[str]:
    tests: Set[str] = set()
    for file_path in find_candidate_files(root, exts):
        try:
            text = file_path.read_text(errors="ignore")
        except Exception:
            continue

        for m in UVM_TEST_CLASS_RE.finditer(text):
            tests.add(m.group("name"))
        for m in UVM_UTILS_RE.finditer(text):
            # Not all utils entries are tests, but include as candidates
            tests.add(m.group("name"))

    # Apply include/exclude filters
    discovered = sorted(tests)
    if include_regex is not None:
        discovered = [t for t in discovered if include_regex.search(t)]
    if exclude_regex is not None:
        discovered = [t for t in discovered if not exclude_regex.search(t)]
    return discovered


def parse_test_list_arg(values: Optional[List[str]]) -> Optional[Set[str]]:
    if not values:
        return None
    names: Set[str] = set()
    for v in values:
        parts = [p.strip() for p in v.replace(",", " ").split() if p.strip()]
        names.update(parts)
    return names or None


def make_command(template: str, test: str) -> List[str]:
    # Allow both {test} and $TEST placeholders
    replaced = template.replace("{test}", test).replace("$TEST", test)
    # Split using shell-like syntax, but do not execute in shell for safety
    return shlex.split(replaced)


def run_single_test(test: str, cmd_template: str, log_dir: Path, timeout_s: Optional[int]) -> TestResult:
    log_dir.mkdir(parents=True, exist_ok=True)
    log_path = log_dir / f"{test}.log"
    cmd = make_command(cmd_template, test)

    # Ensure log redirection if not already provided in template
    redirect_in_template = any(token in {"-l", "+UVM_LOG", "-logfile"} or token.startswith("-l") for token in cmd)

    start = time.time()
    try:
        with log_path.open("w") as log_file:
            proc = subprocess.Popen(cmd, stdout=log_file, stderr=subprocess.STDOUT)
            try:
                exit_code = proc.wait(timeout=timeout_s)
            except subprocess.TimeoutExpired:
                proc.kill()
                duration = time.time() - start
                return TestResult(test, False, -1, duration, log_path, reason=f"timeout after {timeout_s}s")
    except FileNotFoundError as e:
        duration = time.time() - start
        return TestResult(test, False, 127, duration, log_path, reason=f"command not found: {e}")
    except Exception as e:
        duration = time.time() - start
        return TestResult(test, False, 1, duration, log_path, reason=str(e))

    duration = time.time() - start
    # Determine pass/fail
    try:
        log_text = log_path.read_text(errors="ignore")
    except Exception:
        log_text = ""

    passed = (exit_code == 0)
    reason = ""

    # Heuristics based on UVM report summary
    if "UVM_FATAL" in log_text:
        passed = False
        reason = "UVM_FATAL found in log"
    elif "UVM_ERROR" in log_text:
        # Try to detect zero error summary
        if re.search(r"UVM_ERROR\s*:\s*0\b", log_text) or re.search(r"\bErrors\s*:\s*0\b", log_text):
            pass
        else:
            passed = False
            reason = "UVM_ERROR found in log"

    return TestResult(test, passed, exit_code, duration, log_path, reason)


def write_junit(results: List[TestResult], junit_path: Path) -> None:
    from xml.sax.saxutils import escape

    total = len(results)
    failures = sum(0 if r.passed else 1 for r in results)
    time_sum = sum(r.duration_s for r in results)

    lines: List[str] = []
    lines.append('<?xml version="1.0" encoding="UTF-8"?>')
    lines.append(f'<testsuite name="uvm" tests="{total}" failures="{failures}" time="{time_sum:.3f}">')
    for r in results:
        case = f'<testcase classname="uvm" name="{escape(r.test_name)}" time="{r.duration_s:.3f}">'
        lines.append(case)
        if not r.passed:
            message = escape(r.reason or f"exit_code={r.exit_code}")
            log_ref = escape(str(r.log_path) if r.log_path else "")
            lines.append(f'<failure message="{message}">{log_ref}</failure>')
        lines.append("</testcase>")
    lines.append("</testsuite>")

    junit_path.parent.mkdir(parents=True, exist_ok=True)
    junit_path.write_text("\n".join(lines))


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Discover and run all UVM tests")
    parser.add_argument("--root", default=str(Path.cwd()), help="Root directory to search for tests (default: CWD)")
    parser.add_argument("--exts", default=".sv,.svh,.v,.vh", help="Comma-separated extensions to scan")
    parser.add_argument("--include", default=None, help="Regex to include test names")
    parser.add_argument("--exclude", default=None, help="Regex to exclude test names")
    parser.add_argument("--tests", action="append", help="Explicit list of test names (comma/space separated). If set, skips discovery.")
    parser.add_argument("--list", action="store_true", help="Only list discovered tests and exit")
    parser.add_argument("--cmd-template", default=os.environ.get("UVM_CMD_TEMPLATE", ""),
                        help="Simulator command template containing {test} or $TEST placeholder")
    parser.add_argument("--jobs", "-j", type=int, default=max(1, os.cpu_count() or 1), help="Parallel jobs")
    parser.add_argument("--timeout", type=int, default=None, help="Per-test timeout in seconds")
    parser.add_argument("--log-dir", default="logs", help="Directory to write per-test logs")
    parser.add_argument("--junit", default=None, help="Write JUnit XML to this file path")
    parser.add_argument("--stop-on-fail", action="store_true", help="Stop running after first failure")
    parser.add_argument("--allow-empty", action="store_true", help="Do not error if no tests discovered")

    args = parser.parse_args(argv)

    root = Path(args.root).resolve()
    exts = tuple(e.strip() for e in args.exts.split(",") if e.strip())
    include_re = re.compile(args.include) if args.include else None
    exclude_re = re.compile(args.exclude) if args.exclude else None
    explicit_tests = parse_test_list_arg(args.tests)

    if explicit_tests is not None:
        tests = sorted(explicit_tests)
    else:
        tests = discover_tests(root, include_re, exclude_re, exts)

    if args.list:
        for t in tests:
            print(t)
        return 0

    if not tests:
        print("No UVM tests found.")
        return 0 if args.allow_empty else 2

    if not args.cmd_template:
        print("Error: --cmd-template is required (or set UVM_CMD_TEMPLATE env var).", file=sys.stderr)
        return 2

    log_dir = Path(args.log_dir)
    results: List[TestResult] = []

    def _run_and_collect(name: str) -> TestResult:
        return run_single_test(name, args.cmd_template, log_dir, args.timeout)

    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, int(args.jobs))) as executor:
        future_to_test = {executor.submit(_run_and_collect, t): t for t in tests}
        for future in concurrent.futures.as_completed(future_to_test):
            result = future.result()
            results.append(result)
            status = "PASS" if result.passed else "FAIL"
            print(f"[{status}] {result.test_name}  ({result.duration_s:.2f}s)  log={result.log_path}")
            if args.stop_on_fail and not result.passed:
                # Cancel remaining
                for f in future_to_test:
                    f.cancel()
                break

    # Sort results in test name order for stable output
    results.sort(key=lambda r: r.test_name)

    total = len(results)
    passed = sum(1 for r in results if r.passed)
    failed = total - passed
    print("\nSummary:")
    print(f"  Total:  {total}")
    print(f"  Passed: {passed}")
    print(f"  Failed: {failed}")

    if args.junit:
        write_junit(results, Path(args.junit))
        print(f"JUnit report written to {args.junit}")

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())

