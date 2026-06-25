// ════════════════════════════════════════════════════════════════════════════
// visuall-fuzzer-frontend — libFuzzer harness for lexer + parser fuzzing
//
// Feeds random bytes through the Visuall lexer and parser, catching:
//   • Memory errors (via ASan): buffer overflows, use-after-free, leaks
//   • Undefined behavior (via UBSan): integer overflow, null deref, etc.
//   • Unhandled C++ exceptions / assertion failures / segfaults
//
// Expected compiler errors (LexError, ParseError inheriting Diagnostic) are
// caught and ignored — libFuzzer treats them as non-crashing inputs.
//
// Build: cmake -DVISUALL_BUILD_FUZZER=ON (requires Clang)
// Run:   ./visuall-fuzzer-frontend -max_total_time=60 corpus/
// ════════════════════════════════════════════════════════════════════════════

#include "lexer.h"
#include "parser.h"
#include "diagnostic.h"
#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // libFuzzer input is NOT null-terminated — construct a std::string.
    std::string source(reinterpret_cast<const char*>(data), size);

    try {
        // ── Lexer ──────────────────────────────────────────────────────
        visuall::Lexer lexer(source, "fuzz.vsl");
        auto tokens = lexer.tokenize();

        // ── Parser ─────────────────────────────────────────────────────
        visuall::Parser parser(tokens, "fuzz.vsl");
        auto program = parser.parse();

        // Successfully lexed and parsed — program destructs cleanly here.
        // ASan + UBSan will catch any heap corruption in the destructors.

    } catch (const visuall::Diagnostic&) {
        // Expected compiler errors (LexError, ParseError, IndentationError).
        // These are legitimate outcomes for malformed input — not crashes.
    }
    // IMPORTANT: Do NOT catch std::exception or (...).
    // A true crash (segfault, __assert_fail, unhandled std::exception)
    // must terminate the process so libFuzzer records it as a failure.

    return 0; // non-zero return tells libFuzzer this input is "interesting"
}
