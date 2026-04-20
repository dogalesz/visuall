#pragma once

#include "ast.h"
#include "builtins.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace visuall {

// Forward declarations
class ModuleLoader;
struct Module;

// ════════════════════════════════════════════════════════════════════════════
// CodegenError — thrown on all code-generation errors.
// Format: "CodegenError: [message] at [line]:[col]"
// ════════════════════════════════════════════════════════════════════════════
class CodegenError : public std::runtime_error {
public:
    CodegenError(const std::string& msg, int line, int col)
        : std::runtime_error("CodegenError: " + msg + " at " +
                             std::to_string(line) + ":" + std::to_string(col)),
          line_(line), col_(col) {}
    int line() const { return line_; }
    int col()  const { return col_; }
private:
    int line_;
    int col_;
};

// ════════════════════════════════════════════════════════════════════════════
// Codegen — LLVM IR code generator for the Visuall AST.
// ════════════════════════════════════════════════════════════════════════════
class Codegen {
public:
    explicit Codegen(const std::string& moduleName);

    /// Walk the entire program AST and emit LLVM IR.
    void generate(const ast::Program& program);

    /// Run LLVM optimization passes (mem2reg, instcombine, etc.).
    void optimize();

    /// Print the LLVM IR to a C++ ostream.
    void printIR(std::ostream& os) const;

    /// Write the LLVM IR (.ll) to a file.
    void writeIRToFile(const std::string& path) const;

    /// Emit a native object file (.o) via LLVM TargetMachine.
    void emitObjectFile(const std::string& path) const;

    /// Link an object file into a final binary using the system linker.
    static int linkToBinary(const std::string& objPath,
                            const std::string& outPath);

    /// Convenience: write IR, emit object, link — all in one call.
    void compileToNative(const std::string& basePath) const;

    // ── Module system ───────────────────────────────────────────────────
    /// Set the module loader for resolving imports.
    void setModuleLoader(ModuleLoader* loader) { moduleLoader_ = loader; }

    /// Set the source file path (for module resolution context).
    void setSourceFile(const std::string& path) { sourceFile_ = path; }

    /// Enable module mode: skip main() creation, only compile functions.
    void setModuleMode(bool mode) { moduleMode_ = mode; }

    /// Get the set of loaded user modules (for linking).
    const std::unordered_set<std::string>& loadedUserModules() const {
        return loadedUserModules_;
    }

    /// Enable GC statistics reporting at shutdown.
    void setGCStats(bool enabled) { gcStatsEnabled_ = enabled; }

    /// Transfer ownership of the LLVM module out.
    std::unique_ptr<llvm::Module> takeModule() { return std::move(module_); }

    /// Transfer ownership of the LLVM context out (call after takeModule).
    std::unique_ptr<llvm::LLVMContext> takeContext() { return std::move(context_); }

    // Accessors for testing
    llvm::Module&      getModule()  const { return *module_; }
    llvm::LLVMContext& getContext() const { return *context_; }

private:
    std::unique_ptr<llvm::LLVMContext> context_;
    std::unique_ptr<llvm::Module>      module_;
    std::unique_ptr<llvm::IRBuilder<>> builder_;
    bool moduleMode_ = false;
    bool gcStatsEnabled_ = false;

    // ── Symbol table ────────────────────────────────────────────────────
    // Scope stack: each scope is a map from name → alloca.
    std::vector<std::unordered_map<std::string, llvm::AllocaInst*>> scopes_;
    void pushScope();
    void popScope();
    void declareVar(const std::string& name, llvm::AllocaInst* alloca);
    llvm::AllocaInst* lookupVar(const std::string& name) const;

    // Current function being generated
    llvm::Function* currentFunction_ = nullptr;

    // ── Loop control ────────────────────────────────────────────────────
    // Stack of (loopHeader, loopExit) blocks for break/continue.
    std::vector<std::pair<llvm::BasicBlock*, llvm::BasicBlock*>> loopStack_;

    // ── Class context ───────────────────────────────────────────────────
    std::string currentClassName_;
    // Maps className → ordered list of field names (discovered from init body)
    std::unordered_map<std::string, std::vector<std::string>> classFields_;

    // ── Generic function store ──────────────────────────────────────────
    std::unordered_map<std::string, const ast::FuncDef*> genericFuncDefs_;
    std::unordered_set<std::string> emittedMonomorphizations_;

    // ── Closure support ───────────────────────────────────────────────
    // Track which variables hold closures (fat pointers) so call sites
    // know to indirect through fn_ptr(env, args...).
    std::unordered_set<std::string> closureVars_;

    // The closure struct type: { i8* env, i8* fn_ptr }
    llvm::StructType* getClosureType();

    // Declare malloc if not already declared.
    llvm::Function* getOrDeclareMalloc();

    // Declare __visuall_alloc if not already declared.
    llvm::Function* getOrDeclareVisualAlloc();

    // ── Codegen methods for statements ──────────────────────────────────
    void codegenStmt(const ast::Stmt& stmt);
    void codegenStmtList(const ast::StmtList& stmts);
    void codegenFuncDef(const ast::FuncDef& node);
    void codegenClassDef(const ast::ClassDef& node);
    void codegenInitDef(const ast::InitDef& node);
    void codegenIfStmt(const ast::IfStmt& node);
    void codegenForStmt(const ast::ForStmt& node);
    void codegenWhileStmt(const ast::WhileStmt& node);
    void codegenReturnStmt(const ast::ReturnStmt& node);
    void codegenTryStmt(const ast::TryStmt& node);
    void codegenThrowStmt(const ast::ThrowStmt& node);
    void codegenImportStmt(const ast::ImportStmt& node);
    void codegenFromImportStmt(const ast::FromImportStmt& node);
    void codegenAssignStmt(const ast::AssignStmt& node);
    void codegenExprStmt(const ast::ExprStmt& node);
    void codegenTupleUnpackStmt(const ast::TupleUnpackStmt& node);
    void codegenInterfaceDef(const ast::InterfaceDef& node);

    // ── Codegen methods for expressions ─────────────────────────────────
    llvm::Value* codegenExpr(const ast::Expr& expr);
    llvm::Value* codegenIntLiteral(const ast::IntLiteral& node);
    llvm::Value* codegenFloatLiteral(const ast::FloatLiteral& node);
    llvm::Value* codegenStringLiteral(const ast::StringLiteral& node);
    llvm::Value* codegenFStringLiteral(const ast::FStringLiteral& node);
    llvm::Value* codegenFStringExpr(const ast::FStringExpr& node);
    llvm::Value* codegenBoolLiteral(const ast::BoolLiteral& node);
    llvm::Value* codegenNullLiteral(const ast::NullLiteral& node);
    llvm::Value* codegenIdentifier(const ast::Identifier& node);
    llvm::Value* codegenBinaryExpr(const ast::BinaryExpr& node);
    llvm::Value* codegenUnaryExpr(const ast::UnaryExpr& node);
    llvm::Value* codegenCallExpr(const ast::CallExpr& node);
    llvm::Value* codegenMemberExpr(const ast::MemberExpr& node);
    llvm::Value* codegenIndexExpr(const ast::IndexExpr& node);
    llvm::Value* codegenLambdaExpr(const ast::LambdaExpr& node);
    llvm::Value* codegenListExpr(const ast::ListExpr& node);
    llvm::Value* codegenDictExpr(const ast::DictExpr& node);
    llvm::Value* codegenTupleExpr(const ast::TupleExpr& node);
    llvm::Value* codegenTernaryExpr(const ast::TernaryExpr& node);
    llvm::Value* codegenSliceExpr(const ast::SliceExpr& node);
    llvm::Value* codegenListComprehension(const ast::ListComprehension& node);
    llvm::Value* codegenDictComprehension(const ast::DictComprehension& node);
    llvm::Value* codegenSpreadExpr(const ast::SpreadExpr& node);

    void emitMonomorphization(const ast::FuncDef* def,
                               const std::vector<std::string>& typeArgs,
                               const std::string& mangledName);

    // ── Helpers ─────────────────────────────────────────────────────────
    llvm::AllocaInst* createEntryBlockAlloca(llvm::Function* fn,
                                              const std::string& name,
                                              llvm::Type* type);
    llvm::Type* getLLVMType(const std::string& typeName);
    llvm::Value* promoteToDouble(llvm::Value* v);
    llvm::Value* toBool(llvm::Value* v);

    void declarePrintfAndBuiltins();
    llvm::Value* emitPrintCall(const ast::CallExpr& node);
    llvm::Value* emitBuiltinCall(const std::string& name, const ast::CallExpr& node);
    llvm::Value* emitModuleCall(const std::string& moduleName,
                                 const std::string& funcName,
                                 const ast::CallExpr& node);
    llvm::Value* emitIntPow(llvm::Value* base, llvm::Value* exp);

    // ── Imported modules ────────────────────────────────────────────────
    std::unordered_set<std::string> importedModules_;

    // ── Module system ───────────────────────────────────────────────────
    ModuleLoader* moduleLoader_ = nullptr;
    std::string   sourceFile_;
    // User modules that were loaded via import (for linking).
    std::unordered_set<std::string> loadedUserModules_;
    // Module exports injected into scope via "from X import Y":
    // maps local name → {module, symbolName}
    std::unordered_map<std::string, std::pair<std::string, std::string>>
        importedSymbols_;
};

} // namespace visuall
