#pragma once

#include "ast.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace visuall {

// ════════════════════════════════════════════════════════════════════════════
// ClassAnalyzer — pre-pass that walks the full class body AST and collects
// all `this.field` assignments across init and all methods (including fields
// set conditionally or inside loops/try blocks).
//
// Must run AFTER CaptureAnalyzer and BEFORE TypeChecker / Codegen.
//
// Usage:
//   ClassAnalyzer ca;
//   ca.analyze(program);
//   codegen.setClassFields(ca.classFields());
// ════════════════════════════════════════════════════════════════════════════
class ClassAnalyzer {
public:
    /// Walk the entire program AST and populate classFields_.
    /// Handles inheritance: base-class fields precede derived-class fields.
    void analyze(const ast::Program& program);

    /// Return the fully-built map: className → ordered, deduplicated field list.
    const std::unordered_map<std::string, std::vector<std::string>>&
    classFields() const { return classFields_; }

private:
    // Result: className → ordered field names (no duplicates).
    std::unordered_map<std::string, std::vector<std::string>> classFields_;

    // Inheritance map: className → baseClassName (populated by analyze()).
    std::unordered_map<std::string, std::string> baseClasses_;

    // Process one class (recursively ensures base is processed first).
    void processClass(
        const std::string& name,
        const std::unordered_map<std::string, const ast::ClassDef*>& classDefs,
        std::unordered_set<std::string>& processed);

    // Walk a statement list and record this.field assignments for className.
    void collectFields(const std::string& className,
                       const ast::StmtList& stmts);

    // Dispatch a single statement (recurses into control-flow bodies).
    void collectFieldsFromStmt(const std::string& className,
                               const ast::Stmt& stmt);

    // Add fieldName to classFields_[className] if not already present.
    void addField(const std::string& className,
                  const std::string& fieldName);
};

} // namespace visuall
