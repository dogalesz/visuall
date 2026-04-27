// ════════════════════════════════════════════════════════════════════════════
// 10 test cases:
//   1. Integer addition produces correct IR
//   2. Float promotion in mixed arithmetic
//   3. if/else produces correct BasicBlocks
//   4. while loop produces correct loop structure
//   5. Function call emits CreateCall
//   6. print("hello") emits printf call
//   7. true/false emit i1 constants
//   8. null emits ConstantPointerNull
//   9. String literal produces global constant
//  10. Optimization passes run without error
//  11. str[i] positive index emits __visuall_string_index
//  12. str[i] negative index emits __visuall_string_index
//  13. str[i] on variable emits __visuall_string_index
//  14. try/catch/finally generates invoke/landingpad/__cxa_throw IR
// ════════════════════════════════════════════════════════════════════════════

#include "lexer.h"
#include "parser.h"
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

static int failures = 0;

static void expect(bool condition, const std::string& testName) {
    if (!condition) {
        std::cerr << "  FAIL: " << testName << "\n";
        failures++;
    } else {
        std::cout << "  PASS: " << testName << "\n";
    }
}

// Helper: lex + parse + codegen, return the LLVM IR as a string.
// Returns "" on error.
static std::string generateIR(const std::string& src) {
    try {
        Lexer lexer(src, "test.vsl");
        auto tokens = lexer.tokenize();
        Parser parser(tokens, "test.vsl");
        auto program = parser.parse();
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

// ── 1. Integer addition produces correct IR ────────────────────────────────
static void test_intAddition() {
    std::string ir = generateIR("x = 40\ny = x + 2\n");
    expect(!ir.empty(), "1a. Integer addition generates IR");
    // The IR should contain an 'add' instruction.
    expect(ir.find("add") != std::string::npos, "1b. IR contains 'add' instruction");
    // Should contain the constants 40 and 2.
    expect(ir.find("40") != std::string::npos, "1c. IR contains constant 40");
    expect(ir.find("2") != std::string::npos,  "1d. IR contains constant 2");
}

// ── 2. Float promotion in mixed arithmetic ─────────────────────────────────
static void test_floatPromotion() {
    std::string ir = generateIR("x = 1\ny = x + 2.5\n");
    expect(!ir.empty(), "2a. Mixed arithmetic generates IR");
    // Should contain sitofp (int→double promotion) and fadd.
    expect(ir.find("sitofp") != std::string::npos, "2b. IR contains sitofp promotion");
    expect(ir.find("fadd") != std::string::npos,   "2c. IR contains fadd instruction");
}

// ── 3. if/else produces correct BasicBlocks ────────────────────────────────
static void test_ifElseBlocks() {
    std::string src =
        "x = 10\n"
        "if x:\n"
        "\ty = 1\n"
        "else:\n"
        "\ty = 2\n";
    std::string ir = generateIR(src);
    expect(!ir.empty(), "3a. if/else generates IR");
    // Should contain basic block labels for then and else.
    expect(ir.find("if.then") != std::string::npos, "3b. IR has 'if.then' block");
    expect(ir.find("if.else") != std::string::npos, "3c. IR has 'if.else' block");
    expect(ir.find("if.end") != std::string::npos,  "3d. IR has 'if.end' merge block");
    // Should contain conditional branch.
    expect(ir.find("br i1") != std::string::npos, "3e. IR contains conditional branch");
}

// ── 4. while loop produces correct loop structure ──────────────────────────
static void test_whileLoop() {
    std::string src =
        "x = 5\n"
        "while x:\n"
        "\tx = x - 1\n";
    std::string ir = generateIR(src);
    expect(!ir.empty(), "4a. while loop generates IR");
    // Should contain loop structure blocks.
    expect(ir.find("while.cond") != std::string::npos, "4b. IR has 'while.cond' block");
    expect(ir.find("while.body") != std::string::npos, "4c. IR has 'while.body' block");
    expect(ir.find("while.end") != std::string::npos,  "4d. IR has 'while.end' exit block");
}

// ── 5. Function call emits CreateCall ──────────────────────────────────────
static void test_functionCall() {
    std::string src =
        "define add(a: int, b: int) -> int:\n"
        "\treturn a + b\n"
        "\n"
        "result = add(3, 4)\n";
    std::string ir = generateIR(src);
    expect(!ir.empty(), "5a. Function call generates IR");
    // Should contain the function definition.
    expect(ir.find("define") != std::string::npos, "5b. IR defines the function");
    // Should contain a call instruction (possibly with fastcc convention).
    expect(ir.find("call i64 @add") != std::string::npos ||
           ir.find("call fastcc i64 @add") != std::string::npos, "5c. IR emits call to @add");
}

// ── 6. print("hello") emits printf call ────────────────────────────────────
static void test_printBuiltin() {
    std::string ir = generateIR("print(\"hello\")\n");
    expect(!ir.empty(), "6a. print() generates IR");
    // Should declare printf or runtime print as external.
    expect(ir.find("@printf") != std::string::npos ||
           ir.find("@__visuall_print_str") != std::string::npos, "6b. IR declares print function");
    // Should call printf or runtime print.
    expect(ir.find("call i32") != std::string::npos ||
           ir.find("call void") != std::string::npos, "6c. IR calls printf via call i32");
    // The string "hello" should appear in the IR.
    expect(ir.find("hello") != std::string::npos, "6d. IR contains the string 'hello'");
}

// ── 7. true/false emit i1 constants ────────────────────────────────────────
static void test_boolConstants() {
    std::string ir = generateIR("a = true\nb = false\n");
    expect(!ir.empty(), "7a. Bool constants generate IR");
    // Should store i1 true (1) and i1 false (0).
    expect(ir.find("i1 true") != std::string::npos ||
           ir.find("i1 1") != std::string::npos,
           "7b. IR contains i1 true constant");
    expect(ir.find("i1 false") != std::string::npos ||
           ir.find("i1 0") != std::string::npos,
           "7c. IR contains i1 false constant");
}

// ── 8. null emits ConstantPointerNull ──────────────────────────────────────
static void test_nullLiteral() {
    std::string ir = generateIR("x = null\n");
    expect(!ir.empty(), "8a. null literal generates IR");
    // Should contain a null pointer constant.
    expect(ir.find("null") != std::string::npos, "8b. IR contains 'null' pointer constant");
}

// ── 9. String literal produces global constant ─────────────────────────────
static void test_stringLiteral() {
    std::string ir = generateIR("s = \"world\"\n");
    expect(!ir.empty(), "9a. String literal generates IR");
    // Should have a global string constant.
    expect(ir.find("@str") != std::string::npos ||
           ir.find("private unnamed_addr constant") != std::string::npos,
           "9b. IR contains global string constant");
    expect(ir.find("world") != std::string::npos,
           "9c. IR contains the string 'world'");
}

// ── 10. Optimization passes run without error ──────────────────────────────
static void test_optimizationPasses() {
    bool success = true;
    try {
        std::string src =
            "define square(x: int) -> int:\n"
            "\treturn x * x\n"
            "\n"
            "y = square(5)\n";
        Lexer lexer(src, "test.vsl");
        auto tokens = lexer.tokenize();
        Parser parser(tokens, "test.vsl");
        auto program = parser.parse();
        Codegen codegen("opt_test_module");
        codegen.generate(*program);
        codegen.optimize();
        // Verify we can still print IR after optimization.
        std::ostringstream oss;
        codegen.printIR(oss);
        success = !oss.str().empty();
    } catch (const std::exception& e) {
        std::cerr << "  [optimization error] " << e.what() << "\n";
        success = false;
    }
    expect(success, "10. Optimization passes run without error");
}

// ── 11. str[positive] dispatches to __visuall_string_index ────────────────
static void test_stringIndexPositive() {
    // Direct literal index: "hello"[1]
    std::string ir = generateIR("c = \"hello\"[1]\n");
    expect(!ir.empty(), "11a. str literal [positive] generates IR");
    // 'str.char' is the CreateCall hint name for __visuall_string_index results.
    expect(ir.find("str.char") != std::string::npos,
           "11b. IR calls __visuall_string_index for literal[1]");
    // 'idx.get' is the CreateCall hint name for __visuall_list_get results.
    expect(ir.find("idx.get") == std::string::npos,
           "11c. IR does NOT call list_get for string index");
}

// ── 12. str[negative] dispatches to __visuall_string_index ────────────────
static void test_stringIndexNegative() {
    std::string ir = generateIR("c = \"hello\"[-1]\n");
    expect(!ir.empty(), "12a. str literal [negative] generates IR");
    expect(ir.find("str.char") != std::string::npos,
           "12b. IR calls __visuall_string_index for literal[-1]");
}

// ── 13. str variable[i] dispatches to __visuall_string_index ──────────────
static void test_stringIndexVariable() {
    std::string ir = generateIR("s = \"world\"\nc = s[0]\n");
    expect(!ir.empty(), "13a. str variable [0] generates IR");
    expect(ir.find("str.char") != std::string::npos,
           "13b. IR calls __visuall_string_index for s[0]");
    expect(ir.find("idx.get") == std::string::npos,
           "13c. IR does NOT call list_get for string variable index");
}

// ── 14. try/catch/finally uses invoke/landingpad/__cxa_throw ─────────────
static void test_tryCatchFinally() {
    // A function that throws, called inside a try/catch/finally block.
    // The codegen must:
    //   • emit __cxa_throw in the throwing function
    //   • use invoke (not call) for the call inside the try body
    //   • emit a landingpad with catch-all clause
    //   • emit __cxa_begin_catch to retrieve the exception object
    //   • emit finally body on both normal and exception paths
    const char* src =
        "define raiser() -> void:\n"
        "\tthrow \"oops\"\n"
        "try:\n"
        "\traiser()\n"
        "catch Exception as e:\n"
        "\tprint(e)\n"
        "finally:\n"
        "\tprint(\"fin\")\n";
    std::string ir = generateIR(src);
    expect(!ir.empty(), "14a. try/catch/finally generates IR");
    expect(ir.find("invoke") != std::string::npos,
           "14b. raiser() inside try emits invoke");
    expect(ir.find("landingpad") != std::string::npos,
           "14c. IR contains a landingpad block");
    expect(ir.find("__cxa_throw") != std::string::npos,
           "14d. throw emits __cxa_throw");
    expect(ir.find("__cxa_begin_catch") != std::string::npos,
           "14e. catch path calls __cxa_begin_catch");
    // The finally block label should appear in the IR (normal path).
    expect(ir.find("finally.norm") != std::string::npos,
           "14f. IR contains finally.norm block");
}

// ════════════════════════════════════════════════════════════════════════════
// Test runner
// ════════════════════════════════════════════════════════════════════════════

int runCodegenTests() {
    failures = 0;

    test_intAddition();
    test_floatPromotion();
    test_ifElseBlocks();
    test_whileLoop();
    test_functionCall();
    test_printBuiltin();
    test_boolConstants();
    test_nullLiteral();
    test_stringLiteral();
    test_optimizationPasses();
    test_stringIndexPositive();
    test_stringIndexNegative();
    test_stringIndexVariable();
    test_tryCatchFinally();

    std::cout << "  " << (14 - failures) << "/14 codegen tests passed.\n";
    return failures;
}
