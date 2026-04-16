// ════════════════════════════════════════════════════════════════════════════
// tests/syntax_test.cpp — Test suite for new syntax features.
//
// 14 test cases covering:
//   1. Default parameter values
//   2. Default parameter with type annotation
//   3. Slice notation (start:stop)
//   4. Slice notation (start:stop:step)
//   5. List comprehension
//   6. List comprehension with filter
//   7. Dict comprehension
//   8. Tuple unpacking / destructuring
//   9. Variadic function (*args)
//  10. Decorator on function
//  11. Multiple decorators
//  12. Spread expression (*list)
//  13. Empty slice ([:])
//  14. Dict comprehension with filter
// ════════════════════════════════════════════════════════════════════════════

#include "lexer.h"
#include "parser.h"
#include "typechecker.h"
#include "codegen.h"
#include "ast_printer.h"
#include <iostream>
#include <sstream>
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

// Helper: lex + parse, return the AST as a string.
static std::string parseToAST(const std::string& src) {
    try {
        Lexer lexer(src, "test.vsl");
        auto tokens = lexer.tokenize();
        Parser parser(tokens, "test.vsl");
        auto program = parser.parse();
        std::ostringstream oss;
        AstPrinter::print(*program, oss);
        return oss.str();
    } catch (const std::exception& e) {
        return std::string("ERROR: ") + e.what();
    }
}

// Helper: lex + parse + typecheck, return true if no errors.
static bool typecheckOK(const std::string& src) {
    try {
        Lexer lexer(src, "test.vsl");
        auto tokens = lexer.tokenize();
        Parser parser(tokens, "test.vsl");
        auto program = parser.parse();
        TypeChecker tc("test.vsl");
        tc.check(*program);
        return true;
    } catch (...) {
        return false;
    }
}

// Helper: lex + parse + codegen, return IR.
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
        return std::string("ERROR: ") + e.what();
    }
}

// ── Test 1: Default parameter values ──────────────────────────────────────
static void test_default_params() {
    std::string src = "define greet(name = \"world\"):\n\tprint(name)\n";
    auto ast = parseToAST(src);
    expect(ast.find("FunctionDef: greet") != std::string::npos,
           "Default param: parses function");
    expect(ast.find("[default]") != std::string::npos,
           "Default param: default marker present");
}

// ── Test 2: Default param with type annotation ────────────────────────────
static void test_default_param_with_type() {
    std::string src = "define add(x: int, y: int = 0):\n\treturn x + y\n";
    auto ast = parseToAST(src);
    expect(ast.find("Param: x (int)") != std::string::npos,
           "Default param + type: x has type annotation");
    expect(ast.find("Param: y (int) [default]") != std::string::npos,
           "Default param + type: y has type and default");
    expect(typecheckOK(src), "Default param + type: typechecks OK");
}

// ── Test 3: Slice notation (start:stop) ───────────────────────────────────
static void test_slice_start_stop() {
    std::string src = "x = items[1:3]\n";
    auto ast = parseToAST(src);
    expect(ast.find("Slice") != std::string::npos,
           "Slice start:stop: AST contains Slice node");
}

// ── Test 4: Slice notation (start:stop:step) ─────────────────────────────
static void test_slice_with_step() {
    std::string src = "x = items[0:10:2]\n";
    auto ast = parseToAST(src);
    expect(ast.find("Slice") != std::string::npos,
           "Slice start:stop:step: AST contains Slice node");
}

// ── Test 5: List comprehension ────────────────────────────────────────────
static void test_list_comprehension() {
    std::string src = "squares = [x ** 2 for x in range]\n";
    auto ast = parseToAST(src);
    expect(ast.find("ListComp") != std::string::npos,
           "List comprehension: AST contains ListComp");
}

// ── Test 6: List comprehension with filter ────────────────────────────────
static void test_list_comprehension_filter() {
    std::string src = "evens = [x for x in nums if x % 2 == 0]\n";
    auto ast = parseToAST(src);
    expect(ast.find("ListComp") != std::string::npos,
           "List comprehension filter: AST contains ListComp");
}

// ── Test 7: Dict comprehension ────────────────────────────────────────────
static void test_dict_comprehension() {
    std::string src = "d = {k: k * 2 for k in items}\n";
    auto ast = parseToAST(src);
    expect(ast.find("DictComp") != std::string::npos,
           "Dict comprehension: AST contains DictComp");
}

// ── Test 8: Tuple unpacking ──────────────────────────────────────────────
static void test_tuple_unpacking() {
    std::string src = "a, b, c = (1, 2, 3)\n";
    auto ast = parseToAST(src);
    expect(ast.find("TupleUnpack") != std::string::npos,
           "Tuple unpacking: AST contains TupleUnpack");
    expect(ast.find("a, b, c") != std::string::npos,
           "Tuple unpacking: targets are a, b, c");
}

// ── Test 9: Variadic function ─────────────────────────────────────────────
static void test_variadic_function() {
    std::string src = "define sum_all(*args):\n\treturn 0\n";
    auto ast = parseToAST(src);
    expect(ast.find("FunctionDef: sum_all") != std::string::npos,
           "Variadic: parses function");
    expect(ast.find("[variadic]") != std::string::npos,
           "Variadic: variadic marker present");
}

// ── Test 10: Decorator on function ────────────────────────────────────────
static void test_decorator() {
    std::string src = "@log\ndefine hello():\n\tprint(\"hi\")\n";
    auto ast = parseToAST(src);
    expect(ast.find("FunctionDef: hello") != std::string::npos,
           "Decorator: function parsed");
    expect(ast.find("Decorator") != std::string::npos,
           "Decorator: decorator node present");
}

// ── Test 11: Multiple decorators ──────────────────────────────────────────
static void test_multiple_decorators() {
    std::string src = "@cache\n@log\ndefine compute(x):\n\treturn x\n";
    auto ast = parseToAST(src);
    // Should have two decorator nodes
    size_t first = ast.find("Decorator");
    expect(first != std::string::npos, "Multi-decorator: first found");
    if (first != std::string::npos) {
        size_t second = ast.find("Decorator", first + 1);
        expect(second != std::string::npos, "Multi-decorator: second found");
    }
}

// ── Test 12: Spread expression ────────────────────────────────────────────
static void test_spread_expression() {
    std::string src = "x = *items\n";
    auto ast = parseToAST(src);
    expect(ast.find("Spread") != std::string::npos ||
           ast.find("*items") != std::string::npos,
           "Spread: AST contains spread node");
}

// ── Test 13: Empty slice ──────────────────────────────────────────────────
static void test_empty_slice() {
    std::string src = "y = items[:]\n";
    auto ast = parseToAST(src);
    expect(ast.find("Slice") != std::string::npos,
           "Empty slice: AST contains Slice node");
}

// ── Test 14: Dict comprehension with filter ───────────────────────────────
static void test_dict_comprehension_filter() {
    std::string src = "d = {k: v for k in items if k > 0}\n";
    auto ast = parseToAST(src);
    expect(ast.find("DictComp") != std::string::npos,
           "Dict comp filter: AST contains DictComp");
}

// ════════════════════════════════════════════════════════════════════════════
// Entry point
// ════════════════════════════════════════════════════════════════════════════
int runSyntaxTests() {
    failures = 0;

    test_default_params();
    test_default_param_with_type();
    test_slice_start_stop();
    test_slice_with_step();
    test_list_comprehension();
    test_list_comprehension_filter();
    test_dict_comprehension();
    test_tuple_unpacking();
    test_variadic_function();
    test_decorator();
    test_multiple_decorators();
    test_spread_expression();
    test_empty_slice();
    test_dict_comprehension_filter();

    return failures;
}
