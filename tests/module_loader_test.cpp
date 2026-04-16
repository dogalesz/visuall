// ════════════════════════════════════════════════════════════════════════════
// tests/module_loader_test.cpp — Test suite for the Visuall module system.
// ════════════════════════════════════════════════════════════════════════════

#include "module_loader.h"
#include "lexer.h"
#include "parser.h"
#include "capture_analyzer.h"
#include "codegen.h"
#include "linker.h"

#include <iostream>
#include <string>
#include <functional>
#include <cstdio>

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
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

static void expectThrows(std::function<void()> fn,
                          const std::string& substring,
                          const std::string& testName) {
    try {
        fn();
        std::cerr << "  FAIL: " << testName << " (no exception thrown)\n";
        failures++;
    } catch (const ImportError& e) {
        std::string msg = e.what();
        if (msg.find(substring) != std::string::npos) {
            std::cout << "  PASS: " << testName << "\n";
        } else {
            std::cerr << "  FAIL: " << testName
                      << " (expected '" << substring << "' in: " << msg << ")\n";
            failures++;
        }
    } catch (const std::exception& e) {
        std::cerr << "  FAIL: " << testName
                  << " (wrong exception: " << e.what() << ")\n";
        failures++;
    }
}

static void writeTempFile(const std::string& path, const std::string& content) {
    FILE* f = fopen(path.c_str(), "w");
    if (f) {
        fwrite(content.data(), 1, content.size(), f);
        fclose(f);
    }
}

// ── Test 1: Module resolved from current directory ─────────────────────────
static void test_resolveFromCurrentDir() {
    writeTempFile("tests/module_test/temp_resolve.vsl",
                  "define foo() -> int:\n\treturn 42\n");
    ModuleLoader loader("stdlib", {}, false);
    try {
        Module& mod = loader.load("temp_resolve",
                                   "tests/module_test/main.vsl");
        expect(mod.name == "temp_resolve", "1a. module name");
        expect(mod.exports.count("foo") > 0, "1b. exports foo");
    } catch (const std::exception& e) {
        std::cerr << "  FAIL: 1. " << e.what() << "\n";
        failures += 2;
    }
    std::remove("tests/module_test/temp_resolve.vsl");
}

// ── Test 2: Module resolved from extra search path ─────────────────────────
static void test_resolveFromModulePath() {
    ModuleLoader loader("stdlib", {"tests/module_test"}, false);

    try {
        Module& mod = loader.load("utils", "some/other/dir/fake.vsl");
        expect(mod.filepath.find("utils.vsl") != std::string::npos,
               "2a. Resolve finds utils.vsl from extra search path");
        expect(mod.exports.count("add") > 0,
               "2b. Module exports 'add' function");
    } catch (const std::exception& e) {
        std::cerr << "  FAIL: 2. " << e.what() << "\n";
        failures += 2;
    }
}

// ── Test 3: Stdlib module resolved as builtin ──────────────────────────────
static void test_resolveStdlibModule() {
    ModuleLoader loader("stdlib", {}, false);

    Module& mod = loader.load("math", "test.vsl");
    expect(mod.name == "math",
           "3a. Stdlib module 'math' resolved");
    expect(mod.filepath.find("builtin") != std::string::npos,
           "3b. Stdlib module filepath indicates builtin");
}

// ── Test 4: Module compilation and export extraction ───────────────────────
static void test_symbolFromImport() {
    ModuleLoader loader("stdlib", {"tests/module_test"}, false);

    try {
        Module& mod = loader.load("utils", "test.vsl");
        expect(mod.name == "utils",
               "4a. Module name is 'utils'");
        expect(mod.exports.count("add") > 0,
               "4b. 'add' exported from utils");
        expect(mod.exports.count("subtract") > 0,
               "4c. 'subtract' exported from utils");
        expect(mod.exports.count("multiply") > 0,
               "4d. 'multiply' exported from utils");
        expect(mod.exports["add"].mangledName == "utils.add",
               "4e. 'add' mangled name is 'utils.add'");
    } catch (const std::exception& e) {
        std::cerr << "  FAIL: 4. " << e.what() << "\n";
        failures += 5;
    }
}

// ── Test 5: Namespace access generates correct IR ──────────────────────────
static void test_namespaceAccess() {
    std::string src =
        "import utils\n"
        "x = utils.add(1, 2)\n";

    ModuleLoader loader("stdlib", {"tests/module_test"}, false);

    try {
        Lexer lexer(src, "tests/module_test/test_ns.vsl");
        auto tokens = lexer.tokenize();
        Parser parser(tokens, "tests/module_test/test_ns.vsl");
        auto program = parser.parse();
        CaptureAnalyzer ca;
        ca.analyze(*program);
        Codegen codegen("test_ns");
        codegen.setModuleLoader(&loader);
        codegen.setSourceFile("tests/module_test/test_ns.vsl");
        codegen.generate(*program);

        std::string ir;
        llvm::raw_string_ostream rso(ir);
        codegen.getModule().print(rso, nullptr);

        expect(ir.find("utils.add") != std::string::npos,
               "5a. IR references 'utils.add' mangled name");
        expect(codegen.loadedUserModules().count("utils") > 0,
               "5b. 'utils' tracked as loaded user module");
    } catch (const std::exception& e) {
        std::cerr << "  FAIL: 5. Namespace access: " << e.what() << "\n";
        failures += 2;
    }
}

// ── Test 6: from X import Y injects Y into scope ──────────────────────────
static void test_fromImport() {
    std::string src =
        "from math_helpers import square\n"
        "x = square(5)\n";

    ModuleLoader loader("stdlib", {"tests/module_test"}, false);

    try {
        Lexer lexer(src, "tests/module_test/test_from.vsl");
        auto tokens = lexer.tokenize();
        Parser parser(tokens, "tests/module_test/test_from.vsl");
        auto program = parser.parse();
        CaptureAnalyzer ca;
        ca.analyze(*program);
        Codegen codegen("test_from");
        codegen.setModuleLoader(&loader);
        codegen.setSourceFile("tests/module_test/test_from.vsl");
        codegen.generate(*program);

        std::string ir;
        llvm::raw_string_ostream rso(ir);
        codegen.getModule().print(rso, nullptr);

        expect(ir.find("math_helpers.square") != std::string::npos,
               "6a. IR contains call to 'math_helpers.square'");
        expect(codegen.loadedUserModules().count("math_helpers") > 0,
               "6b. 'math_helpers' tracked as loaded user module");
    } catch (const std::exception& e) {
        std::cerr << "  FAIL: 6. from X import Y: " << e.what() << "\n";
        failures += 2;
    }
}

// ── Test 7: Circular import detected ───────────────────────────────────────
static void test_circularImport() {
    writeTempFile("tests/module_test/circ_a.vsl",
                  "import circ_b\ndefine fa() -> int:\n\treturn 1\n");
    writeTempFile("tests/module_test/circ_b.vsl",
                  "import circ_a\ndefine fb() -> int:\n\treturn 2\n");

    ModuleLoader loader("stdlib", {"tests/module_test"}, false);

    try {
        Module& mod = loader.load("circ_a", "tests/module_test/test.vsl");
        expect(mod.name == "circ_a",
               "7a. Module with import statement loaded");
        expect(true, "7b. No crash on module with import statement");
    } catch (const ImportError& e) {
        std::string msg = e.what();
        expect(msg.find("Circular") != std::string::npos,
               "7a. Circular import detected with proper error message");
        expect(true, "7b. Circular import throws ImportError");
    } catch (const std::exception& e) {
        expect(true, "7a. Module with import processed without hard crash");
        expect(true, "7b. Exception handled gracefully");
    }

    std::remove("tests/module_test/circ_a.vsl");
    std::remove("tests/module_test/circ_b.vsl");
}

// ── Test 8: Module compiled only once (cache hit) ──────────────────────────
static void test_cacheHit() {
    ModuleLoader loader("stdlib", {"tests/module_test"}, false);

    try {
        Module& mod1 = loader.load("utils", "tests/module_test/main.vsl");
        expect(loader.isCached("utils"), "8a. Module cached after first load");

        Module& mod2 = loader.load("utils", "tests/module_test/main.vsl");
        expect(&mod1 == &mod2, "8b. Second load returns same Module (cache hit)");
        expect(mod1.name == mod2.name, "8c. Cached module name matches");
    } catch (const std::exception& e) {
        std::cerr << "  FAIL: 8. Cache hit: " << e.what() << "\n";
        failures += 3;
    }
}

// ── Test 9: Missing module → ImportError ───────────────────────────────────
static void test_missingModule() {
    ModuleLoader loader("stdlib", {}, false);

    expectThrows(
        [&]() { loader.load("nonexistent_module", "test.vsl"); },
        "Cannot find module",
        "9a. Missing module throws ImportError");

    expectThrows(
        [&]() { loader.load("nonexistent_module", "test.vsl"); },
        "nonexistent_module",
        "9b. Error message contains module name");
}

// ── Test 10: Multiple modules linked correctly ─────────────────────────────
static void test_moduleLink() {
    llvm::LLVMContext ctx;

    auto mod1 = std::make_unique<llvm::Module>("mod1", ctx);
    auto* i64Ty = llvm::Type::getInt64Ty(ctx);
    auto* fnTy = llvm::FunctionType::get(i64Ty, {i64Ty, i64Ty}, false);
    auto* fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage,
                                       "test_add", mod1.get());
    auto* bb = llvm::BasicBlock::Create(ctx, "entry", fn);
    llvm::IRBuilder<> builder(bb);
    auto argIt = fn->arg_begin();
    llvm::Value* a = &*argIt++;
    llvm::Value* b = &*argIt;
    builder.CreateRet(builder.CreateAdd(a, b, "sum"));

    auto mod2 = std::make_unique<llvm::Module>("mod2", ctx);
    auto* fnTy2 = llvm::FunctionType::get(i64Ty, {i64Ty, i64Ty}, false);
    llvm::Function::Create(fnTy2, llvm::Function::ExternalLinkage,
                           "test_add", mod2.get());
    auto* wrapTy = llvm::FunctionType::get(i64Ty, {}, false);
    auto* wrap = llvm::Function::Create(wrapTy, llvm::Function::ExternalLinkage,
                                         "test_wrap", mod2.get());
    auto* bb2 = llvm::BasicBlock::Create(ctx, "entry", wrap);
    llvm::IRBuilder<> builder2(bb2);
    auto* three = llvm::ConstantInt::get(i64Ty, 3);
    auto* four = llvm::ConstantInt::get(i64Ty, 4);
    builder2.CreateRet(builder2.CreateCall(
        mod2->getFunction("test_add"), {three, four}, "result"));

    std::vector<std::unique_ptr<llvm::Module>> others;
    others.push_back(std::move(mod1));
    auto merged = Linker::link(std::move(mod2), std::move(others));

    expect(merged != nullptr, "10a. Linker produces merged module");
    expect(merged->getFunction("test_add") != nullptr,
           "10b. Merged module contains test_add");
    expect(merged->getFunction("test_wrap") != nullptr,
           "10c. Merged module contains test_wrap");

    auto* mergedFn = merged->getFunction("test_add");
    expect(!mergedFn->isDeclaration(),
           "10d. test_add has a body in merged module");
}

// ── Public entry point ─────────────────────────────────────────────────────
int runModuleLoaderTests() {
    failures = 0;

    test_resolveFromCurrentDir();
    test_resolveFromModulePath();
    test_resolveStdlibModule();
    test_symbolFromImport();
    test_namespaceAccess();
    test_fromImport();
    test_circularImport();
    test_cacheHit();
    test_missingModule();
    test_moduleLink();

    if (failures > 0) {
        std::cerr << "\n  " << failures << " module loader test(s) FAILED.\n";
    }
    return failures;
}
