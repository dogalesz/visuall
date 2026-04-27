#pragma once

// Forward-include ast.h so callers only need to include ast_visitor.h.
#include "ast.h"

namespace visuall {

// ════════════════════════════════════════════════════════════════════════════
// ASTVisitor — pure-virtual visitor interface for every concrete AST node.
//
// Double-dispatch protocol:
//   1. Call node.accept(visitor)  (defined on each concrete node in ast.h)
//   2. The node calls visitor.visit(*this) with its static type.
//
// Return-value convention
// ───────────────────────
// visit() is always void. Visitor implementations that need to produce a value
// (e.g. TypeChecker → Type, Codegen → llvm::Value*) store it in a member
// field and the caller reads it after accept() returns.
// ════════════════════════════════════════════════════════════════════════════
class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;

    // ── Expression nodes ──────────────────────────────────────────────────
    virtual void visit(const ast::IntLiteral&)         = 0;
    virtual void visit(const ast::FloatLiteral&)       = 0;
    virtual void visit(const ast::StringLiteral&)      = 0;
    virtual void visit(const ast::FStringLiteral&)     = 0;
    virtual void visit(const ast::FStringExpr&)        = 0;
    virtual void visit(const ast::BoolLiteral&)        = 0;
    virtual void visit(const ast::NullLiteral&)        = 0;
    virtual void visit(const ast::Identifier&)         = 0;
    virtual void visit(const ast::ThisExpr&)           = 0;
    virtual void visit(const ast::SuperExpr&)          = 0;
    virtual void visit(const ast::BinaryExpr&)         = 0;
    virtual void visit(const ast::UnaryExpr&)          = 0;
    virtual void visit(const ast::CallExpr&)           = 0;
    virtual void visit(const ast::MemberExpr&)         = 0;
    virtual void visit(const ast::IndexExpr&)          = 0;
    virtual void visit(const ast::LambdaExpr&)         = 0;
    virtual void visit(const ast::ListExpr&)           = 0;
    virtual void visit(const ast::DictExpr&)           = 0;
    virtual void visit(const ast::TupleExpr&)          = 0;
    virtual void visit(const ast::TernaryExpr&)        = 0;
    virtual void visit(const ast::SliceExpr&)          = 0;
    virtual void visit(const ast::ListComprehension&)  = 0;
    virtual void visit(const ast::DictComprehension&)  = 0;
    virtual void visit(const ast::SpreadExpr&)         = 0;

    // ── Statement nodes ───────────────────────────────────────────────────
    virtual void visit(const ast::ExprStmt&)           = 0;
    virtual void visit(const ast::AssignStmt&)         = 0;
    virtual void visit(const ast::TupleUnpackStmt&)    = 0;
    virtual void visit(const ast::ReturnStmt&)         = 0;
    virtual void visit(const ast::BreakStmt&)          = 0;
    virtual void visit(const ast::ContinueStmt&)       = 0;
    virtual void visit(const ast::PassStmt&)           = 0;
    virtual void visit(const ast::FuncDef&)            = 0;
    virtual void visit(const ast::ClassDef&)           = 0;
    virtual void visit(const ast::InitDef&)            = 0;
    virtual void visit(const ast::IfStmt&)             = 0;
    virtual void visit(const ast::ForStmt&)            = 0;
    virtual void visit(const ast::WhileStmt&)          = 0;
    virtual void visit(const ast::ThrowStmt&)          = 0;
    virtual void visit(const ast::TryStmt&)            = 0;
    virtual void visit(const ast::ImportStmt&)         = 0;
    virtual void visit(const ast::FromImportStmt&)     = 0;
    virtual void visit(const ast::InterfaceDef&)       = 0;
};

// ════════════════════════════════════════════════════════════════════════════
// ASTVisitorBase — default no-op implementations for every visit() overload.
//
// Derive from this when you only care about a subset of node types (e.g. the
// AstPrinter's compact-expression helper).
// ════════════════════════════════════════════════════════════════════════════
class ASTVisitorBase : public ASTVisitor {
public:
    // Expressions
    void visit(const ast::IntLiteral&)        override {}
    void visit(const ast::FloatLiteral&)      override {}
    void visit(const ast::StringLiteral&)     override {}
    void visit(const ast::FStringLiteral&)    override {}
    void visit(const ast::FStringExpr&)       override {}
    void visit(const ast::BoolLiteral&)       override {}
    void visit(const ast::NullLiteral&)       override {}
    void visit(const ast::Identifier&)        override {}
    void visit(const ast::ThisExpr&)          override {}
    void visit(const ast::SuperExpr&)         override {}
    void visit(const ast::BinaryExpr&)        override {}
    void visit(const ast::UnaryExpr&)         override {}
    void visit(const ast::CallExpr&)          override {}
    void visit(const ast::MemberExpr&)        override {}
    void visit(const ast::IndexExpr&)         override {}
    void visit(const ast::LambdaExpr&)        override {}
    void visit(const ast::ListExpr&)          override {}
    void visit(const ast::DictExpr&)          override {}
    void visit(const ast::TupleExpr&)         override {}
    void visit(const ast::TernaryExpr&)       override {}
    void visit(const ast::SliceExpr&)         override {}
    void visit(const ast::ListComprehension&) override {}
    void visit(const ast::DictComprehension&) override {}
    void visit(const ast::SpreadExpr&)        override {}

    // Statements
    void visit(const ast::ExprStmt&)          override {}
    void visit(const ast::AssignStmt&)        override {}
    void visit(const ast::TupleUnpackStmt&)   override {}
    void visit(const ast::ReturnStmt&)        override {}
    void visit(const ast::BreakStmt&)         override {}
    void visit(const ast::ContinueStmt&)      override {}
    void visit(const ast::PassStmt&)          override {}
    void visit(const ast::FuncDef&)           override {}
    void visit(const ast::ClassDef&)          override {}
    void visit(const ast::InitDef&)           override {}
    void visit(const ast::IfStmt&)            override {}
    void visit(const ast::ForStmt&)           override {}
    void visit(const ast::WhileStmt&)         override {}
    void visit(const ast::ThrowStmt&)         override {}
    void visit(const ast::TryStmt&)           override {}
    void visit(const ast::ImportStmt&)        override {}
    void visit(const ast::FromImportStmt&)    override {}
    void visit(const ast::InterfaceDef&)      override {}
};

} // namespace visuall

// ════════════════════════════════════════════════════════════════════════════
// accept() definitions — must be after ASTVisitor is fully defined.
// Placed here so every translation unit that includes ast_visitor.h picks
// them up without needing a separate ast.cpp.
// ════════════════════════════════════════════════════════════════════════════
namespace visuall { namespace ast {

// Expressions
inline void IntLiteral::accept(ASTVisitor& v)         const { v.visit(*this); }
inline void FloatLiteral::accept(ASTVisitor& v)       const { v.visit(*this); }
inline void StringLiteral::accept(ASTVisitor& v)      const { v.visit(*this); }
inline void FStringLiteral::accept(ASTVisitor& v)     const { v.visit(*this); }
inline void FStringExpr::accept(ASTVisitor& v)        const { v.visit(*this); }
inline void BoolLiteral::accept(ASTVisitor& v)        const { v.visit(*this); }
inline void NullLiteral::accept(ASTVisitor& v)        const { v.visit(*this); }
inline void Identifier::accept(ASTVisitor& v)         const { v.visit(*this); }
inline void ThisExpr::accept(ASTVisitor& v)           const { v.visit(*this); }
inline void SuperExpr::accept(ASTVisitor& v)          const { v.visit(*this); }
inline void BinaryExpr::accept(ASTVisitor& v)         const { v.visit(*this); }
inline void UnaryExpr::accept(ASTVisitor& v)          const { v.visit(*this); }
inline void CallExpr::accept(ASTVisitor& v)           const { v.visit(*this); }
inline void MemberExpr::accept(ASTVisitor& v)         const { v.visit(*this); }
inline void IndexExpr::accept(ASTVisitor& v)          const { v.visit(*this); }
inline void LambdaExpr::accept(ASTVisitor& v)         const { v.visit(*this); }
inline void ListExpr::accept(ASTVisitor& v)           const { v.visit(*this); }
inline void DictExpr::accept(ASTVisitor& v)           const { v.visit(*this); }
inline void TupleExpr::accept(ASTVisitor& v)          const { v.visit(*this); }
inline void TernaryExpr::accept(ASTVisitor& v)        const { v.visit(*this); }
inline void SliceExpr::accept(ASTVisitor& v)          const { v.visit(*this); }
inline void ListComprehension::accept(ASTVisitor& v)  const { v.visit(*this); }
inline void DictComprehension::accept(ASTVisitor& v)  const { v.visit(*this); }
inline void SpreadExpr::accept(ASTVisitor& v)         const { v.visit(*this); }

// Statements
inline void ExprStmt::accept(ASTVisitor& v)           const { v.visit(*this); }
inline void AssignStmt::accept(ASTVisitor& v)         const { v.visit(*this); }
inline void TupleUnpackStmt::accept(ASTVisitor& v)    const { v.visit(*this); }
inline void ReturnStmt::accept(ASTVisitor& v)         const { v.visit(*this); }
inline void BreakStmt::accept(ASTVisitor& v)          const { v.visit(*this); }
inline void ContinueStmt::accept(ASTVisitor& v)       const { v.visit(*this); }
inline void PassStmt::accept(ASTVisitor& v)           const { v.visit(*this); }
inline void FuncDef::accept(ASTVisitor& v)            const { v.visit(*this); }
inline void ClassDef::accept(ASTVisitor& v)           const { v.visit(*this); }
inline void InitDef::accept(ASTVisitor& v)            const { v.visit(*this); }
inline void IfStmt::accept(ASTVisitor& v)             const { v.visit(*this); }
inline void ForStmt::accept(ASTVisitor& v)            const { v.visit(*this); }
inline void WhileStmt::accept(ASTVisitor& v)          const { v.visit(*this); }
inline void ThrowStmt::accept(ASTVisitor& v)          const { v.visit(*this); }
inline void TryStmt::accept(ASTVisitor& v)            const { v.visit(*this); }
inline void ImportStmt::accept(ASTVisitor& v)         const { v.visit(*this); }
inline void FromImportStmt::accept(ASTVisitor& v)     const { v.visit(*this); }
inline void InterfaceDef::accept(ASTVisitor& v)       const { v.visit(*this); }

}} // namespace visuall::ast
