// ════════════════════════════════════════════════════════════════════════════
// tests/diagnostic_test.cpp — Tests for Diagnostic error message formatting.
//
// Verifies that compiler errors:
//   1. Use clang-style  filename:line:col: error: message  format
//   2. Use Visuall type syntax (int, str, list[int]) not internal C++ names
//   3. Produce a caret line pointing at the offending column
//   4. Include hints for well-known situations
//
// Test cases:
//   1. Undefined variable message format + hint
//   2. Type mismatch message format + hint
//   3. Missing return message format + hint
//   4. Import not found message format
// ════════════════════════════════════════════════════════════════════════════

#include "diagnostic.h"
#include "lexer.h"
#include "parser.h"
#include "typechecker.h"
#include "module_loader.h"

#include <iostream>
#include <string>

using namespace visuall;

static int failures = 0;

static void expect(bool condition, const std::string& testName) {
    if (!condition) {
        std::cerr << "  FAIL: " << testName << "\n";
        failures++;
    } else {
        std::cout << "  PASS: " << testName << "\n";
    }
}

// Helper: lex + parse + typecheck; return empty string on success, what() on failure.
static std::string typecheck(const std::string& src,
                              const std::string& filename = "test.vsl") {
    try {
        Lexer lexer(src, filename);
        auto tokens = lexer.tokenize();
        Parser parser(tokens, filename);
        auto program = parser.parse();
        TypeChecker checker(filename);
        checker.check(*program);
        return "";
    } catch (const Diagnostic& d) {
        return d.format();
    } catch (const std::exception& e) {
        return e.what();
    }
}

// ── 1. Undefined variable ─────────────────────────────────────────────────
static void test_undefinedVariable() {
    // 'x' is not declared
    std::string src = "y = x + 1\n";
    std::string msg = typecheck(src, "foo.vsl");

    // Must contain filename:line:col: error:
    expect(msg.find("foo.vsl:1:5:") != std::string::npos ||
           msg.find("foo.vsl:1:") != std::string::npos,
           "1a. Undefined-var: includes filename and line");
    expect(msg.find("error:") != std::string::npos,
           "1b. Undefined-var: severity is 'error'");
    expect(msg.find("Undefined variable 'x'") != std::string::npos,
           "1c. Undefined-var: names the missing variable");

    // Type names must not be internal enum names like Type::Kind::Int
    expect(msg.find("Type::Kind") == std::string::npos,
           "1d. Undefined-var: no internal type names");
}

// ── 1e. Undefined variable near a similar name gets a suggestion ──────────
static void test_undefinedVariableSimilarName() {
    // 'coun' is close to 'count' (distance 1)
    std::string src = "count = 10\ny = coun + 1\n";
    std::string msg = typecheck(src, "bar.vsl");

    expect(!msg.empty(), "1e. Undefined similar-name: error produced");
    expect(msg.find("coun") != std::string::npos,
           "1f. Undefined similar-name: mentions misspelled name");
    // Hint should suggest 'count'
    expect(msg.find("count") != std::string::npos,
           "1g. Undefined similar-name: hint mentions 'count'");
    expect(msg.find("did you mean") != std::string::npos,
           "1h. Undefined similar-name: contains 'did you mean'");
}

// ── 2. Type mismatch ──────────────────────────────────────────────────────
static void test_typeMismatch() {
    // Assign str to an int variable
    std::string src = "x = 42\nx = \"hello\"\n";
    std::string msg = typecheck(src, "mismatch.vsl");

    expect(!msg.empty(), "2a. Type-mismatch: error produced");
    expect(msg.find("mismatch.vsl:") != std::string::npos,
           "2b. Type-mismatch: includes filename");
    expect(msg.find("error:") != std::string::npos,
           "2c. Type-mismatch: severity is 'error'");
    // Must use Visuall type names, not internal names
    expect(msg.find("str") != std::string::npos,
           "2d. Type-mismatch: mentions 'str' (Visuall syntax)");
    expect(msg.find("int") != std::string::npos,
           "2e. Type-mismatch: mentions 'int' (Visuall syntax)");
    expect(msg.find("Type::") == std::string::npos,
           "2f. Type-mismatch: no internal 'Type::' names");
    // Should hint about int(x) conversion
    expect(msg.find("int(x)") != std::string::npos,
           "2g. Type-mismatch: hint suggests int(x)");
}

// ── 3. Missing return ─────────────────────────────────────────────────────
static void test_missingReturn() {
    std::string src =
        "define compute(a: int) -> int:\n"
        "\tx = a + 1\n";
    std::string msg = typecheck(src, "missing_ret.vsl");

    expect(!msg.empty(), "3a. Missing-return: error produced");
    expect(msg.find("missing_ret.vsl:") != std::string::npos,
           "3b. Missing-return: includes filename");
    expect(msg.find("error:") != std::string::npos,
           "3c. Missing-return: severity is 'error'");
    expect(msg.find("missing return") != std::string::npos ||
           msg.find("return") != std::string::npos,
           "3d. Missing-return: message mentions 'return'");
    // Type name must be Visuall 'int', not internal
    expect(msg.find("int") != std::string::npos,
           "3e. Missing-return: type shown as 'int'");
    expect(msg.find("Type::Kind") == std::string::npos,
           "3f. Missing-return: no internal type names");
    // Hint should say to add return
    expect(msg.find("hint:") != std::string::npos,
           "3g. Missing-return: contains a hint");
}

// ── 4. Import not found ───────────────────────────────────────────────────
static void test_importNotFound() {
    ModuleLoader loader("stdlib_nonexistent", {}, false);
    std::string caught;
    try {
        loader.load("nonexistent_module_xyz", "test.vsl");
    } catch (const Diagnostic& d) {
        caught = d.format();
    } catch (const std::exception& e) {
        caught = e.what();
    }

    expect(!caught.empty(), "4a. Import-not-found: error produced");
    expect(caught.find("nonexistent_module_xyz") != std::string::npos,
           "4b. Import-not-found: names the missing module");
    // Should not expose raw C++ exception text
    expect(caught.find("std::") == std::string::npos,
           "4c. Import-not-found: no raw C++ exception text");
    expect(caught.find("error:") != std::string::npos ||
           caught.find("ImportError") != std::string::npos ||
           caught.find("Could not find") != std::string::npos ||
           caught.find("not found") != std::string::npos ||
           caught.find("import") != std::string::npos,
           "4d. Import-not-found: message is descriptive");
}

// ── 5. Diagnostic format — caret line ─────────────────────────────────────
static void test_diagnosticCaretFormat() {
    // Build a Diagnostic directly and verify formatting.
    Diagnostic diag(Diagnostic::Severity::Error,
                    "test error", "try something else",
                    "file.vsl", 3, 5,
                    "    hello world");
    std::string fmt = diag.format();

    expect(fmt.find("file.vsl:3:5:") != std::string::npos,
           "5a. Diagnostic format: filename:line:col");
    expect(fmt.find("error: test error") != std::string::npos,
           "5b. Diagnostic format: error message");
    expect(fmt.find("hello world") != std::string::npos,
           "5c. Diagnostic format: source line included");
    // Caret at col 5 means 4 spaces then '^'
    expect(fmt.find("    ^") != std::string::npos,
           "5d. Diagnostic format: caret at correct column");
    expect(fmt.find("hint: try something else") != std::string::npos,
           "5e. Diagnostic format: hint line included");
}

// ════════════════════════════════════════════════════════════════════════════
// Entry point
// ════════════════════════════════════════════════════════════════════════════
int runDiagnosticTests() {
    failures = 0;
    test_undefinedVariable();
    test_undefinedVariableSimilarName();
    test_typeMismatch();
    test_missingReturn();
    test_importNotFound();
    test_diagnosticCaretFormat();
    return failures;
}
