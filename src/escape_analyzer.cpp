#include "escape_analyzer.h"
#include <stdexcept>

namespace visuall {

// ════════════════════════════════════════════════════════════════════════════
// Public entry point
// ════════════════════════════════════════════════════════════════════════════

void EscapeAnalyzer::analyze(const ast::Program& program) {
    stackAllocatable_.clear();

    for (const auto& stmt : program.statements) {
        if (auto* fd = dynamic_cast<ast::FuncDef*>(stmt.get())) {
            fd->accept(*this);
        }
    }

    // Top-level allocations always escape (global scope).
    // Nothing to mark as stack-allocatable from the top level.
}

// ════════════════════════════════════════════════════════════════════════════
// Variable tracking helpers
// ════════════════════════════════════════════════════════════════════════════

void EscapeAnalyzer::registerAlloc(const ast::Expr& site, const std::string& varName) {
    if (varName.empty()) return;

    if (loopDepth_ > 0 || isYieldingFunction_) {
        // Traps 1 + 8: never stack-allocate inside loops or yielding functions
        stackAllocatable_[&site] = false;
        markEscaped(varName);
        return;
    }

    VarInfo info;
    info.sourceSite = &site;
    varMap_[varName] = info;

    // Optimistically mark as stack-allocatable; will be demoted if later
    // analysis shows the variable escapes.
    stackAllocatable_[&site] = true;
}

void EscapeAnalyzer::markEscaped(const std::string& varName) {
    if (varName.empty()) return;
    escapedVars_.insert(varName);

    auto it = varMap_.find(varName);
    if (it != varMap_.end() && it->second.sourceSite) {
        stackAllocatable_[it->second.sourceSite] = false;
        it->second.escaped = true;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Statement analysis
// ════════════════════════════════════════════════════════════════════════════

void EscapeAnalyzer::analyzeStmtList(const ast::StmtList& stmts) {
    for (const auto& stmt : stmts) {
        if (stmt) stmt->accept(*this);
    }
}

void EscapeAnalyzer::analyzeStmt(const ast::Stmt& stmt) {
    stmt.accept(*this);
}

// ════════════════════════════════════════════════════════════════════════════
// Expression analysis — returns nullptr; callers use assignedVar for tracking
// ════════════════════════════════════════════════════════════════════════════

void EscapeAnalyzer::analyzeExpr(const ast::Expr& expr, std::string* assignedVar) {
    expr.accept(*this);
    (void)assignedVar; // reserved for future interprocedural analysis
}

// ════════════════════════════════════════════════════════════════════════════
// Visitor overrides — Statement nodes
// ════════════════════════════════════════════════════════════════════════════

void EscapeAnalyzer::visit(const ast::FuncDef& n) {
    // Save per-function state
    auto savedVars = std::move(varMap_);
    auto savedEscaped = std::move(escapedVars_);
    auto savedYielding = isYieldingFunction_;
    varMap_.clear();
    escapedVars_.clear();

    analyzeStmtList(n.body);

    // After analysis: any allocation that was NOT marked as escaped
    // stays stack-allocatable. Those already marked false in the map
    // are escaped. We don't need to do more here — the registerAlloc
    // calls already set the optimistic true/false values.

    // Restore
    varMap_ = std::move(savedVars);
    escapedVars_ = std::move(savedEscaped);
    isYieldingFunction_ = savedYielding;
}

void EscapeAnalyzer::visit(const ast::ClassDef& n) {
    // Methods are their own functions — analyze each
    for (const auto& stmt : n.body) {
        if (auto* fd = dynamic_cast<ast::FuncDef*>(stmt.get())) {
            fd->accept(*this);
        } else if (auto* init = dynamic_cast<ast::InitDef*>(stmt.get())) {
            init->accept(*this);
        }
    }
}

void EscapeAnalyzer::visit(const ast::IfStmt& n) {
    analyzeExpr(*n.condition);
    analyzeStmtList(n.thenBranch);
    for (const auto& elsif : n.elsifBranches) {
        analyzeExpr(*elsif.first);
        analyzeStmtList(elsif.second);
    }
    analyzeStmtList(n.elseBranch);
}

void EscapeAnalyzer::visit(const ast::ForStmt& n) {
    analyzeExpr(*n.iterable);
    loopDepth_++;
    analyzeStmtList(n.body);
    loopDepth_--;
    analyzeStmtList(n.elseBranch);
}

void EscapeAnalyzer::visit(const ast::WhileStmt& n) {
    analyzeExpr(*n.condition);
    loopDepth_++;
    analyzeStmtList(n.body);
    loopDepth_--;
    analyzeStmtList(n.elseBranch);
}

void EscapeAnalyzer::visit(const ast::ReturnStmt& n) {
    if (n.value) {
        // If the returned expression is a simple identifier, mark it as escaped.
        if (auto* id = dynamic_cast<ast::Identifier*>(n.value.get())) {
            markEscaped(id->name);
        } else {
            // Complex expression — conservatively scan for identifiers that
            // might reference tracked allocations, and mark them escaped.
            // For now, just mark any direct identifier in the expression.
            n.value->accept(*this);
        }
        analyzeExpr(*n.value);
    }
}

void EscapeAnalyzer::visit(const ast::AssignStmt& n) {
    // Analyze the RHS first to detect allocation sites
    std::string lhsName;
    if (auto* id = dynamic_cast<ast::Identifier*>(n.target.get())) {
        lhsName = id->name;
    }

    // For RHS that is a direct allocation (list, dict, tuple, constructor call),
    // register it under the LHS variable name.
    if (dynamic_cast<ast::ListExpr*>(n.value.get()) ||
        dynamic_cast<ast::DictExpr*>(n.value.get()) ||
        dynamic_cast<ast::TupleExpr*>(n.value.get())) {
        analyzeExpr(*n.value);
        registerAlloc(*n.value, lhsName);
    } else if (auto* call = dynamic_cast<ast::CallExpr*>(n.value.get())) {
        // Constructor call — treat as allocation site
        analyzeExpr(*n.value);
        registerAlloc(*n.value, lhsName);
    } else if (auto* lambda = dynamic_cast<ast::LambdaExpr*>(n.value.get())) {
        // Lambda with captures is an allocation site
        analyzeExpr(*n.value);
        if (!lambda->captures.empty()) {
            registerAlloc(*n.value, lhsName);
        }
    } else if (auto* id = dynamic_cast<ast::Identifier*>(n.value.get())) {
        // Assignment from another variable — propagate tracking
        analyzeExpr(*n.value);
        auto it = varMap_.find(id->name);
        if (it != varMap_.end()) {
            varMap_[lhsName] = it->second;
        }
    } else {
        // Complex RHS — analyze but don't register as allocation
        analyzeExpr(*n.value);
    }

    // Handle chained assignment: a = b = [1,2,3]
    // The RHS allocation is already registered for the first target;
    // propagate to extra targets.
    for (const auto& extra : n.extraTargets) {
        if (auto* eid = dynamic_cast<ast::Identifier*>(extra.get())) {
            auto it = varMap_.find(lhsName);
            if (it != varMap_.end()) {
                varMap_[eid->name] = it->second;
            }
        }
    }

    // If LHS is a MemberExpr (obj.field = val), the RHS escapes into the heap.
    if (dynamic_cast<ast::MemberExpr*>(n.target.get())) {
        if (!lhsName.empty()) markEscaped(lhsName);
        // Also mark direct allocations on the RHS as escaped
        if (auto* list = dynamic_cast<ast::ListExpr*>(n.value.get()))
            stackAllocatable_[list] = false;
        if (auto* dict = dynamic_cast<ast::DictExpr*>(n.value.get()))
            stackAllocatable_[dict] = false;
        if (auto* tup = dynamic_cast<ast::TupleExpr*>(n.value.get()))
            stackAllocatable_[tup] = false;
    }

    // If LHS is an IndexExpr (obj[idx] = val), the RHS escapes into a container.
    if (dynamic_cast<ast::IndexExpr*>(n.target.get())) {
        if (!lhsName.empty()) markEscaped(lhsName);
    }
}

void EscapeAnalyzer::visit(const ast::ExprStmt& n) {
    analyzeExpr(*n.expr);
}

void EscapeAnalyzer::visit(const ast::ThrowStmt& n) {
    if (n.expr && *n.expr) {
        analyzeExpr(*(*n.expr));
    }
}

void EscapeAnalyzer::visit(const ast::TryStmt& n) {
    analyzeStmtList(n.tryBody);
    for (const auto& clause : n.catchClauses) {
        analyzeStmtList(clause.body);
    }
    analyzeStmtList(n.finallyBody);
}

void EscapeAnalyzer::visit(const ast::MatchStmt& n) {
    analyzeExpr(*n.subject);
    for (const auto& kase : n.cases) {
        analyzeStmtList(kase.body);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Visitor overrides — Expression nodes
// ════════════════════════════════════════════════════════════════════════════

void EscapeAnalyzer::visit(const ast::CallExpr& n) {
    // If the callee is a known pure/transient function, args don't escape.
    // For v1: conservatively treat ALL call arguments as escaping.
    // The callee might store the argument into a global or heap object.
    for (const auto& arg : n.args) {
        if (auto* id = dynamic_cast<ast::Identifier*>(arg.get())) {
            markEscaped(id->name);
        }
        // Also handle nested allocations passed directly as arguments
        arg->accept(*this);
    }
    n.callee->accept(*this);
}

void EscapeAnalyzer::visit(const ast::ListExpr& n) {
    // Elements of a list that is itself an allocation site are analyzed
    // by the caller. If this list is standalone (not assigned), elements
    // still need traversal.
    for (const auto& elem : n.elements) {
        elem->accept(*this);
    }
}

void EscapeAnalyzer::visit(const ast::DictExpr& n) {
    for (const auto& entry : n.entries) {
        entry.first->accept(*this);
        entry.second->accept(*this);
    }
}

void EscapeAnalyzer::visit(const ast::TupleExpr& n) {
    for (const auto& elem : n.elements) {
        elem->accept(*this);
    }
}

void EscapeAnalyzer::visit(const ast::LambdaExpr& n) {
    // Analyze the lambda body — it may contain further allocations.
    n.body->accept(*this);

    // If this lambda captures variables, those captured variables
    // become reachable through the closure and thus escape.
    for (const auto& cap : n.captures) {
        markEscaped(cap.name);
    }
}

void EscapeAnalyzer::visit(const ast::MemberExpr& n) {
    // Member access on a variable — the object itself doesn't escape
    // just from being accessed, but the result of the member expression
    // doesn't introduce a new allocation.
    n.object->accept(*this);
}

} // namespace visuall
