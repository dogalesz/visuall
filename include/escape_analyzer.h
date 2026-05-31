#pragma once

#include "ast_visitor.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace visuall {

// ════════════════════════════════════════════════════════════════════════════
// EscapeAnalyzer — determines which allocation sites can be stack-allocated.
//
// Runs post-typecheck, pre-codegen.  For each allocation site (class ctor,
// ListExpr, DictExpr, TupleExpr, LambdaExpr with captures), decides whether
// the allocated value escapes the enclosing function.
//
// Constraints applied (see plan Traps 1, 2, 8):
//   1. Allocations inside loops always escape (stack-overflow prevention).
//   2. Allocations inside yielding functions (goroutines with channel ops)
//      always escape (CPS state machine destroys the physical stack frame).
//   3. Escaping via return, global store, field store, unknown-call argument,
//      or closure capture forces heap allocation.
//
// Usage:
//   EscapeAnalyzer ea;
//   ea.analyze(program);
//   codegen.setEscapeInfo(&ea.stackAllocatable());
// ════════════════════════════════════════════════════════════════════════════
class EscapeAnalyzer : public ASTVisitorBase {
public:
    void analyze(const ast::Program& program);

    /// Map from allocation-site AST node pointer → true if stack-allocatable.
    const std::unordered_map<const void*, bool>& stackAllocatable() const {
        return stackAllocatable_;
    }

private:
    // ── Result ──────────────────────────────────────────────────────────
    std::unordered_map<const void*, bool> stackAllocatable_;

    // ── Traversal state ─────────────────────────────────────────────────
    int loopDepth_ = 0;     // > 0 when inside a for/while body

    // Per-function analysis: variable name → set of allocation-site pointers
    // that may be reachable through that variable.
    struct VarInfo {
        const ast::Expr* sourceSite = nullptr; // allocation site this var points to
        bool escaped = false;                   // already determined to escape
    };
    std::unordered_map<std::string, VarInfo> varMap_;

    // Set of variable names that escape the current function.
    std::unordered_set<std::string> escapedVars_;

    // If true, the current function contains channel operations or is a
    // goroutine entry point — all allocations inside it must be heap.
    bool isYieldingFunction_ = false;

    // ── AST walk ────────────────────────────────────────────────────────
    void analyzeStmtList(const ast::StmtList& stmts);
    void analyzeStmt(const ast::Stmt& stmt);
    void analyzeExpr(const ast::Expr& expr, std::string* assignedVar = nullptr);

    void registerAlloc(const ast::Expr& site, const std::string& varName);
    void markEscaped(const std::string& varName);

    // Visitor overrides
    void visit(const ast::FuncDef& n) override;
    void visit(const ast::ClassDef& n) override;
    void visit(const ast::IfStmt& n) override;
    void visit(const ast::ForStmt& n) override;
    void visit(const ast::WhileStmt& n) override;
    void visit(const ast::ReturnStmt& n) override;
    void visit(const ast::AssignStmt& n) override;
    void visit(const ast::ExprStmt& n) override;
    void visit(const ast::CallExpr& n) override;
    void visit(const ast::ListExpr& n) override;
    void visit(const ast::DictExpr& n) override;
    void visit(const ast::TupleExpr& n) override;
    void visit(const ast::LambdaExpr& n) override;
    void visit(const ast::MemberExpr& n) override;
    void visit(const ast::ThrowStmt& n) override;
    void visit(const ast::TryStmt& n) override;
    void visit(const ast::MatchStmt& n) override;
};

} // namespace visuall
