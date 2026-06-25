// ════════════════════════════════════════════════════════════════════════════
// visuall-fuzzer-afl — AFL++ harness for Visuall compiler fuzzing
//
// Reads a .vsl source file from stdin (or @@-replaced path), runs the
// lexer + parser pipeline, and exits:
//   0 — successful parse or expected compiler error
//   1 — crash / unhandled exception / internal error
//
// AFL++ uses the exit code to distinguish crashes from normal runs.
//
// Build: cmake -DVISUALL_BUILD_AFL=ON (with afl-clang-fast++ as compiler)
// Run:   afl-fuzz -i corpus/ -o findings/ -- ./visuall-fuzzer-afl @@
// ════════════════════════════════════════════════════════════════════════════

#include "visuall_compile.h"
#include "diagnostic.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#ifdef __AFL_HAVE_MANUAL_CONTROL
#include <unistd.h>
#endif

static std::string readStdin() {
    std::ostringstream ss;
    ss << std::cin.rdbuf();
    return ss.str();
}

static std::string readFile(const char* path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

int main(int argc, char* argv[]) {
    std::string source;

#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT();
#endif

    // Read input: either from a file path (argv[1]) or from stdin.
    if (argc > 1 && argv[1][0] != '@') {
        source = readFile(argv[1]);
    } else {
        source = readStdin();
    }

    if (source.empty()) {
        return 0; // empty input is not a crash
    }

#ifdef __AFL_HAVE_MANUAL_CONTROL
    // Persistent mode: loop on stdin until EOF.
    while (__AFL_LOOP(10000)) {
#endif

    try {
        auto result = visuall::compileFrontend(source, "fuzz.vsl");
        (void)result; // destructor coverage
    } catch (const visuall::Diagnostic&) {
        // Expected compiler error — not a crash.
    } catch (const std::exception& e) {
        // Unexpected C++ exception — report as crash.
        std::fprintf(stderr, "internal error: %s\n", e.what());
        return 1;
    } catch (...) {
        // Unknown exception — definitely a crash.
        std::fprintf(stderr, "internal error: unknown exception\n");
        return 1;
    }

#ifdef __AFL_HAVE_MANUAL_CONTROL
    }
#endif

    return 0;
}
