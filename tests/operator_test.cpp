// ════════════════════════════════════════════════════════════════════════════
// tests/operator_test.cpp — Test suite for augmented assignment and bitwise
// operator support in the Visuall compiler.
//
// 14 test cases:
//   1.  x += 1 desugars correctly
//   2.  arr[0] += 1 desugars correctly
//   3.  this.x += 1 desugars correctly
//   4.  All augmented assignment operators desugar
//   5.  & | ^ ~ << >> lex as correct tokens
//   6.  &= |= ^= <<= >>= lex correctly
//   7.  Bitwise AND of two ints produces CreateAnd IR
//   8.  Bitwise OR of two ints produces CreateOr IR
//   9.  Left shift produces CreateShl IR
//  10.  Right shift produces CreateAShr IR
//  11.  ~ on int produces CreateNot IR
//  12.  Bitwise op on float produces TypeError
//  13.  Precedence: a | b & c is a | (b & c)
//  14.  a <<= 2 desugars to a = a << 2
// ════════════════════════════════════════════════════════════════════════════

#include "lexer.h"
#include "parser.h"
#include "typechecker.h"
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

// Helper: lex + parse, return the program AST. Returns nullptr on error.
static std::unique_ptr<ast::Program> parseSource(const std::string& src) {
    try {
        Lexer lexer(src, "test.vsl");
        auto tokens = lexer.tokenize();
        Parser parser(tokens, "test.vsl");
        return parser.parse();
    } catch (const std::exception& e) {
        std::cerr << "  [parseSource error] " << e.what() << "\n";
        return nullptr;
    }
}

// Helper: lex + parse + codegen, return LLVM IR as string.
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

// Helper: lex + parse + typecheck. Returns "" on success, error message on failure.
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

// ── 1. x += 1 desugars correctly ──────────────────────────────────────────
static void test_simpleAugAssign() {
    auto prog = parseSource("x = 10\nx += 1\n");
    expect(prog != nullptr, "1a. x += 1 parses");
    if (!prog) return;
    expect(prog->statements.size() == 2, "1b. two statements");
    auto* assign = dynamic_cast<ast::AssignStmt*>(prog->statements[1].get());
    expect(assign != nullptr, "1c. second stmt is AssignStmt");
    if (!assign) return;
    // Target should be Identifier "x"
    auto* target = dynamic_cast<ast::Identifier*>(assign->target.get());
    expect(target != nullptr && target->name == "x", "1d. target is 'x'");
    // Value should be BinaryExpr(Add, x, 1)
    auto* binop = dynamic_cast<ast::BinaryExpr*>(assign->value.get());
    expect(binop != nullptr, "1e. value is BinaryExpr");
    if (binop) {
        expect(binop->op == ast::BinOp::Add, "1f. op is Add");
        auto* lhs = dynamic_cast<ast::Identifier*>(binop->left.get());
        expect(lhs != nullptr && lhs->name == "x", "1g. left is 'x'");
        auto* rhs = dynamic_cast<ast::IntLiteral*>(binop->right.get());
        expect(rhs != nullptr && rhs->value == 1, "1h. right is 1");
    }
}

// ── 2. arr[0] += 1 desugars correctly ─────────────────────────────────────
static void test_indexAugAssign() {
    auto prog = parseSource("arr = [1, 2, 3]\narr[0] += 1\n");
    expect(prog != nullptr, "2a. arr[0] += 1 parses");
    if (!prog) return;
    auto* assign = dynamic_cast<ast::AssignStmt*>(prog->statements[1].get());
    expect(assign != nullptr, "2b. second stmt is AssignStmt");
    if (!assign) return;
    // Target should be IndexExpr
    auto* target = dynamic_cast<ast::IndexExpr*>(assign->target.get());
    expect(target != nullptr, "2c. target is IndexExpr");
    // Value should be BinaryExpr(Add, IndexExpr(arr, 0), 1)
    auto* binop = dynamic_cast<ast::BinaryExpr*>(assign->value.get());
    expect(binop != nullptr && binop->op == ast::BinOp::Add, "2d. value is Add expr");
    if (binop) {
        auto* lhs = dynamic_cast<ast::IndexExpr*>(binop->left.get());
        expect(lhs != nullptr, "2e. left is IndexExpr (clone of target)");
    }
}

// ── 3. this.x += 1 desugars correctly ─────────────────────────────────────
static void test_memberAugAssign() {
    // We parse inside a class context for 'this' to be valid
    std::string src =
        "class Foo:\n"
        "\tinit():\n"
        "\t\tthis.count = 0\n"
        "\tdefine inc():\n"
        "\t\tthis.count += 1\n";
    auto prog = parseSource(src);
    expect(prog != nullptr, "3a. this.count += 1 parses");
    if (!prog) return;
    auto* cls = dynamic_cast<ast::ClassDef*>(prog->statements[0].get());
    expect(cls != nullptr, "3b. is ClassDef");
    if (!cls || cls->body.size() < 2) return;
    auto* method = dynamic_cast<ast::FuncDef*>(cls->body[1].get());
    expect(method != nullptr, "3c. second member is method");
    if (!method || method->body.empty()) return;
    auto* assign = dynamic_cast<ast::AssignStmt*>(method->body[0].get());
    expect(assign != nullptr, "3d. body stmt is AssignStmt");
    if (!assign) return;
    auto* target = dynamic_cast<ast::MemberExpr*>(assign->target.get());
    expect(target != nullptr, "3e. target is MemberExpr");
    auto* binop = dynamic_cast<ast::BinaryExpr*>(assign->value.get());
    expect(binop != nullptr && binop->op == ast::BinOp::Add, "3f. value is Add expr");
    if (binop) {
        auto* lhs = dynamic_cast<ast::MemberExpr*>(binop->left.get());
        expect(lhs != nullptr, "3g. left is MemberExpr (clone of target)");
    }
}

// ── 4. All augmented assignment operators desugar ──────────────────────────
static void test_allAugAssignOps() {
    struct TestCase { const char* src; ast::BinOp expectedOp; const char* name; };
    TestCase cases[] = {
        {"x = 0\nx += 1\n",   ast::BinOp::Add,    "+="},
        {"x = 0\nx -= 1\n",   ast::BinOp::Sub,    "-="},
        {"x = 0\nx *= 1\n",   ast::BinOp::Mul,    "*="},
        {"x = 0\nx /= 1\n",   ast::BinOp::Div,    "/="},
        {"x = 0\nx //= 1\n",  ast::BinOp::IntDiv, "//="},
        {"x = 0\nx %= 1\n",   ast::BinOp::Mod,    "%="},
        {"x = 0\nx **= 1\n",  ast::BinOp::Pow,    "**="},
        {"x = 0\nx &= 1\n",   ast::BinOp::BitAnd, "&="},
        {"x = 0\nx |= 1\n",   ast::BinOp::BitOr,  "|="},
        {"x = 0\nx ^= 1\n",   ast::BinOp::BitXor, "^="},
        {"x = 0\nx <<= 1\n",  ast::BinOp::Shl,    "<<="},
        {"x = 0\nx >>= 1\n",  ast::BinOp::Shr,    ">>="},
    };
    for (auto& tc : cases) {
        auto prog = parseSource(tc.src);
        bool ok = false;
        if (prog && prog->statements.size() == 2) {
            auto* assign = dynamic_cast<ast::AssignStmt*>(prog->statements[1].get());
            if (assign) {
                auto* binop = dynamic_cast<ast::BinaryExpr*>(assign->value.get());
                if (binop && binop->op == tc.expectedOp) ok = true;
            }
        }
        expect(ok, std::string("4. augmented assign desugars: ") + tc.name);
    }
}

// ── 5. & | ^ ~ << >> lex as correct tokens ────────────────────────────────
static void test_bitwiseTokens() {
    Lexer lexer("& | ^ ~ << >>\n", "test.vsl");
    auto tokens = lexer.tokenize();
    // Tokens: AMP PIPE CARET TILDE LSHIFT RSHIFT NEWLINE EOF
    expect(tokens.size() >= 7, "5a. at least 7 tokens");
    expect(tokens[0].type == TokenType::AMP,    "5b. & is AMP");
    expect(tokens[1].type == TokenType::PIPE,   "5c. | is PIPE");
    expect(tokens[2].type == TokenType::CARET,  "5d. ^ is CARET");
    expect(tokens[3].type == TokenType::TILDE,  "5e. ~ is TILDE");
    expect(tokens[4].type == TokenType::LSHIFT, "5f. << is LSHIFT");
    expect(tokens[5].type == TokenType::RSHIFT, "5g. >> is RSHIFT");
}

// ── 6. &= |= ^= <<= >>= lex correctly ────────────────────────────────────
static void test_bitwiseAssignTokens() {
    Lexer lexer("&= |= ^= <<= >>=\n", "test.vsl");
    auto tokens = lexer.tokenize();
    expect(tokens.size() >= 6, "6a. at least 6 tokens");
    expect(tokens[0].type == TokenType::AMP_EQ,    "6b. &= is AMP_EQ");
    expect(tokens[1].type == TokenType::PIPE_EQ,   "6c. |= is PIPE_EQ");
    expect(tokens[2].type == TokenType::CARET_EQ,  "6d. ^= is CARET_EQ");
    expect(tokens[3].type == TokenType::LSHIFT_EQ, "6e. <<= is LSHIFT_EQ");
    expect(tokens[4].type == TokenType::RSHIFT_EQ, "6f. >>= is RSHIFT_EQ");
}

// ── 7. Bitwise AND of two ints produces CreateAnd IR ───────────────────────
static void test_bitwiseAndIR() {
    std::string ir = generateIR("a = 5\nb = 3\nc = a & b\n");
    expect(!ir.empty(), "7a. bitwise AND generates IR");
    // LLVM's CreateAnd produces 'and' instruction in IR
    expect(ir.find(" and ") != std::string::npos, "7b. IR contains 'and' instruction");
}

// ── 8. Bitwise OR of two ints produces CreateOr IR ─────────────────────────
static void test_bitwiseOrIR() {
    std::string ir = generateIR("a = 5\nb = 3\nc = a | b\n");
    expect(!ir.empty(), "8a. bitwise OR generates IR");
    // LLVM's CreateOr produces 'or' instruction in IR
    expect(ir.find(" or ") != std::string::npos, "8b. IR contains 'or' instruction");
}

// ── 9. Left shift produces CreateShl IR ────────────────────────────────────
static void test_shlIR() {
    std::string ir = generateIR("a = 1\nb = a << 4\n");
    expect(!ir.empty(), "9a. left shift generates IR");
    expect(ir.find("shl") != std::string::npos, "9b. IR contains 'shl' instruction");
}

// ── 10. Right shift produces CreateAShr IR ─────────────────────────────────
static void test_ashrIR() {
    std::string ir = generateIR("a = 16\nb = a >> 2\n");
    expect(!ir.empty(), "10a. right shift generates IR");
    expect(ir.find("ashr") != std::string::npos, "10b. IR contains 'ashr' instruction");
}

// ── 11. ~ on int produces CreateNot IR ─────────────────────────────────────
static void test_bitwiseNotIR() {
    std::string ir = generateIR("a = 42\nb = ~a\n");
    expect(!ir.empty(), "11a. bitwise NOT generates IR");
    // LLVM's CreateNot produces 'xor ..., -1' in the IR
    expect(ir.find("xor") != std::string::npos, "11b. IR contains 'xor' (CreateNot)");
}

// ── 12. Bitwise op on float produces TypeError ─────────────────────────────
static void test_bitwiseFloatTypeError() {
    std::string err = typecheck("a = 1.0\nb = 2.0\nc = a & b\n");
    expect(err.find("TypeError") != std::string::npos,
           "12a. & on float produces TypeError");

    err = typecheck("a = 1.0\nb = ~a\n");
    expect(err.find("TypeError") != std::string::npos,
           "12b. ~ on float produces TypeError");
}

// ── 13. Precedence: a | b & c is a | (b & c) ──────────────────────────────
static void test_bitwisePrecedence() {
    auto prog = parseSource("a = 1\nb = 2\nc = 3\nd = a | b & c\n");
    expect(prog != nullptr, "13a. precedence expr parses");
    if (!prog || prog->statements.size() < 4) return;
    auto* assign = dynamic_cast<ast::AssignStmt*>(prog->statements[3].get());
    expect(assign != nullptr, "13b. fourth stmt is AssignStmt");
    if (!assign) return;
    // d = a | (b & c)  →  BinaryExpr(BitOr, a, BinaryExpr(BitAnd, b, c))
    auto* orExpr = dynamic_cast<ast::BinaryExpr*>(assign->value.get());
    expect(orExpr != nullptr && orExpr->op == ast::BinOp::BitOr, "13c. top-level op is BitOr");
    if (!orExpr) return;
    auto* andExpr = dynamic_cast<ast::BinaryExpr*>(orExpr->right.get());
    expect(andExpr != nullptr && andExpr->op == ast::BinOp::BitAnd,
           "13d. right child is BitAnd (& binds tighter than |)");
}

// ── 14. a <<= 2 desugars to a = a << 2 ────────────────────────────────────
static void test_shiftAugAssign() {
    auto prog = parseSource("a = 1\na <<= 2\n");
    expect(prog != nullptr, "14a. a <<= 2 parses");
    if (!prog || prog->statements.size() < 2) return;
    auto* assign = dynamic_cast<ast::AssignStmt*>(prog->statements[1].get());
    expect(assign != nullptr, "14b. second stmt is AssignStmt");
    if (!assign) return;
    auto* target = dynamic_cast<ast::Identifier*>(assign->target.get());
    expect(target != nullptr && target->name == "a", "14c. target is 'a'");
    auto* binop = dynamic_cast<ast::BinaryExpr*>(assign->value.get());
    expect(binop != nullptr && binop->op == ast::BinOp::Shl, "14d. op is Shl (<<)");
    if (binop) {
        auto* lhs = dynamic_cast<ast::Identifier*>(binop->left.get());
        expect(lhs != nullptr && lhs->name == "a", "14e. left is 'a'");
        auto* rhs = dynamic_cast<ast::IntLiteral*>(binop->right.get());
        expect(rhs != nullptr && rhs->value == 2, "14f. right is 2");
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Entry point
// ════════════════════════════════════════════════════════════════════════════
int runOperatorTests() {
    failures = 0;

    test_simpleAugAssign();
    test_indexAugAssign();
    test_memberAugAssign();
    test_allAugAssignOps();
    test_bitwiseTokens();
    test_bitwiseAssignTokens();
    test_bitwiseAndIR();
    test_bitwiseOrIR();
    test_shlIR();
    test_ashrIR();
    test_bitwiseNotIR();
    test_bitwiseFloatTypeError();
    test_bitwisePrecedence();
    test_shiftAugAssign();

    return failures;
}
