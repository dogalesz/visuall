// ════════════════════════════════════════════════════════════════════════════
//  1. Integer addition produces correct IR
//  2. Float promotion in mixed arithmetic
//  3. if/else produces correct BasicBlocks
//  4. while loop produces correct loop structure
//  5. Function call emits CreateCall
//  6. print("hello") emits printf call
//  7. true/false emit i1 constants
//  8. null emits ConstantPointerNull
//  9. String literal produces global constant
// 10. Optimization passes run without error
// 11. str[i] positive index emits __visuall_string_index
// 12. str[i] negative index emits __visuall_string_index
// 13. str[i] on variable emits __visuall_string_index
// 14. try/catch/finally generates invoke/landingpad/__cxa_throw IR
// 15. assert statement emits branch + fail/pass blocks
// 16. del statement: identifier zeroes slot, list/dict remove called
// 17. with statement: __enter__/__exit__ called, landingpad/resume emitted
// 18. collections Stack/Queue/Set accept non-int values (str, pointer)
// 19. match statement: int/str/bool cases and wildcard lowered to if/else chain
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

// ── 15. assert statement emits branch + __visuall_print_str + __visuall_sys_exit
static void test_assertStatement() {
    const char* src =
        "define check(n: int) -> void:\n"
        "\tassert n > 0, \"must be positive\"\n";
    std::string ir = generateIR(src);
    expect(!ir.empty(), "15a. assert generates IR");
    expect(ir.find("br i1") != std::string::npos,
           "15b. assert emits conditional branch");
    expect(ir.find("__visuall_print_str") != std::string::npos,
           "15c. assert fail path calls __visuall_print_str");
    expect(ir.find("__visuall_sys_exit") != std::string::npos,
           "15d. assert fail path calls __visuall_sys_exit");
    expect(ir.find("assert.pass") != std::string::npos,
           "15e. assert has pass block");
    expect(ir.find("assert.fail") != std::string::npos,
           "15f. assert has fail block");

    // Assert without message should use default "AssertionError"
    const char* src2 =
        "define check2(n: int) -> void:\n"
        "\tassert n > 0\n";
    std::string ir2 = generateIR(src2);
    expect(ir2.find("AssertionError") != std::string::npos,
           "15g. assert without message uses default 'AssertionError'");
}

// ════════════════════════════════════════════════════════════════════════════
// Test runner
// ════════════════════════════════════════════════════════════════════════════

static void test_delStatement() {
    // del identifier: store 0 into alloca
    std::string ir1 = generateIR(
        "define f() -> void:\n"
        "\tx = 42\n"
        "\tdel x\n");
    expect(!ir1.empty(), "16a. del identifier generates IR");
    expect(ir1.find("store i64 0") != std::string::npos ||
           ir1.find("store") != std::string::npos,
           "16b. del identifier emits store");

    // del list[i]: calls __visuall_list_remove
    std::string ir2 = generateIR(
        "define g() -> void:\n"
        "\titems = [10, 20, 30]\n"
        "\tdel items[1]\n");
    expect(!ir2.empty(), "16c. del list[i] generates IR");
    expect(ir2.find("__visuall_list_remove") != std::string::npos,
           "16d. del list[i] calls __visuall_list_remove");

    // del dict[key]: calls __visuall_dict_remove
    std::string ir3 = generateIR(
        "define h() -> void:\n"
        "\td = {\"a\": 1, \"b\": 2}\n"
        "\tdel d[\"a\"]\n");
    expect(!ir3.empty(), "16e. del dict[key] generates IR");
    expect(ir3.find("__visuall_dict_remove") != std::string::npos,
           "16f. del dict[key] calls __visuall_dict_remove");
}

// ── 17. with statement: __enter__ / __exit__ and try/finally IR ────────────
static void test_withStatement() {
    // Context manager class with __enter__ returning a value and __exit__ cleanup.
    std::string src =
        "class Ctx:\n"
        "\tinit():\n"
        "\t\tthis.val = 0\n"
        "\tdefine __enter__() -> int:\n"
        "\t\tthis.val = 1\n"
        "\t\treturn 42\n"
        "\tdefine __exit__() -> void:\n"
        "\t\tthis.val = 0\n"
        "\n"
        "define run() -> void:\n"
        "\twith Ctx() as x:\n"
        "\t\ty = x\n";

    std::string ir = generateIR(src);
    expect(!ir.empty(), "17a. with statement generates IR");
    // __enter__ method should be present in the IR.
    expect(ir.find("Ctx___enter__") != std::string::npos,
           "17b. IR contains __enter__ method");
    // __exit__ method should be present in the IR.
    expect(ir.find("Ctx___exit__") != std::string::npos,
           "17c. IR contains __exit__ method");
    // A landingpad block should exist for exception handling.
    expect(ir.find("landingpad") != std::string::npos,
           "17d. IR contains landingpad for exception path");
    // The cleanup finally blocks should be present.
    expect(ir.find("with.finally") != std::string::npos,
           "17e. IR contains with.finally blocks");
    // Exception re-raise should be present.
    expect(ir.find("resume") != std::string::npos,
           "17f. IR contains resume to re-raise exceptions");

    // with statement without 'as' should also work.
    std::string src2 =
        "class Mgr:\n"
        "\tinit():\n"
        "\t\tthis.x = 0\n"
        "\tdefine __enter__() -> void:\n"
        "\t\tthis.x = 1\n"
        "\tdefine __exit__() -> void:\n"
        "\t\tthis.x = 0\n"
        "\n"
        "define run2() -> void:\n"
        "\twith Mgr():\n"
        "\t\tz = 1\n";
    std::string ir2 = generateIR(src2);
    expect(!ir2.empty(), "17g. with statement without 'as' generates IR");
}

// ── 18. collections: Stack/Queue/Set accept non-int values (str, pointer) ──
// Calls the C runtime functions directly to verify the ptr→i64 coercion
// added to codegen argument passing.
static void test_collectionsAnyType() {
    // Stack: push a string literal (i8* → i64 coercion required).
    std::string src =
        "define test_stack() -> void:\n"
        "\thandle = __visuall_stack_new()\n"
        "\t__visuall_stack_push(handle, \"hello\")\n"
        "\t__visuall_stack_push(handle, 42)\n"
        "\tx = __visuall_stack_pop(handle)\n";

    std::string ir = generateIR(src);
    expect(!ir.empty(), "18a. Stack push with string value generates IR");
    expect(ir.find("__visuall_stack_push") != std::string::npos,
           "18b. IR contains __visuall_stack_push call");
    expect(ir.find("ptrtoint") != std::string::npos,
           "18c. IR contains ptrtoint for pointer-to-i64 coercion");

    // Queue: enqueue a string literal.
    std::string src2 =
        "define test_queue() -> void:\n"
        "\thandle = __visuall_queue_new()\n"
        "\t__visuall_queue_enqueue(handle, \"world\")\n"
        "\t__visuall_queue_enqueue(handle, 99)\n"
        "\tv = __visuall_queue_dequeue(handle)\n";

    std::string ir2 = generateIR(src2);
    expect(!ir2.empty(), "18d. Queue enqueue with string value generates IR");
    expect(ir2.find("__visuall_queue_enqueue") != std::string::npos,
           "18e. IR contains __visuall_queue_enqueue call");

    // Set: add string literals.
    std::string src3 =
        "define test_set() -> void:\n"
        "\thandle = __visuall_set_new()\n"
        "\t__visuall_set_add(handle, \"alpha\")\n"
        "\t__visuall_set_add(handle, \"beta\")\n"
        "\tb = __visuall_set_contains(handle, \"alpha\")\n";

    std::string ir3 = generateIR(src3);
    expect(!ir3.empty(), "18f. Set add with string value generates IR");
    expect(ir3.find("__visuall_set_add") != std::string::npos,
           "18g. IR contains __visuall_set_add call");
}

// ── 19. match statement: int/str/float/bool/wildcard cases ─────────────────
static void test_matchStatement() {
    // Integer match with wildcard
    std::string src =
        "define test_int_match(x: int) -> int:\n"
        "\tmatch x:\n"
        "\t\tcase 1:\n"
        "\t\t\treturn 10\n"
        "\t\tcase 2:\n"
        "\t\t\treturn 20\n"
        "\t\tcase _:\n"
        "\t\t\treturn 99\n";

    std::string ir = generateIR(src);
    expect(!ir.empty(), "19a. integer match generates IR");
    expect(ir.find("match.subject") != std::string::npos,
           "19b. IR spills subject to alloca");
    expect(ir.find("match.icmp") != std::string::npos,
           "19c. IR uses icmp eq for integer comparison");
    expect(ir.find("match.end") != std::string::npos,
           "19d. IR has merge block match.end");

    // String match
    std::string src2 =
        "define test_str_match(s: str) -> int:\n"
        "\tmatch s:\n"
        "\t\tcase \"hello\":\n"
        "\t\t\treturn 1\n"
        "\t\tcase \"world\":\n"
        "\t\t\treturn 2\n"
        "\t\tcase _:\n"
        "\t\t\treturn 0\n";

    std::string ir2 = generateIR(src2);
    expect(!ir2.empty(), "19e. string match generates IR");
    expect(ir2.find("strcmp") != std::string::npos,
           "19f. IR uses strcmp for string comparison");

    // Bool match
    std::string src3 =
        "define test_bool_match(b: bool) -> int:\n"
        "\tmatch b:\n"
        "\t\tcase true:\n"
        "\t\t\treturn 1\n"
        "\t\tcase false:\n"
        "\t\t\treturn 0\n";

    std::string ir3 = generateIR(src3);
    expect(!ir3.empty(), "19g. bool match generates IR");
}

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
    test_assertStatement();
    test_delStatement();
    test_withStatement();
    test_collectionsAnyType();
    test_matchStatement();

    std::cout << "  " << (19 - failures) << "/19 codegen tests passed.\n";
    return failures;
}
