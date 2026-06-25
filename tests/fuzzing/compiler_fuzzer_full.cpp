// ════════════════════════════════════════════════════════════════════════════
// visuall-fuzzer-full — libFuzzer harness for full frontend pipeline fuzzing
//
// Feeds random bytes through the complete Visuall frontend:
//   lexer → parser → capture analysis → class analysis →
//   escape analysis → type checker
//
// Catches memory errors, undefined behavior, and crashes in the deeper
// analysis and type-checking phases that the shallow fuzzer may miss.
//
// Uses the shared compileFrontend() API (visuall_compile.h) which is also
// used by tests and the CLI — ensuring fuzzer coverage directly translates
// to real-world robustness.
//
// Build: cmake -DVISUALL_BUILD_FUZZER=ON (requires Clang)
// Run:   ./visuall-fuzzer-full -max_total_time=60 corpus/
// ════════════════════════════════════════════════════════════════════════════

#include "visuall_compile.h"
#include "diagnostic.h"
#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::string source(reinterpret_cast<const char*>(data), size);

    try {
        // ── Full frontend pipeline ─────────────────────────────────────
        auto result = visuall::compileFrontend(source, "fuzz.vsl");

        // Result destructs cleanly — ASan verifies no heap corruption
        // in Program, ClassType, FuncType, SymbolTable, and all the
        // analysis data structures (capture maps, class fields, escape info).

    } catch (const visuall::Diagnostic&) {
        // Expected compiler errors — not a crash.
    }
    // DO NOT catch std::exception or (...) — let libFuzzer see real crashes.

    return 0;
}
