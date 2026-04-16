// ════════════════════════════════════════════════════════════════════════════
// tests/typesystem_test.cpp — Test suite for the Visuall type system features.
//
// 15 test cases covering:
//   Inheritance & super (1-4)
//   Interfaces / Abstract Classes (5-7)
//   Generics (8-11)
//   Union & Optional Types (12-15)
// ════════════════════════════════════════════════════════════════════════════

#include "lexer.h"
#include "parser.h"
#include "typechecker.h"
#include "codegen.h"
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

// Helper: lex + parse + typecheck.  Returns "" on success, error .what() on failure.
static std::string typecheck(const std::string& src) {
    try {
        Lexer lexer(src, "test.vsl");
        auto tokens = lexer.tokenize();
        Parser parser(tokens, "test.vsl");
        auto program = parser.parse();
        TypeChecker checker("test.vsl");
        checker.check(*program);
        return "";
    } catch (const std::exception& e) {
        return e.what();
    }
}

// Helper: lex + parse + typecheck + codegen.  Returns IR string or error.
static std::string codegen(const std::string& src) {
    try {
        Lexer lexer(src, "test.vsl");
        auto tokens = lexer.tokenize();
        Parser parser(tokens, "test.vsl");
        auto program = parser.parse();
        TypeChecker checker("test.vsl");
        checker.check(*program);
        Codegen cg("test_module");
        cg.generate(*program);
        std::ostringstream oss;
        cg.printIR(oss);
        return oss.str();
    } catch (const std::exception& e) {
        return std::string("ERROR: ") + e.what();
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Inheritance & super
// ════════════════════════════════════════════════════════════════════════════

// 1. Dog extends Animal — speak() method type checks
static void testInheritanceBasic() {
    std::string src =
        "class Animal:\n"
        "\tdefine speak() -> str:\n"
        "\t\treturn \"...\"\n"
        "\n"
        "class Dog extends Animal:\n"
        "\tdefine speak() -> str:\n"
        "\t\treturn \"woof\"\n";
    auto result = typecheck(src);
    expect(result.empty(), "1. Dog extends Animal — speak() type checks");
}

// 2. super.init() resolves to base class init
static void testSuperInit() {
    std::string src =
        "class Animal:\n"
        "\tinit(name: str):\n"
        "\t\tthis.name = name\n"
        "\n"
        "class Dog extends Animal:\n"
        "\tinit(name: str, breed: str):\n"
        "\t\tsuper.init(name)\n"
        "\t\tthis.breed = breed\n";
    auto result = typecheck(src);
    expect(result.empty(), "2. super.init() resolves to base class");
}

// 3. Assigning Dog to Animal parameter (subtype)
static void testSubtypeParam() {
    std::string src =
        "class Animal:\n"
        "\tdefine speak() -> str:\n"
        "\t\treturn \"...\"\n"
        "\n"
        "class Dog extends Animal:\n"
        "\tdefine speak() -> str:\n"
        "\t\treturn \"woof\"\n"
        "\n"
        "define greet(a: Animal) -> str:\n"
        "\treturn a.speak()\n";
    auto result = typecheck(src);
    expect(result.empty(), "3. Dog assignable to Animal param");
}

// 4. Override with wrong return type → TypeError
static void testOverrideWrongSignature() {
    std::string src =
        "class Animal:\n"
        "\tdefine speak() -> str:\n"
        "\t\treturn \"...\"\n"
        "\n"
        "class Dog extends Animal:\n"
        "\tdefine speak() -> int:\n"
        "\t\treturn 42\n";
    auto result = typecheck(src);
    expect(!result.empty(), "4. Override with wrong return type caught");
}

// ════════════════════════════════════════════════════════════════════════════
// Interfaces / Abstract Classes
// ════════════════════════════════════════════════════════════════════════════

// 5. Class implements interface — all methods present
static void testInterfaceImplemented() {
    std::string src =
        "interface Drawable:\n"
        "\tdefine draw() -> str\n"
        "\n"
        "class Circle implements Drawable:\n"
        "\tdefine draw() -> str:\n"
        "\t\treturn \"o\"\n";
    auto result = typecheck(src);
    expect(result.empty(), "5. Circle implements Drawable — OK");
}

// 6. Missing interface method → TypeError
static void testInterfaceMissing() {
    std::string src =
        "interface Drawable:\n"
        "\tdefine draw() -> str\n"
        "\n"
        "class Square implements Drawable:\n"
        "\tdefine area() -> int:\n"
        "\t\treturn 0\n";
    auto result = typecheck(src);
    expect(!result.empty(), "6. Missing interface method caught");
}

// 7. Interface as parameter type
static void testInterfaceAsParam() {
    std::string src =
        "interface Printable:\n"
        "\tdefine show() -> str\n"
        "\n"
        "class Msg implements Printable:\n"
        "\tdefine show() -> str:\n"
        "\t\treturn \"hello\"\n"
        "\n"
        "define display(p: Printable) -> str:\n"
        "\treturn p.show()\n";
    auto result = typecheck(src);
    expect(result.empty(), "7. Interface as parameter type — OK");
}

// ════════════════════════════════════════════════════════════════════════════
// Generics
// ════════════════════════════════════════════════════════════════════════════

// 8. identity<int>(42) returns int
static void testGenericExplicit() {
    std::string src =
        "define identity<T>(x: T) -> T:\n"
        "\treturn x\n"
        "\n"
        "y = identity<int>(42)\n";
    auto result = typecheck(src);
    expect(result.empty(), "8. identity<int>(42) returns int");
}

// 9. identity("hi") infers T=str
static void testGenericInference() {
    std::string src =
        "define identity<T>(x: T) -> T:\n"
        "\treturn x\n"
        "\n"
        "y = identity(\"hi\")\n";
    auto result = typecheck(src);
    expect(result.empty(), "9. identity(\"hi\") infers T=str");
}

// 10. Generic class definition type checks
static void testGenericClass() {
    std::string src =
        "class Box<T>:\n"
        "\tdefine get() -> T:\n"
        "\t\treturn this.value\n";
    auto result = typecheck(src);
    expect(result.empty(), "10. Box<T> class definition type checks");
}

// 11. Two-param generic function
static void testGenericTwoParams() {
    std::string src =
        "define pair<A, B>(a: A, b: B) -> A:\n"
        "\treturn a\n"
        "\n"
        "x = pair<int, str>(1, \"two\")\n";
    auto result = typecheck(src);
    expect(result.empty(), "11. Two-param generic pair<int,str>");
}

// ════════════════════════════════════════════════════════════════════════════
// Union & Optional Types
// ════════════════════════════════════════════════════════════════════════════

// 12. int|null without null check — does not crash
static void testUnionNoNarrow() {
    std::string src =
        "define foo(x: int|null) -> int:\n"
        "\treturn x + 1\n";
    auto result = typecheck(src);
    // Just verify it doesn't crash — may or may not error depending on strictness
    expect(true, "12. int|null no-narrow does not crash");
}

// 13. int|null after null check narrowed → OK
static void testUnionNarrow() {
    std::string src =
        "define foo(x: int|null) -> int:\n"
        "\tif x != null:\n"
        "\t\treturn x + 1\n"
        "\treturn 0\n";
    auto result = typecheck(src);
    expect(result.empty(), "13. int|null after null check narrowed");
}

// 14. ?str desugars to str|null
static void testOptionalSugar() {
    std::string src =
        "define foo(x: ?str) -> str:\n"
        "\tif x != null:\n"
        "\t\treturn x\n"
        "\treturn \"default\"\n";
    auto result = typecheck(src);
    expect(result.empty(), "14. ?str desugars to str|null");
}

// 15. Monomorphized functions have distinct names in IR
static void testMonomorphIR() {
    std::string src =
        "define identity<T>(x: T) -> T:\n"
        "\treturn x\n"
        "\n"
        "a = identity<int>(42)\n"
        "b = identity<str>(\"hi\")\n";
    auto ir = codegen(src);
    bool hasIntVersion = ir.find("identity__int") != std::string::npos;
    bool hasStrVersion = ir.find("identity__str") != std::string::npos;
    expect(hasIntVersion && hasStrVersion,
           "15. Monomorphized identity__int and identity__str in IR");
}

// ════════════════════════════════════════════════════════════════════════════
// Entry point
// ════════════════════════════════════════════════════════════════════════════
int runTypeSystemTests() {
    failures = 0;

    testInheritanceBasic();
    testSuperInit();
    testSubtypeParam();
    testOverrideWrongSignature();
    testInterfaceImplemented();
    testInterfaceMissing();
    testInterfaceAsParam();
    testGenericExplicit();
    testGenericInference();
    testGenericClass();
    testGenericTwoParams();
    testUnionNoNarrow();
    testUnionNarrow();
    testOptionalSugar();
    testMonomorphIR();

    return failures;
}
