#pragma once

#include "ast.h"
#include <string>
#include <unordered_set>
#include <vector>

namespace visuall {

// ════════════════════════════════════════════════════════════════════════════
// CaptureAnalyzer — walks the AST and annotates each LambdaExpr with its
// capture list (free variables referenced from enclosing scopes).
//
// Must be run BEFORE codegen.  Populates LambdaExpr::captures.
// ════════════════════════════════════════════════════════════════════════════
class CaptureAnalyzer {
public:
    /// Analyze the entire program.
    void analyze(ast::Program& program);

private:
    /// Scope stack: each entry is the set of names defined in that scope.
    std::vector<std::unordered_set<std::string>> scopes_;

    void pushScope();
    void popScope();
    void define(const std::string& name);
    bool isDefined(const std::string& name) const;

    // ── AST walkers ────────────────────────────────────────────────────
    void analyzeStmt(ast::Stmt& stmt);
    void analyzeStmtList(ast::StmtList& stmts);
    void analyzeExpr(ast::Expr& expr,
                     std::unordered_set<std::string>& freeVars);

    /// Collect all identifiers that are assigned to inside an expression tree.
    /// (For detecting mutable captures in lambda bodies that contain assignments.)
    void collectAssignedNames(const ast::Expr& expr,
                              std::unordered_set<std::string>& assigned);
};

} // namespace visuall
