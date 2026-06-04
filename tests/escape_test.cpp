// ════════════════════════════════════════════════════════════════════════════
// tests/escape_test.cpp — Test suite for EscapeAnalyzer.
//
// 9 test cases:
//   1. Non-escaping list (local use only) → stack-allocatable
//   2. Escaping via return → heap-only
//   3. Escaping via store into another list → heap-only
//   4. Non-escaping closure env → stack-allocatable
//   5. Escaping closure (returned) → heap-only
//   6. List inside for-loop → heap-only (Trap 1)
//   7. List inside while-loop → heap-only (Trap 1)
//   8. Top-level list → heap-only (always escapes)
//   9. List passed as call argument → heap-only (conservative)
// ════════════════════════════════════════════════════════════════════════════

#include "lexer.h"
#include "parser.h"
#include "capture_analyzer.h"
#include "class_analyzer.h"
#include "typechecker.h"
#include "escape_analyzer.h"
#include <functional>
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

static bool lookupEsc(const EscapeAnalyzer& ea, const void* key) {
    auto it = ea.stackAllocatable().find(key);
    return it != ea.stackAllocatable().end() && it->second;
}

static void runTest(const std::string& source, const std::string& filename,
                    std::function<void(const EscapeAnalyzer&, const ast::Program&)> check) {
    Lexer lex(source, filename);
    auto tokens = lex.tokenize();
    Parser parser(tokens, filename);
    auto program = parser.parse();
    CaptureAnalyzer ca; ca.analyze(*program);
    ClassAnalyzer cla; cla.analyze(*program);
    TypeChecker tc(filename); tc.check(*program);
    EscapeAnalyzer ea; ea.analyze(*program);
    check(ea, *program);
}

static const ast::ListExpr* findListInFunc(const ast::Program& prog) {
    for (const auto& stmt : prog.statements) {
        if (auto* fd = dynamic_cast<const ast::FuncDef*>(stmt.get())) {
            for (const auto& bs : fd->body) {
                if (auto* assign = dynamic_cast<const ast::AssignStmt*>(bs.get())) {
                    if (auto* le = dynamic_cast<const ast::ListExpr*>(assign->value.get()))
                        return le;
                }
            }
        }
    }
    return nullptr;
}

static const ast::ListExpr* findTopLevelList(const ast::Program& prog) {
    for (const auto& stmt : prog.statements) {
        if (auto* assign = dynamic_cast<const ast::AssignStmt*>(stmt.get())) {
            if (auto* le = dynamic_cast<const ast::ListExpr*>(assign->value.get()))
                return le;
        }
    }
    return nullptr;
}

static const ast::LambdaExpr* findLambdaInFunc(const ast::Program& prog) {
    for (const auto& stmt : prog.statements) {
        if (auto* fd = dynamic_cast<const ast::FuncDef*>(stmt.get())) {
            for (const auto& bs : fd->body) {
                if (auto* assign = dynamic_cast<const ast::AssignStmt*>(bs.get())) {
                    if (auto* le = dynamic_cast<const ast::LambdaExpr*>(assign->value.get()))
                        return le;
                }
            }
        }
    }
    return nullptr;
}

int runEscapeTests() {
    std::cout << "\n--- Escape Analysis Tests ---\n";

    // ── 1. Non-escaping list (local use only) ────────────────────────────
    runTest(
        "define f() -> int:\n"
        "\tx = [1, 2, 3]\n"
        "\treturn x[0]\n",
        "test1.vsl",
        [](const EscapeAnalyzer& ea, const ast::Program& prog) {
            auto* le = findListInFunc(prog);
            expect(le != nullptr, "1a - found list");
            expect(lookupEsc(ea, le), "1b - local list is stack-allocatable");
        });

    // ── 2. Escaping via return ───────────────────────────────────────────
    runTest(
        "define f() -> list:\n"
        "\treturn [1, 2, 3]\n",
        "test2.vsl",
        [](const EscapeAnalyzer& ea, const ast::Program& prog) {
            const ast::ListExpr* le = nullptr;
            for (const auto& stmt : prog.statements) {
                if (auto* fd = dynamic_cast<const ast::FuncDef*>(stmt.get())) {
                    for (const auto& bs : fd->body) {
                        if (auto* ret = dynamic_cast<const ast::ReturnStmt*>(bs.get())) {
                            le = dynamic_cast<const ast::ListExpr*>(ret->value.get());
                        }
                    }
                }
            }
            expect(le != nullptr, "2a - found list in return");
            expect(!lookupEsc(ea, le), "2b - returned list is NOT stack-allocatable");
        });

    // ── 3. Escaping via store into another list ──────────────────────────
    runTest(
        "define f() -> list:\n"
        "\touter = []\n"
        "\tinner = [1, 2]\n"
        "\touter[0] = inner\n"
        "\treturn outer\n",
        "test3.vsl",
        [](const EscapeAnalyzer& ea, const ast::Program& prog) {
            int listCount = 0, heapOnly = 0;
            for (const auto& stmt : prog.statements) {
                if (auto* fd = dynamic_cast<const ast::FuncDef*>(stmt.get())) {
                    for (const auto& bs : fd->body) {
                        if (auto* assign = dynamic_cast<const ast::AssignStmt*>(bs.get())) {
                            if (auto* le = dynamic_cast<const ast::ListExpr*>(assign->value.get())) {
                                listCount++;
                                if (!lookupEsc(ea, le)) heapOnly++;
                            }
                        }
                    }
                }
            }
            expect(listCount >= 2, "3a - found both lists");
            expect(heapOnly >= 1, "3b - stored-into list escapes");
        });

    // ── 4. Non-escaping closure env ──────────────────────────────────────
    runTest(
        "define f() -> int:\n"
        "\tfn = x -> x * 2\n"
        "\treturn fn(21)\n",
        "test4.vsl",
        [](const EscapeAnalyzer& ea, const ast::Program& prog) {
            auto* le = findLambdaInFunc(prog);
            expect(le != nullptr, "4a - found lambda");
            if (le && !le->captures.empty()) {
                expect(lookupEsc(ea, le), "4b - local closure env stack-allocatable");
            } else {
                std::cout << "  SKIP: 4b - lambda has no captures\n";
            }
        });

    // ── 5. Escaping closure (returned) ───────────────────────────────────
    runTest(
        "define f() -> (int)->int:\n"
        "\tx = 42\n"
        "\treturn n -> n + x\n",
        "test5.vsl",
        [](const EscapeAnalyzer& ea, const ast::Program& prog) {
            const ast::LambdaExpr* le = nullptr;
            for (const auto& stmt : prog.statements) {
                if (auto* fd = dynamic_cast<const ast::FuncDef*>(stmt.get())) {
                    for (const auto& bs : fd->body) {
                        if (auto* ret = dynamic_cast<const ast::ReturnStmt*>(bs.get())) {
                            le = dynamic_cast<const ast::LambdaExpr*>(ret->value.get());
                        }
                    }
                }
            }
            expect(le != nullptr, "5a - found lambda in return");
            if (le) {
                expect(!lookupEsc(ea, le), "5b - returned closure env is NOT stack-allocatable");
            }
        });

    // ── 6. List inside for-loop → heap-only (Trap 1) ─────────────────────
    runTest(
        "define f():\n"
        "\tfor i in range(10):\n"
        "\t\ttmp = [i, i+1]\n",
        "test6.vsl",
        [](const EscapeAnalyzer& ea, const ast::Program& prog) {
            bool foundLoopList = false, loopListEscapes = true;
            for (const auto& stmt : prog.statements) {
                if (auto* fd = dynamic_cast<const ast::FuncDef*>(stmt.get())) {
                    for (const auto& bs : fd->body) {
                        if (auto* forStmt = dynamic_cast<const ast::ForStmt*>(bs.get())) {
                            for (const auto& fbs : forStmt->body) {
                                if (auto* assign = dynamic_cast<const ast::AssignStmt*>(fbs.get())) {
                                    if (auto* le = dynamic_cast<const ast::ListExpr*>(assign->value.get())) {
                                        foundLoopList = true;
                                        loopListEscapes = !lookupEsc(ea, le);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            expect(foundLoopList, "6a - found list inside loop");
            expect(loopListEscapes, "6b - loop-nested list is NOT stack-allocatable (Trap 1)");
        });

    // ── 7. List inside while-loop → heap-only (Trap 1) ───────────────────
    runTest(
        "define f():\n"
        "\ti = 0\n"
        "\twhile i < 5:\n"
        "\t\ttmp = [i]\n"
        "\t\ti = i + 1\n",
        "test7.vsl",
        [](const EscapeAnalyzer& ea, const ast::Program& prog) {
            bool foundLoopList = false, loopListEscapes = true;
            for (const auto& stmt : prog.statements) {
                if (auto* fd = dynamic_cast<const ast::FuncDef*>(stmt.get())) {
                    for (const auto& bs : fd->body) {
                        if (auto* whileStmt = dynamic_cast<const ast::WhileStmt*>(bs.get())) {
                            for (const auto& wbs : whileStmt->body) {
                                if (auto* assign = dynamic_cast<const ast::AssignStmt*>(wbs.get())) {
                                    if (auto* le = dynamic_cast<const ast::ListExpr*>(assign->value.get())) {
                                        foundLoopList = true;
                                        loopListEscapes = !lookupEsc(ea, le);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            expect(foundLoopList, "7a - found list inside while");
            expect(loopListEscapes, "7b - while-nested list is NOT stack-allocatable (Trap 1)");
        });

    // ── 8. Top-level list → heap-only ────────────────────────────────────
    runTest(
        "x = [1, 2, 3]\n",
        "test8.vsl",
        [](const EscapeAnalyzer& ea, const ast::Program& prog) {
            auto* le = findTopLevelList(prog);
            expect(le != nullptr, "8a - found top-level list");
            expect(!lookupEsc(ea, le), "8b - top-level list is NOT stack-allocatable");
        });

    // ── 9. List passed as call argument → heap-only (conservative) ───────
    runTest(
        "define helper(x):\n"
        "\tx = x\n"
        "define f():\n"
        "\thelper([1, 2])\n",
        "test9.vsl",
        [](const EscapeAnalyzer& ea, const ast::Program& prog) {
            const ast::ListExpr* le = nullptr;
            for (const auto& stmt : prog.statements) {
                if (auto* fd = dynamic_cast<const ast::FuncDef*>(stmt.get())) {
                    for (const auto& bs : fd->body) {
                        if (auto* es = dynamic_cast<const ast::ExprStmt*>(bs.get())) {
                            if (auto* call = dynamic_cast<const ast::CallExpr*>(es->expr.get())) {
                                for (const auto& arg : call->args) {
                                    if (auto* argList = dynamic_cast<const ast::ListExpr*>(arg.get()))
                                        le = argList;
                                }
                            }
                        }
                    }
                }
            }
            expect(le != nullptr, "9a - found list argument");
            expect(!lookupEsc(ea, le), "9b - list passed as call arg is NOT stack-allocatable");
        });

    return failures;
}
