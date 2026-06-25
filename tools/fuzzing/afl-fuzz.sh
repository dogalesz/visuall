#!/usr/bin/env bash
# ══════════════════════════════════════════════════════════════════════════════
# AFL++ fuzzing wrapper for Visuall compiler
#
# Builds the AFL++ instrumented fuzzer and launches afl-fuzz with the
# Visuall keyword dictionary and seed corpus.
#
# Prerequisites:
#   - AFL++ installed (https://github.com/AFLplusplus/AFLplusplus)
#   - LLVM 17+ installed
#
# Usage:
#   ./tools/fuzzing/afl-fuzz.sh              # default settings
#   ./tools/fuzzing/afl-fuzz.sh -V 120       # run for 120 seconds
#   ./tools/fuzzing/afl-fuzz.sh -- -t 10000  # pass extra args to afl-fuzz
# ══════════════════════════════════════════════════════════════════════════════

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
CORPUS_DIR="${PROJECT_DIR}/tests/fuzzing/corpus"
ARTIFACTS_DIR="${PROJECT_DIR}/tests/fuzzing/artifacts"
FINDINGS_DIR="${PROJECT_DIR}/tests/fuzzing/findings"
DICT="${SCRIPT_DIR}/vsl_dict.txt"

# Ensure corpus and output directories exist
mkdir -p "${ARTIFACTS_DIR}" "${FINDINGS_DIR}"

# Build the AFL++ instrumented fuzzer if not already built
if [ ! -f "${BUILD_DIR}/visuall-fuzzer-afl" ]; then
    echo "==> Building AFL++-instrumented fuzzer..."

    # Try afl-clang-fast++ first (better performance), fall back to afl-g++
    if command -v afl-clang-fast++ &>/dev/null; then
        CXX_COMPILER="afl-clang-fast++"
        CC_COMPILER="afl-clang-fast"
    elif command -v afl-g++ &>/dev/null; then
        CXX_COMPILER="afl-g++"
        CC_COMPILER="afl-gcc"
    else
        echo "ERROR: Neither afl-clang-fast++ nor afl-g++ found in PATH."
        echo "Install AFL++: https://github.com/AFLplusplus/AFLplusplus"
        exit 1
    fi

    echo "==> Using compiler: ${CXX_COMPILER}"

    cmake -B "${BUILD_DIR}" -G Ninja \
        -DCMAKE_C_COMPILER="${CC_COMPILER}" \
        -DCMAKE_CXX_COMPILER="${CXX_COMPILER}" \
        -DVISUALL_BUILD_AFL=ON \
        -DVISUALL_BUILD_FUZZER=OFF \
        -DBUILD_LSP=OFF \
        -DLLVM_DIR="${LLVM_DIR:-/usr/lib/llvm-17/lib/cmake/llvm}"

    cmake --build "${BUILD_DIR}" --target visuall-fuzzer-afl --parallel
    echo "==> Build complete."
fi

# Launch afl-fuzz
echo "==> Starting afl-fuzz..."
echo "    Corpus:     ${CORPUS_DIR}"
echo "    Findings:   ${FINDINGS_DIR}"
echo "    Dictionary: ${DICT}"

exec afl-fuzz \
    -i "${CORPUS_DIR}" \
    -o "${FINDINGS_DIR}" \
    -x "${DICT}" \
    -t 5000 \
    "$@" \
    -- "${BUILD_DIR}/visuall-fuzzer-afl"
