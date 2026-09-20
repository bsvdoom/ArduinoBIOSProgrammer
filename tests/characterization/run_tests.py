#!/usr/bin/env python3
"""Build and run the native tests against the production xmodem.cpp."""

from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


TESTS = (
    ("send_short_byte_order", "pass"),
    ("send_00_ff", "pass"),
    ("send_long_pattern", "pass"),
    ("exact_128_boundary", "pass"),
    ("multiple_blocks", "pass"),
    ("block_number_rollover", "pass"),
    ("start_and_ack_handling", "pass"),
    ("eot_handling", "pass"),
    ("can_handling", "pass"),
    ("timeout", "pass"),
    ("duplicate_receive_block", "pass"),
    ("nak_retry_identical_and_in_bounds", "pass"),
)


def compile_binary(compiler: str, output: Path, sanitizers: bool) -> subprocess.CompletedProcess[str]:
    root = Path(__file__).resolve().parents[2]
    command = [
        compiler,
        "-std=c++17",
        "-O1",
        "-g",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-I",
        str(root / "tests/characterization/stubs"),
        "-I",
        str(root / "src"),
    ]
    if sanitizers:
        command.extend(
            ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        )
    command.extend(
        [
            str(root / "src/xmodem.cpp"),
            str(root / "tests/characterization/xmodem_characterization.cpp"),
            "-o",
            str(output),
        ]
    )
    return subprocess.run(command, capture_output=True, text=True, check=False)


def failure_summary(stderr: str) -> str:
    if "AddressSanitizer: heap-buffer-overflow" in stderr:
        return "AddressSanitizer heap-buffer-overflow"
    if "AddressSanitizer" in stderr:
        return "AddressSanitizer failure"
    if "runtime error:" in stderr:
        line = next(
            (line.strip() for line in stderr.splitlines() if "runtime error:" in line),
            "UndefinedBehaviorSanitizer failure",
        )
        return line
    line = next((line.strip() for line in stderr.splitlines() if line.strip()), "test failed")
    return line


def main() -> int:
    compiler = shutil.which("g++")
    if compiler is None:
        print("ERROR g++ is not available", file=sys.stderr)
        return 2

    with tempfile.TemporaryDirectory(prefix="xmodem-characterization-") as build_dir:
        binary = Path(build_dir) / "xmodem_characterization"
        sanitizers = True
        build = compile_binary(compiler, binary, sanitizers=True)
        if build.returncode != 0:
            sanitizers = False
            build = compile_binary(compiler, binary, sanitizers=False)
        if build.returncode != 0:
            print("ERROR native test build failed", file=sys.stderr)
            print(build.stderr.rstrip(), file=sys.stderr)
            return 2

        print(f"BUILD compiler={compiler}")
        print(
            "SANITIZERS address,undefined enabled"
            if sanitizers
            else "SANITIZERS unavailable; unsanitized fallback"
        )

        environment = os.environ.copy()
        environment["ASAN_OPTIONS"] = "detect_leaks=0:halt_on_error=1"
        environment["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
        passed = 0
        known_failures = 0
        unexpected_failures = 0

        for name, expected in TESTS:
            result = subprocess.run(
                [str(binary), name],
                capture_output=True,
                text=True,
                check=False,
                timeout=30,
                env=environment,
            )
            if result.returncode == 0:
                print(result.stdout.strip() or f"PASS {name}")
                passed += 1
            elif expected == "known_failure":
                print(f"KNOWN FAILURE {name}: {failure_summary(result.stderr)}")
                known_failures += 1
            else:
                print(f"UNEXPECTED FAILURE {name}: {failure_summary(result.stderr)}")
                unexpected_failures += 1

        print(
            f"SUMMARY PASS={passed} KNOWN_FAILURE={known_failures} "
            f"UNEXPECTED_FAILURE={unexpected_failures}"
        )
        return 1 if unexpected_failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
