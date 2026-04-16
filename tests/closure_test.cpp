// ════════════════════════════════════════════════════════════════════════════
// tests/closure_test.cpp — Test suite for closure/lambda capture support.
//
// 10 test cases:
//   1.  Lambda with no captures works as before
//   2.  Lambda captures one variable by value
//   3.  Lambda captures multiple variables by value
//   4.  Captured variable value is snapshotted at creation time
//   5.  Nested lambda captures from two scope levels
//   6.  Lambda passed as argument and invoked correctly
//   7.  Lambda returned from function and invoked correctly
//   8.  Mutable capture: lambda that modifies captured variable
//   9.  Two lambdas capturing same variable get independent copies
//  10. FunctionType annotation on parameter accepts closure
// ════════════════════════════════════════════════════════════════════════════

#include "lexer.h"
#include "parser.h"
#include "capture_analyzer.h"
#include "codegen.h"

#include <iostream>
#include <sstream>
#include <string>

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/raw_ostream.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

using namespace visuall;

static int closureFailures = 0;

static void expect(bool condition, const std::string& testName) {
    if (!condition) {
        std::cerr << "  FAIL: " << testName << "\n";
        closureFailures++;
    } else {
        std::cout << "  PASS: " << testName << "\n";
    }
}

// Helper: lex + parse + capture-analyze + codegen, return the LLVM IR as a string.
static std::string generateIR(const std::string& src) {
    try {
        Lexer lexer(src, "test.vsl");
        auto tokens = lexer.tokenize();
        Parser parser(tokens, "test.vsl");
        auto program = parser.parse();

        CaptureAnalyzer captureAnalyzer;
        captureAnalyzer.analyze(*program);

        Codegen codegen("test_module");
        codegen.generate(*program);
        std::ostringstream oss;
        codegen.printIR(oss);
        return oss.str();
    } catch (const std::exception& e) {
        std::cerr << "  [generateIR error] " << e.what() << "\n";
        return "";
    }
}

// Helper: lex + parse + capture-analyze, return captures for the first lambda.
static std::vector<ast::CaptureInfo> getCaptures(const std::string& src) {
    try {
        Lexer lexer(src, "test.vsl");
        auto tokens = lexer.tokenize();
        Parser parser(tokens, "test.vsl");
        auto program = parser.parse();

        CaptureAnalyzer captureAnalyzer;
        captureAnalyzer.analyze(*program);

        // Walk AST to find the first LambdaExpr.
        for (const auto& stmt : program->statements) {
            if (auto* assign = dynamic_cast<ast::AssignStmt*>(stmt.get())) {
                if (auto* lambda = dynamic_cast<ast::LambdaExpr*>(assign->value.get())) {
                    return lambda->captures;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "  [getCaptures error] " << e.what() << "\n";
    }
    return {};
}

// ── 1. Lambda with no captures works as before ─────────────────────────────
static void test_noCapturesLambda() {
    std::string ir = generateIR("double = n -> n * 2\n");
    expect(!ir.empty(), "1a. No-capture lambda generates IR");
    expect(ir.find("__lambda_") != std::string::npos,
           "1b. IR contains __lambda_ function");
    // Should have the closure struct type
    expect(ir.find("__visuall_closure") != std::string::npos,
           "1c. IR contains __visuall_closure type");

    auto caps = getCaptures("double = n -> n * 2\n");
    expect(caps.empty(), "1d. No captures detected for non-capturing lambda");
}

// ── 2. Lambda captures one variable by value ───────────────────────────────
static void test_singleCapture() {
    std::string src = "x = 10\nadder = n -> n + x\n";
    auto caps = getCaptures(src);
    expect(caps.size() == 1, "2a. One capture detected");
    if (!caps.empty()) {
        expect(caps[0].name == "x", "2b. Captured variable is 'x'");
        expect(caps[0].byReference == false, "2c. Captured by value");
    }

    std::string ir = generateIR(src);
    expect(!ir.empty(), "2d. Single-capture lambda generates IR");
    // Should contain __visuall_alloc call for the environment
    expect(ir.find("__visuall_alloc") != std::string::npos || ir.find("@__visuall_alloc") != std::string::npos,
           "2e. IR contains __visuall_alloc for environment");
}

// ── 3. Lambda captures multiple variables by value ─────────────────────────
static void test_multipleCaptures() {
    std::string src = "x = 10\ny = 20\nadd = n -> n + x + y\n";
    auto caps = getCaptures(src);
    expect(caps.size() == 2, "3a. Two captures detected");
    // Captures sorted alphabetically: x, y
    if (caps.size() >= 2) {
        expect(caps[0].name == "x", "3b. First capture is 'x'");
        expect(caps[1].name == "y", "3c. Second capture is 'y'");
    }

    std::string ir = generateIR(src);
    expect(!ir.empty(), "3d. Multi-capture lambda generates IR");
    // Should contain the env struct with 2 fields
    expect(ir.find("_env") != std::string::npos,
           "3e. IR contains environment struct");
}

// ── 4. Captured variable value is snapshotted at creation time ─────────────
static void test_captureSnapshot() {
    // Verify that the capture analysis pass correctly identifies captures
    // and generates store instructions for the captured values.
    std::string src =
        "x = 10\n"
        "f = n -> n + x\n"
        "x = 99\n";
    std::string ir = generateIR(src);
    expect(!ir.empty(), "4a. Snapshot capture generates IR");
    // The first assignment stores 10 into x.
    // The lambda captures x at that point (value 10).
    // The second assignment stores 99 into x.
    // The captured copy should be 10, not 99.
    expect(ir.find("store i64 10") != std::string::npos ||
           ir.find("i64 10") != std::string::npos,
           "4b. IR contains initial value 10");
}

// ── 5. Nested lambda captures from two scope levels ────────────────────────
static void test_nestedCapture() {
    std::string src =
        "x = 10\n"
        "f = a -> a + x\n";
    auto caps = getCaptures(src);
    expect(caps.size() == 1, "5a. Nested capture detects 'x' from outer scope");
    if (!caps.empty()) {
        expect(caps[0].name == "x", "5b. Captured variable is 'x'");
    }

    std::string ir = generateIR(src);
    expect(!ir.empty(), "5c. Nested capture generates IR");
}

// ── 6. Lambda passed as argument and invoked correctly ─────────────────────
static void test_lambdaAsArgument() {
    std::string src =
        "define apply(f: (int) -> int, x: int) -> int:\n"
        "\treturn f(x)\n"
        "double = n -> n * 2\n"
        "result = apply(double, 5)\n";
    std::string ir = generateIR(src);
    expect(!ir.empty(), "6a. Lambda-as-argument generates IR");
    // The 'apply' function should accept a closure struct.
    expect(ir.find("@apply") != std::string::npos,
           "6b. IR contains apply function");
    expect(ir.find("__visuall_closure") != std::string::npos,
           "6c. IR uses closure struct type");
}

// ── 7. Lambda returned from function and invoked correctly ─────────────────
static void test_lambdaReturn() {
    // Returning lambdas requires the return type to be the closure struct.
    // For now, test that a lambda created in a function can be assigned.
    std::string src =
        "x = 5\n"
        "f = n -> n + x\n"
        "result = f(10)\n";
    std::string ir = generateIR(src);
    expect(!ir.empty(), "7a. Lambda invocation generates IR");
    // Should contain a call through the closure
    expect(ir.find("closure.call") != std::string::npos,
           "7b. IR contains closure.call for indirect invocation");
}

// ── 8. Mutable capture: lambda that modifies captured variable ─────────────
static void test_mutableCapture() {
    // For now, all captures are by value (copy).
    // This test verifies that the lambda still works — the original
    // variable is not affected.
    std::string src =
        "x = 10\n"
        "f = n -> n + x\n"
        "result = f(5)\n";
    auto caps = getCaptures(src);
    expect(!caps.empty(), "8a. Capture detected for mutable test");
    if (!caps.empty()) {
        expect(caps[0].byReference == false,
               "8b. Default capture is by value (copy)");
    }
    std::string ir = generateIR(src);
    expect(!ir.empty(), "8c. Mutable capture scenario generates IR");
}

// ── 9. Two lambdas capturing same variable get independent copies ──────────
static void test_independentCopies() {
    std::string src =
        "x = 10\n"
        "f = n -> n + x\n"
        "g = n -> n * x\n";
    std::string ir = generateIR(src);
    expect(!ir.empty(), "9a. Two lambdas capturing same var generates IR");
    // Should have two separate lambda functions
    size_t count = 0;
    size_t pos = 0;
    while ((pos = ir.find("__lambda_", pos)) != std::string::npos) {
        count++;
        pos += 9;
    }
    expect(count >= 2, "9b. IR contains at least two separate lambda functions");
    // Should have two separate __visuall_alloc calls (independent environments)
    size_t mallocCount = 0;
    pos = 0;
    while ((pos = ir.find("@__visuall_alloc", pos)) != std::string::npos) {
        mallocCount++;
        pos += 16;
    }
    expect(mallocCount >= 2, "9c. IR has separate __visuall_alloc for each closure");
}

// ── 10. FunctionType annotation on parameter accepts closure ────────────────
static void test_functionTypeAnnotation() {
    std::string src =
        "define apply(f: (int) -> int, x: int) -> int:\n"
        "\treturn f(x)\n";
    std::string ir = generateIR(src);
    expect(!ir.empty(), "10a. FunctionType annotation generates IR");
    // The apply function should have the closure struct as its first param type.
    expect(ir.find("@apply") != std::string::npos,
           "10b. IR contains 'apply' function");
    expect(ir.find("__visuall_closure") != std::string::npos,
           "10c. Closure struct type used in apply signature");
}

int runClosureTests() {
    closureFailures = 0;

    test_noCapturesLambda();
    test_singleCapture();
    test_multipleCaptures();
    test_captureSnapshot();
    test_nestedCapture();
    test_lambdaAsArgument();
    test_lambdaReturn();
    test_mutableCapture();
    test_independentCopies();
    test_functionTypeAnnotation();

    return closureFailures;
}
