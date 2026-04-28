#pragma once

#include "diagnostic.h"
#include "ast_visitor.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace visuall {

// ════════════════════════════════════════════════════════════════════════════
// Type — a simple structural type representation.
// ════════════════════════════════════════════════════════════════════════════
struct Type {
    enum Kind { Int, Float, Str, Bool, Void, Null, List, Dict, Tuple, Func, Class, Unknown, Union, Interface, TypeVar };

    Kind kind = Unknown;
    std::vector<Type> params;   // List<T> → params[0]=T, Dict<K,V> → [K,V], Tuple → [T...], Func → [ret, p0, p1, ...]
    std::string name;           // for Class or Func (function name for diagnostics)

    Type() = default;
    explicit Type(Kind k) : kind(k) {}
    Type(Kind k, std::vector<Type> p) : kind(k), params(std::move(p)) {}
    Type(Kind k, std::string n) : kind(k), name(std::move(n)) {}
    Type(Kind k, std::string n, std::vector<Type> p)
        : kind(k), params(std::move(p)), name(std::move(n)) {}

    bool operator==(const Type& o) const;
    bool operator!=(const Type& o) const { return !(*this == o); }

    bool isNumeric() const { return kind == Int || kind == Float; }
    bool isUnknown() const { return kind == Unknown; }

    // Internal string representation (also used as user-facing Visuall syntax).
    std::string toString() const;

    // User-facing type name in Visuall syntax: int, str, list[int], (int)->bool.
    // Preferred over toString() for error messages shown to users.
    std::string toUserString() const;

    // Convenience factories
    static Type makeInt()   { return Type(Int); }
    static Type makeFloat() { return Type(Float); }
    static Type makeStr()   { return Type(Str); }
    static Type makeBool()  { return Type(Bool); }
    static Type makeVoid()  { return Type(Void); }
    static Type makeNull()  { return Type(Null); }
    static Type makeUnknown() { return Type(Unknown); }
    static Type makeList(Type elem) { return Type(List, {std::move(elem)}); }
    static Type makeDict(Type k, Type v) { return Type(Dict, {std::move(k), std::move(v)}); }
    static Type makeTuple(std::vector<Type> elems) { return Type(Tuple, std::move(elems)); }
    static Type makeFunc(const std::string& name, Type ret, std::vector<Type> paramTypes);
    static Type makeClass(const std::string& name) { return Type(Class, name); }
    static Type makeUnion(std::vector<Type> members) { return Type(Union, std::move(members)); }
    static Type makeInterface(const std::string& name) { return Type(Interface, name); }
    static Type makeTypeVar(const std::string& name) { return Type(TypeVar, name); }
};

// ════════════════════════════════════════════════════════════════════════════
// TypeError — thrown on all type-checking errors.
// Inherits Diagnostic for clang-style formatting.
// ════════════════════════════════════════════════════════════════════════════
class TypeError : public Diagnostic {
public:
    TypeError(const std::string& msg, const std::string& file, int ln, int col,
              const std::string& hnt = "")
        : Diagnostic(Diagnostic::Severity::Error, msg, hnt, file, ln, col) {}
};

// ════════════════════════════════════════════════════════════════════════════
// SymbolTable — scoped name → Type mapping.
// ════════════════════════════════════════════════════════════════════════════
class SymbolTable {
public:
    void enterScope();
    void exitScope();
    void declare(const std::string& name, const Type& type);
    bool isDeclared(const std::string& name) const;
    Type lookup(const std::string& name) const;  // returns Unknown if not found
    bool lookupInCurrentScope(const std::string& name, Type& out) const;

    // Return all currently visible names across all scopes.
    std::vector<std::string> allNames() const;

private:
    std::vector<std::unordered_map<std::string, Type>> scopes_;
};

// ════════════════════════════════════════════════════════════════════════════
// TypeChecker — walks the AST and verifies type correctness.
//
// Implements ASTVisitor: visit() overloads are the per-node type-checking
// logic. checkExpr() / checkStmt() are the entry points that call
// node.accept(*this) and return/carry results.
// ════════════════════════════════════════════════════════════════════════════
class TypeChecker : public ASTVisitor {
public:
    explicit TypeChecker(const std::string& filename);

    void check(const ast::Program& program);
    const std::vector<TypeError>& errors() const { return errors_; };

    // ── ASTVisitor overrides — expression nodes ──────────────────────────
    void visit(const ast::IntLiteral& n)          override;
    void visit(const ast::FloatLiteral& n)        override;
    void visit(const ast::StringLiteral& n)       override;
    void visit(const ast::FStringLiteral& n)      override;
    void visit(const ast::FStringExpr& n)         override;
    void visit(const ast::BoolLiteral& n)         override;
    void visit(const ast::NullLiteral& n)         override;
    void visit(const ast::Identifier& n)          override;
    void visit(const ast::ThisExpr& n)            override;
    void visit(const ast::SuperExpr& n)           override;
    void visit(const ast::BinaryExpr& n)          override;
    void visit(const ast::UnaryExpr& n)           override;
    void visit(const ast::CallExpr& n)            override;
    void visit(const ast::MemberExpr& n)          override;
    void visit(const ast::IndexExpr& n)           override;
    void visit(const ast::LambdaExpr& n)          override;
    void visit(const ast::ListExpr& n)            override;
    void visit(const ast::DictExpr& n)            override;
    void visit(const ast::TupleExpr& n)           override;
    void visit(const ast::TernaryExpr& n)         override;
    void visit(const ast::SliceExpr& n)           override;
    void visit(const ast::ListComprehension& n)   override;
    void visit(const ast::DictComprehension& n)   override;
    void visit(const ast::SpreadExpr& n)          override;

    // ── ASTVisitor overrides — statement nodes ───────────────────────────
    void visit(const ast::ExprStmt& n)            override;
    void visit(const ast::AssignStmt& n)          override;
    void visit(const ast::TupleUnpackStmt& n)     override;
    void visit(const ast::ReturnStmt& n)          override;
    void visit(const ast::BreakStmt& n)           override;
    void visit(const ast::ContinueStmt& n)        override;
    void visit(const ast::PassStmt& n)            override;
    void visit(const ast::FuncDef& n)             override;
    void visit(const ast::ClassDef& n)            override;
    void visit(const ast::InitDef& n)             override;
    void visit(const ast::IfStmt& n)              override;
    void visit(const ast::ForStmt& n)             override;
    void visit(const ast::WhileStmt& n)           override;
    void visit(const ast::ThrowStmt& n)           override;
    void visit(const ast::AssertStmt& n)          override;
    void visit(const ast::TryStmt& n)             override;
    void visit(const ast::ImportStmt& n)          override;
    void visit(const ast::FromImportStmt& n)      override;
    void visit(const ast::InterfaceDef& n)        override;

private:
    std::string filename_;
    SymbolTable symbols_;
    bool insideClass_ = false;
    Type currentReturnType_;           // expected return type of the current function
    bool hasExplicitReturnType_ = false;
    std::string currentClassName_;     // name of the class being checked

    // ── Result slot for expression visits ─────────────────────────────────
    // Set by each visit(ExprNode) override; read back by checkExpr().
    Type exprResult_;

    // ── Type parameter tracking ────────────────────────────────────
    std::vector<std::string> currentTypeParams_;

    // ── Class and interface tables ─────────────────────────────────
    struct MethodInfo {
        std::string name;
        Type returnType;
        std::vector<Type> paramTypes;
    };
    struct ClassInfo {
        std::string name;
        std::string baseClass;
        std::vector<std::string> interfaces;
        std::vector<std::string> typeParams;
        std::vector<MethodInfo> methods;
    };
    struct InterfaceInfo {
        std::string name;
        std::vector<MethodInfo> methods;
    };
    std::unordered_map<std::string, ClassInfo> classTable_;
    std::unordered_map<std::string, InterfaceInfo> interfaceTable_;
    std::unordered_map<std::string, std::vector<std::string>> funcTypeParams_;
    std::vector<TypeError> errors_;

    // ── Scope helpers ──────────────────────────────────────────────────
    void enterScope();
    void exitScope();
    void declare(const std::string& name, const Type& type, int line, int col);
    Type lookup(const std::string& name, int line, int col);

    // ── Type resolution ────────────────────────────────────────────────
    Type resolveTypeName(const std::string& name) const;
    Type unify(const Type& a, const Type& b, int line, int col);
    Type promoteNumeric(const Type& a, const Type& b);
    bool isSubtype(const Type& sub, const Type& super) const;
    bool isAssignableTo(const Type& from, const Type& to) const;
    bool classImplementsInterface(const std::string& className, const std::string& ifaceName) const;

    // ── AST traversal helpers ──────────────────────────────────────────
    // checkStmt / checkExpr are the public-facing entry points used internally.
    // They call node.accept(*this) and return/carry the result.
    void checkStmt(const ast::Stmt& stmt);
    void checkStmtList(const ast::StmtList& stmts);
    Type checkExpr(const ast::Expr& expr);

    void error(const std::string& msg, int line, int col,
               const std::string& hint = "");
};

} // namespace visuall
