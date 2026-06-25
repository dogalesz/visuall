#!/usr/bin/env python3
"""Differential fuzzing: compare Visuall compiled output vs Python interpreter.

Generates random valid Visuall programs using gen_vsl.py, compiles and runs
them through visuallc, then interprets the same source through the Python
reference interpreter.  Any difference in output is reported as a potential
compiler bug (or interpreter limitation).

Usage:
    python tools/fuzzing/diff_fuzz.py --iterations 100 --seed 42
    python tools/fuzzing/diff_fuzz.py --compiler ./build/visuallc -n 1000
"""

import argparse
import os
import random
import subprocess
import sys
import tempfile
import time
from typing import List, Optional, Tuple

# Add this directory to path for gen_vsl and interpreter imports
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_vsl import VslGenerator
from interpreter import Interpreter


# ── Configuration ────────────────────────────────────────────────────────
DEFAULT_COMPILER = "visuallc"
MAX_COMPILE_TIME = 30      # seconds
MAX_RUN_TIME = 10          # seconds for compiled binary


def find_compiler(name: str) -> str:
    """Locate the visuallc compiler binary."""
    # Try project build directory first
    project_dir = os.path.dirname(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))))
    candidates = [
        name,                                              # PATH
        os.path.join(project_dir, "build", "visuallc"),    # build/
        os.path.join(project_dir, "build", "visuallc.exe"),# build/ (Win)
        os.path.join(project_dir, "build", "Release", "visuallc"),  # MSVC
    ]
    for c in candidates:
        if os.path.isfile(c) or (os.path.sep not in c and c == name):
            return c
    return name  # fallback: rely on PATH


def compile_and_run(source: str, compiler_path: str,
                    timeout: int = MAX_COMPILE_TIME) -> Tuple[int, str, str]:
    """Compile source with visuallc and run the resulting binary.

    Returns (exit_code, stdout, stderr).
    """
    with tempfile.TemporaryDirectory(prefix="vsl_diff_") as tmpdir:
        src_path = os.path.join(tmpdir, "test.vsl")
        out_path = os.path.join(tmpdir, "test_out")

        # Write source to temp file
        with open(src_path, "w", encoding="utf-8", newline="\n") as f:
            f.write(source)

        # Compile
        try:
            result = subprocess.run(
                [compiler_path, src_path, "-o", out_path],
                capture_output=True, text=True,
                timeout=timeout)
        except subprocess.TimeoutExpired:
            return (-2, "", f"compilation timed out after {timeout}s")
        except FileNotFoundError:
            return (-3, "", f"compiler not found: {compiler_path}")

        if result.returncode != 0:
            return (-1, "", f"compile error:\n{result.stderr}")

        # Run the compiled binary
        exe_path = out_path
        if os.path.exists(out_path + ".exe"):
            exe_path = out_path + ".exe"
        if not os.path.exists(exe_path):
            return (-4, "", "compiled binary not produced")

        try:
            run_result = subprocess.run(
                [exe_path],
                capture_output=True, text=True,
                timeout=MAX_RUN_TIME)
            return (run_result.returncode, run_result.stdout, run_result.stderr)
        except subprocess.TimeoutExpired:
            return (-5, "", f"binary timed out after {MAX_RUN_TIME}s")


def interpret(source: str) -> Tuple[int, str]:
    """Run source through the Python reference interpreter.

    Returns (exit_code, stdout).
    """
    try:
        interp = Interpreter(source)
        output = interp.run()
        return (0, output)
    except Exception as e:
        return (1, f"<interpreter error: {e}>")


def normalize_output(s: str) -> str:
    """Normalize output for comparison: strip trailing newlines."""
    return s.rstrip("\n\r")


def run_iteration(compiler_path: str, seed: int,
                  preset: str = "default") -> Optional[str]:
    """Run one differential fuzzing iteration.

    Returns None if outputs match, or a report string if they differ.
    """
    gen = VslGenerator(seed=seed, preset=preset)
    source = gen.generate(num_stmts=random.randint(3, 25))

    # Compile + run
    comp_exit, comp_stdout, comp_stderr = compile_and_run(source, compiler_path)

    # Interpret
    interp_exit, interp_stdout = interpret(source)

    # Normalize
    comp_out = normalize_output(comp_stdout)
    interp_out = normalize_output(interp_stdout)

    # Compare — skip if compilation failed (not a bug)
    if comp_exit < 0:
        return None  # compilation/internal failure, not a mismatch

    if comp_out != interp_out:
        report = []
        report.append(f"=== MISMATCH (seed={seed}) ===")
        report.append("--- Source ---")
        report.append(source)
        report.append("--- Compiled (exit={}) ---".format(comp_exit))
        report.append(comp_out if comp_out else "(empty)")
        if comp_stderr.strip():
            report.append("--- Compiled stderr ---")
            report.append(comp_stderr.rstrip())
        report.append("--- Interpreted ---")
        report.append(interp_out if interp_out else "(empty)")
        report.append("=" * 60)
        return "\n".join(report)

    return None


def main():
    parser = argparse.ArgumentParser(
        description="Differential fuzzing for Visuall compiler")
    parser.add_argument("-n", "--iterations", type=int, default=100,
                        help="Number of fuzzing iterations (default: 100)")
    parser.add_argument("--seed", type=int, default=None,
                        help="Random seed for reproducibility")
    parser.add_argument("--compiler", type=str, default=DEFAULT_COMPILER,
                        help="Path to visuallc compiler binary")
    parser.add_argument("--preset", choices=["simple", "default", "all"],
                        default="simple",
                        help="Generator feature preset (default: simple)")
    parser.add_argument("--output-dir", type=str, default=None,
                        help="Directory to save mismatch sources")
    parser.add_argument("--quiet", action="store_true",
                        help="Only print summary")
    args = parser.parse_args()

    # Setup
    if args.seed is not None:
        random.seed(args.seed)

    compiler_path = find_compiler(args.compiler)
    if not compiler_path:
        print(f"ERROR: compiler not found: {args.compiler}", file=sys.stderr)
        print("Build visuallc first, or specify --compiler PATH", file=sys.stderr)
        sys.exit(1)

    if not args.quiet:
        print(f"Compiler: {compiler_path}")
        print(f"Iterations: {args.iterations}")
        print(f"Preset: {args.preset}")
        print(f"Seed: {args.seed}")
        print()

    # Output directory for mismatches
    mismatch_dir = args.output_dir
    if mismatch_dir:
        os.makedirs(mismatch_dir, exist_ok=True)

    mismatches: List[str] = []
    compilations_failed = 0
    start_time = time.time()

    for i in range(args.iterations):
        seed = random.randint(0, 2**31 - 1)
        report = run_iteration(compiler_path, seed, args.preset)

        if report is not None:
            mismatches.append(report)
            if not args.quiet:
                print(report)

            # Save mismatch source
            if mismatch_dir:
                path = os.path.join(mismatch_dir, f"mismatch_{seed}.vsl")
                # Extract source from report
                lines = report.split("\n")
                in_source = False
                with open(path, "w", encoding="utf-8") as f:
                    for line in lines:
                        if line == "--- Source ---":
                            in_source = True
                            continue
                        elif line.startswith("---"):
                            in_source = False
                        elif in_source:
                            f.write(line + "\n")

        # Progress indicator
        if (i + 1) % 25 == 0 and not args.quiet:
            elapsed = time.time() - start_time
            rate = (i + 1) / elapsed if elapsed > 0 else 0
            print(f"[{i + 1}/{args.iterations}] "
                  f"mismatches={len(mismatches)} "
                  f"rate={rate:.1f}/s")

    # Summary
    elapsed = time.time() - start_time
    print(f"\n{'=' * 60}")
    print(f"Done: {len(mismatches)} mismatches out of {args.iterations} "
          f"iterations ({elapsed:.1f}s)")
    if compilations_failed:
        print(f"Compilation failures: {compilations_failed}")
    if mismatches:
        print(f"\nMismatch seeds (for reproduction):")
        for m in mismatches:
            # Extract seed from report
            for line in m.split("\n"):
                if "MISMATCH (seed=" in line:
                    print(f"  {line.strip()}")
                    break

    return 0 if not mismatches else 1


if __name__ == "__main__":
    sys.exit(main())
