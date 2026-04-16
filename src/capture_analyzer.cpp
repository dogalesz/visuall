#include "capture_analyzer.h"
#include <algorithm>

namespace visuall {

// ════════════════════════════════════════════════════════════════════════════
// Scope management
// ════════════════════════════════════════════════════════════════════════════

void CaptureAnalyzer::pushScope() {
    scopes_.emplace_back();
}

void CaptureAnalyzer::popScope() {
    if (!scopes_.empty()) scopes_.pop_back();
}

void CaptureAnalyzer::define(const std::string& name) {
    if (!scopes_.empty()) {
        scopes_.back().insert(name);
    }
}

bool CaptureAnalyzer::isDefined(const std::string& name) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        if (it->count(name)) return true;
    }
    return false;
}

// ════════════════════════════════════════════════════════════════════════════
// Entry point
// ════════════════════════════════════════════════════════════════════════════

void CaptureAnalyzer::analyze(ast::Program& program) {
    pushScope();

    // Built-in names that should never be captured.
    define("print");

    // First pass: define all top-level function names so they are not
    // treated as free variables.
    for (auto& stmt : program.statements) {
        if (auto* f = dynamic_cast<ast::FuncDef*>(stmt.get())) {
            define(f->name);
        }
        if (auto* c = dynamic_cast<ast::ClassDef*>(stmt.get())) {
            define(c->name);
        }
    }

    analyzeStmtList(program.statements);
    popScope();
}

// ════════════════════════════════════════════════════════════════════════════
// Statement walkers
// ════════════════════════════════════════════════════════════════════════════

void CaptureAnalyzer::analyzeStmtList(ast::StmtList& stmts) {
    for (auto& s : stmts) {
        analyzeStmt(*s);
    }
}

void CaptureAnalyzer::analyzeStmt(ast::Stmt& stmt) {
    // ── AssignStmt ─────────────────────────────────────────────────────
    if (auto* s = dynamic_cast<ast::AssignStmt*>(&stmt)) {
        // Define the LHS variable FIRST so that code like `x = 10` makes
        // x available for subsequent lambdas.
        if (auto* id = dynamic_cast<ast::Identifier*>(s->target.get())) {
            define(id->name);
        }

        // Walk the RHS expression.
        std::unordered_set<std::string> fv;
        analyzeExpr(*s->value, fv);
        return;
    }

    // ── ExprStmt ───────────────────────────────────────────────────────
    if (auto* s = dynamic_cast<ast::ExprStmt*>(&stmt)) {
        std::unordered_set<std::string> fv;
        analyzeExpr(*s->expr, fv);
        return;
    }

    // ── FuncDef ────────────────────────────────────────────────────────
    if (auto* s = dynamic_cast<ast::FuncDef*>(&stmt)) {
        define(s->name);
        pushScope();
        for (const auto& p : s->params) {
            define(p.name);
        }
        analyzeStmtList(s->body);
        popScope();
        return;
    }

    // ── ClassDef ───────────────────────────────────────────────────────
    if (auto* s = dynamic_cast<ast::ClassDef*>(&stmt)) {
        define(s->name);
        pushScope();
        define("this");
        define("super");
        analyzeStmtList(s->body);
        popScope();
        return;
    }

    // ── InitDef ────────────────────────────────────────────────────────
    if (auto* s = dynamic_cast<ast::InitDef*>(&stmt)) {
        pushScope();
        for (const auto& p : s->params) {
            define(p.name);
        }
        analyzeStmtList(s->body);
        popScope();
        return;
    }

    // ── IfStmt ─────────────────────────────────────────────────────────
    if (auto* s = dynamic_cast<ast::IfStmt*>(&stmt)) {
        std::unordered_set<std::string> fv;
        analyzeExpr(*s->condition, fv);
        pushScope();
        analyzeStmtList(s->thenBranch);
        popScope();
        for (auto& [cond, body] : s->elsifBranches) {
            analyzeExpr(*cond, fv);
            pushScope();
            analyzeStmtList(body);
            popScope();
        }
        if (!s->elseBranch.empty()) {
            pushScope();
            analyzeStmtList(s->elseBranch);
            popScope();
        }
        return;
    }

    // ── ForStmt ────────────────────────────────────────────────────────
    if (auto* s = dynamic_cast<ast::ForStmt*>(&stmt)) {
        std::unordered_set<std::string> fv;
        analyzeExpr(*s->iterable, fv);
        pushScope();
        define(s->variable);
        analyzeStmtList(s->body);
        popScope();
        return;
    }

    // ── WhileStmt ──────────────────────────────────────────────────────
    if (auto* s = dynamic_cast<ast::WhileStmt*>(&stmt)) {
        std::unordered_set<std::string> fv;
        analyzeExpr(*s->condition, fv);
        pushScope();
        analyzeStmtList(s->body);
        popScope();
        return;
    }

    // ── ReturnStmt ─────────────────────────────────────────────────────
    if (auto* s = dynamic_cast<ast::ReturnStmt*>(&stmt)) {
        if (s->value) {
            std::unordered_set<std::string> fv;
            analyzeExpr(*s->value, fv);
        }
        return;
    }

    // ── TryStmt ────────────────────────────────────────────────────────
    if (auto* s = dynamic_cast<ast::TryStmt*>(&stmt)) {
        pushScope();
        analyzeStmtList(s->tryBody);
        popScope();
        for (auto& c : s->catchClauses) {
            pushScope();
            if (!c.varName.empty()) define(c.varName);
            analyzeStmtList(c.body);
            popScope();
        }
        if (!s->finallyBody.empty()) {
            pushScope();
            analyzeStmtList(s->finallyBody);
            popScope();
        }
        return;
    }

    // ── ThrowStmt ──────────────────────────────────────────────────────
    if (auto* s = dynamic_cast<ast::ThrowStmt*>(&stmt)) {
        std::unordered_set<std::string> fv;
        analyzeExpr(*s->expr, fv);
        return;
    }

    // ── TupleUnpackStmt ────────────────────────────────────────────────
    if (auto* s = dynamic_cast<ast::TupleUnpackStmt*>(&stmt)) {
        std::unordered_set<std::string> fv;
        analyzeExpr(*s->value, fv);
        for (const auto& t : s->targets) {
            define(t);
        }
        return;
    }

    // Other statements (break, continue, pass, import, interface) — nothing to do.
}

// ════════════════════════════════════════════════════════════════════════════
// Expression walkers — collect ALL referenced identifiers in usedVars.
// The lambda handler subtracts its own params and keeps only those that
// come from enclosing scopes.
// ════════════════════════════════════════════════════════════════════════════

void CaptureAnalyzer::analyzeExpr(ast::Expr& expr,
                                   std::unordered_set<std::string>& freeVars) {
    // ── Identifier ─────────────────────────────────────────────────────
    if (auto* e = dynamic_cast<ast::Identifier*>(&expr)) {
        // Always report the identifier — the lambda handler will decide
        // whether it's a capture.
        freeVars.insert(e->name);
        return;
    }

    // ── LambdaExpr — this is where capture analysis happens ────────────
    if (auto* e = dynamic_cast<ast::LambdaExpr*>(&expr)) {
        // Collect all identifiers used in the lambda body.
        std::unordered_set<std::string> bodyVars;
        analyzeExpr(*e->body, bodyVars);

        // Remove the lambda's own parameters — they aren't captures.
        for (const auto& p : e->params) {
            bodyVars.erase(p);
        }

        // What remains in bodyVars are free variables of this lambda.
        // If they are defined in an enclosing scope, add them as captures.
        // If not, propagate them upward so an outer lambda can capture them.
        e->captures.clear();
        for (const auto& name : bodyVars) {
            if (isDefined(name)) {
                ast::CaptureInfo cap;
                cap.name = name;
                cap.byReference = false;
                e->captures.push_back(cap);
            } else {
                // Not defined anywhere — propagate upward.
                freeVars.insert(name);
            }
        }

        // Sort captures for deterministic struct layout.
        std::sort(e->captures.begin(), e->captures.end(),
                  [](const ast::CaptureInfo& a, const ast::CaptureInfo& b) {
                      return a.name < b.name;
                  });

        return;
    }

    // ── BinaryExpr ─────────────────────────────────────────────────────
    if (auto* e = dynamic_cast<ast::BinaryExpr*>(&expr)) {
        analyzeExpr(*e->left, freeVars);
        analyzeExpr(*e->right, freeVars);
        return;
    }

    // ── UnaryExpr ──────────────────────────────────────────────────────
    if (auto* e = dynamic_cast<ast::UnaryExpr*>(&expr)) {
        analyzeExpr(*e->operand, freeVars);
        return;
    }

    // ── CallExpr ───────────────────────────────────────────────────────
    if (auto* e = dynamic_cast<ast::CallExpr*>(&expr)) {
        analyzeExpr(*e->callee, freeVars);
        for (auto& arg : e->args) {
            analyzeExpr(*arg, freeVars);
        }
        return;
    }

    // ── MemberExpr ─────────────────────────────────────────────────────
    if (auto* e = dynamic_cast<ast::MemberExpr*>(&expr)) {
        analyzeExpr(*e->object, freeVars);
        return;
    }

    // ── IndexExpr ──────────────────────────────────────────────────────
    if (auto* e = dynamic_cast<ast::IndexExpr*>(&expr)) {
        analyzeExpr(*e->object, freeVars);
        analyzeExpr(*e->index, freeVars);
        return;
    }

    // ── TernaryExpr ────────────────────────────────────────────────────
    if (auto* e = dynamic_cast<ast::TernaryExpr*>(&expr)) {
        analyzeExpr(*e->condition, freeVars);
        analyzeExpr(*e->thenExpr, freeVars);
        analyzeExpr(*e->elseExpr, freeVars);
        return;
    }

    // ── ListExpr ───────────────────────────────────────────────────────
    if (auto* e = dynamic_cast<ast::ListExpr*>(&expr)) {
        for (auto& elem : e->elements) analyzeExpr(*elem, freeVars);
        return;
    }

    // ── DictExpr ───────────────────────────────────────────────────────
    if (auto* e = dynamic_cast<ast::DictExpr*>(&expr)) {
        for (auto& [k, v] : e->entries) {
            analyzeExpr(*k, freeVars);
            analyzeExpr(*v, freeVars);
        }
        return;
    }

    // ── TupleExpr ──────────────────────────────────────────────────────
    if (auto* e = dynamic_cast<ast::TupleExpr*>(&expr)) {
        for (auto& elem : e->elements) analyzeExpr(*elem, freeVars);
        return;
    }

    // ── SpreadExpr ─────────────────────────────────────────────────────
    if (auto* e = dynamic_cast<ast::SpreadExpr*>(&expr)) {
        analyzeExpr(*e->operand, freeVars);
        return;
    }

    // ── ListComprehension ──────────────────────────────────────────────
    if (auto* e = dynamic_cast<ast::ListComprehension*>(&expr)) {
        analyzeExpr(*e->iterable, freeVars);
        // The comprehension variable is local — remove it after collecting
        std::unordered_set<std::string> bodyVars;
        analyzeExpr(*e->body, bodyVars);
        if (e->condition) analyzeExpr(*e->condition, bodyVars);
        bodyVars.erase(e->variable);
        freeVars.insert(bodyVars.begin(), bodyVars.end());
        return;
    }

    // ── DictComprehension ──────────────────────────────────────────────
    if (auto* e = dynamic_cast<ast::DictComprehension*>(&expr)) {
        analyzeExpr(*e->iterable, freeVars);
        std::unordered_set<std::string> bodyVars;
        analyzeExpr(*e->key, bodyVars);
        analyzeExpr(*e->value, bodyVars);
        if (e->condition) analyzeExpr(*e->condition, bodyVars);
        bodyVars.erase(e->variable);
        freeVars.insert(bodyVars.begin(), bodyVars.end());
        return;
    }

    // ── SliceExpr ──────────────────────────────────────────────────────
    if (auto* e = dynamic_cast<ast::SliceExpr*>(&expr)) {
        analyzeExpr(*e->object, freeVars);
        if (e->start) analyzeExpr(*e->start, freeVars);
        if (e->stop) analyzeExpr(*e->stop, freeVars);
        if (e->step) analyzeExpr(*e->step, freeVars);
        return;
    }

    // Literals, ThisExpr, SuperExpr, NullLiteral etc. — no free variables.
}

void CaptureAnalyzer::collectAssignedNames(const ast::Expr& /*expr*/,
                                            std::unordered_set<std::string>& /*assigned*/) {
    // Reserved for future mutable-capture detection.
}

} // namespace visuall
