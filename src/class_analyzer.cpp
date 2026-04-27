#include "class_analyzer.h"
#include <algorithm>

namespace visuall {

// ════════════════════════════════════════════════════════════════════════════
// Public entry point
// ════════════════════════════════════════════════════════════════════════════

void ClassAnalyzer::analyze(const ast::Program& program) {
    // First pass: collect all top-level ClassDef nodes and inheritance edges.
    std::unordered_map<std::string, const ast::ClassDef*> classDefs;
    for (const auto& s : program.statements) {
        if (auto* cls = dynamic_cast<const ast::ClassDef*>(s.get())) {
            classDefs[cls->name] = cls;
            if (cls->baseClass) {
                baseClasses_[cls->name] = *cls->baseClass;
            }
        }
    }

    // Second pass: process each class in dependency order so that base-class
    // fields are always inserted before derived-class fields.
    std::unordered_set<std::string> processed;
    for (const auto& [name, cls] : classDefs) {
        processClass(name, classDefs, processed);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Private helpers
// ════════════════════════════════════════════════════════════════════════════

void ClassAnalyzer::processClass(
    const std::string& name,
    const std::unordered_map<std::string, const ast::ClassDef*>& classDefs,
    std::unordered_set<std::string>& processed)
{
    if (processed.count(name)) return;
    processed.insert(name);

    // Ensure the base class is fully processed first.
    auto baseIt = baseClasses_.find(name);
    if (baseIt != baseClasses_.end()) {
        const std::string& baseName = baseIt->second;
        if (classDefs.count(baseName)) {
            processClass(baseName, classDefs, processed);
        }
        // Inherit base-class fields (in order, without duplicates).
        auto baseFieldsIt = classFields_.find(baseName);
        if (baseFieldsIt != classFields_.end()) {
            for (const auto& f : baseFieldsIt->second) {
                addField(name, f);
            }
        }
    }

    // Collect this class's own fields from init + all method bodies.
    auto defIt = classDefs.find(name);
    if (defIt != classDefs.end()) {
        collectFields(name, defIt->second->body);
    }
}

void ClassAnalyzer::collectFields(const std::string& className,
                                  const ast::StmtList& stmts)
{
    for (const auto& s : stmts) {
        collectFieldsFromStmt(className, *s);
    }
}

void ClassAnalyzer::collectFieldsFromStmt(const std::string& className,
                                          const ast::Stmt& stmt)
{
    // ── InitDef: constructor body ──────────────────────────────────────
    if (auto* s = dynamic_cast<const ast::InitDef*>(&stmt)) {
        collectFields(className, s->body);
        return;
    }

    // ── FuncDef: method body ───────────────────────────────────────────
    if (auto* s = dynamic_cast<const ast::FuncDef*>(&stmt)) {
        collectFields(className, s->body);
        return;
    }

    // ── AssignStmt: this.field = ... ──────────────────────────────────
    if (auto* s = dynamic_cast<const ast::AssignStmt*>(&stmt)) {
        if (auto* mem = dynamic_cast<const ast::MemberExpr*>(s->target.get())) {
            bool isThisRef =
                dynamic_cast<const ast::ThisExpr*>(mem->object.get()) != nullptr;
            if (!isThisRef) {
                // Fallback for any code path that emits Identifier("this"/"self")
                if (auto* id = dynamic_cast<const ast::Identifier*>(mem->object.get())) {
                    isThisRef = (id->name == "this" || id->name == "self");
                }
            }
            if (isThisRef) {
                addField(className, mem->member);
            }
        }
        return;
    }

    // ── IfStmt: scan all branches ─────────────────────────────────────
    if (auto* s = dynamic_cast<const ast::IfStmt*>(&stmt)) {
        collectFields(className, s->thenBranch);
        for (const auto& [cond, body] : s->elsifBranches) {
            collectFields(className, body);
        }
        collectFields(className, s->elseBranch);
        return;
    }

    // ── ForStmt ───────────────────────────────────────────────────────
    if (auto* s = dynamic_cast<const ast::ForStmt*>(&stmt)) {
        collectFields(className, s->body);
        return;
    }

    // ── WhileStmt ─────────────────────────────────────────────────────
    if (auto* s = dynamic_cast<const ast::WhileStmt*>(&stmt)) {
        collectFields(className, s->body);
        return;
    }

    // ── TryStmt: try body + all catch bodies + finally ────────────────
    if (auto* s = dynamic_cast<const ast::TryStmt*>(&stmt)) {
        collectFields(className, s->tryBody);
        for (const auto& c : s->catchClauses) {
            collectFields(className, c.body);
        }
        collectFields(className, s->finallyBody);
        return;
    }

    // ExprStmt, ReturnStmt, BreakStmt, etc. cannot contain this.field = ...
    // assignments at the statement level, so they are intentionally ignored.
}

void ClassAnalyzer::addField(const std::string& className,
                             const std::string& fieldName)
{
    auto& fields = classFields_[className];
    for (const auto& f : fields) {
        if (f == fieldName) return; // already present — preserve order
    }
    fields.push_back(fieldName);
}

} // namespace visuall
