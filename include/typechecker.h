#pragma once

#include "diagnostic.h"
#include "ast_visitor.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace visuall {

// ════════════════════════════════════════════════════════════════════════════
// TypeNode — base class for the polymorphic type IR.
// ════════════════════════════════════════════════════════════════════════════
struct TypeNode {
    // Kind enum — same values as the old Type::Kind for drop-in compatibility.
    enum Kind {
        Int, Float, Str, Bool, Void, Null,
        List, Dict, Tuple, Func, Class, Unknown,
        Union, Interface, TypeVar, Nullable
    };

    Kind kind;

    virtual ~TypeNode() = default;

    // Primary virtual interface.
    virtual std::string toString() const = 0;
    virtual std::string toUserString() const { return toString(); }
    virtual bool equals(const TypeNode& other) const = 0;
    virtual bool isAssignableTo(const TypeNode& other) const;

    // Convenience predicates — non-virtual, derived from kind.
    bool isUnknown() const { return kind == Unknown; }
    bool isNumeric() const { return kind == Int || kind == Float; }

    // Virtual name accessor for Class / Func / Interface / TypeVar nodes.
    // Returns "" for primitive and composite types.
    virtual const std::string& typeName() const;

protected:
    explicit TypeNode(Kind k) : kind(k) {}
};

using TypeRef = std::shared_ptr<TypeNode>;

// Structural equality helper (handles null TypeRef as "Unknown").
bool typeEquals(const TypeRef& a, const TypeRef& b);

// ════════════════════════════════════════════════════════════════════════════
// Concrete TypeNode subclasses
// ════════════════════════════════════════════════════════════════════════════

// PrimitiveType — int, float, str, bool, void, null, unknown
struct PrimitiveType : TypeNode {
    explicit PrimitiveType(Kind k) : TypeNode(k) {}
    std::string toString() const override;
    bool equals(const TypeNode& other) const override;
};

// ListType — list[T]
struct ListType : TypeNode {
    TypeRef elem;
    explicit ListType(TypeRef e) : TypeNode(List), elem(std::move(e)) {}
    std::string toString() const override;
    bool equals(const TypeNode& other) const override;
};

// DictType — dict[K, V]
struct DictType : TypeNode {
    TypeRef key;
    TypeRef value;
    DictType(TypeRef k, TypeRef v)
        : TypeNode(Dict), key(std::move(k)), value(std::move(v)) {}
    std::string toString() const override;
    bool equals(const TypeNode& other) const override;
};

// TupleType — tuple[T1, T2, ...]
struct TupleType : TypeNode {
    std::vector<TypeRef> elems;
    explicit TupleType(std::vector<TypeRef> e)
        : TypeNode(Tuple), elems(std::move(e)) {}
    std::string toString() const override;
    bool equals(const TypeNode& other) const override;
};

// FuncType — function signature: name(p1, p2, ...) -> ret
// paramTypes[0..n-1] are parameter types; retType is the return type.
struct FuncType : TypeNode {
    std::string name;      // function's declared name (for diagnostics / equality)
    TypeRef     retType;
    std::vector<TypeRef> paramTypes;

    FuncType(std::string n, TypeRef ret, std::vector<TypeRef> params)
        : TypeNode(Func),
          name(std::move(n)),
          retType(std::move(ret)),
          paramTypes(std::move(params)) {}

    std::string toString() const override;
    std::string toUserString() const override;
    bool equals(const TypeNode& other) const override;
    const std::string& typeName() const override { return name; }
};

// ClassType — a user-defined class
struct ClassType : TypeNode {
    std::string name;
    explicit ClassType(std::string n) : TypeNode(Class), name(std::move(n)) {}
    std::string toString() const override { return name; }
    bool equals(const TypeNode& other) const override;
    const std::string& typeName() const override { return name; }
};

// InterfaceTypeNode — a named interface
struct InterfaceTypeNode : TypeNode {
    std::string name;
    explicit InterfaceTypeNode(std::string n) : TypeNode(Interface), name(std::move(n)) {}
    std::string toString() const override { return name; }
    bool equals(const TypeNode& other) const override;
    const std::string& typeName() const override { return name; }
};

// TypeVarNode — generic type variable (e.g. T in identity<T>)
struct TypeVarNode : TypeNode {
    std::string name;
    explicit TypeVarNode(std::string n) : TypeNode(TypeVar), name(std::move(n)) {}
    std::string toString() const override { return name; }
    bool equals(const TypeNode& other) const override;
    const std::string& typeName() const override { return name; }
};

// UnionType — T1 | T2 | ...
struct UnionType : TypeNode {
    std::vector<TypeRef> members;
    explicit UnionType(std::vector<TypeRef> m) : TypeNode(Union), members(std::move(m)) {}
    std::string toString() const override;
    bool equals(const TypeNode& other) const override;
};

// NullableType — T | null  (sugar over UnionType for common case)
struct NullableType : TypeNode {
    TypeRef inner;
    explicit NullableType(TypeRef i) : TypeNode(Nullable), inner(std::move(i)) {}
    std::string toString() const override;
    bool equals(const TypeNode& other) const override;
};

// ════════════════════════════════════════════════════════════════════════════
// Factory functions — return TypeRef, preserve the old Type::make*() names.
// ════════════════════════════════════════════════════════════════════════════
TypeRef makeInt();
TypeRef makeFloat();
TypeRef makeStr();
TypeRef makeBool();
TypeRef makeVoid();
TypeRef makeNull();
TypeRef makeUnknown();
TypeRef makeList(TypeRef elem);
TypeRef makeDict(TypeRef key, TypeRef value);
TypeRef makeTuple(std::vector<TypeRef> elems);
TypeRef makeFunc(const std::string& name, TypeRef ret, std::vector<TypeRef> paramTypes);
TypeRef makeClass(const std::string& name);
TypeRef makeInterface(const std::string& name);
TypeRef makeTypeVar(const std::string& name);
TypeRef makeUnion(std::vector<TypeRef> members);
TypeRef makeNullable(TypeRef inner);

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
// SymbolTable — scoped name → TypeRef mapping.
// ════════════════════════════════════════════════════════════════════════════
class SymbolTable {
public:
    void enterScope();
    void exitScope();
    void declare(const std::string& name, const TypeRef& type);
    bool isDeclared(const std::string& name) const;
    TypeRef lookup(const std::string& name) const;  // returns makeUnknown() if not found
    bool lookupInCurrentScope(const std::string& name, TypeRef& out) const;

    // Return all currently visible names across all scopes.
    std::vector<std::string> allNames() const;

private:
    std::vector<std::unordered_map<std::string, TypeRef>> scopes_;
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
    void visit(const ast::DictSpreadExpr& n)      override;
    void visit(const ast::WalrusExpr& n)          override;

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
    void visit(const ast::DelStmt& n)             override;
    void visit(const ast::WithStmt& n)            override;
    void visit(const ast::MatchStmt& n)           override;
    void visit(const ast::TryStmt& n)             override;
    void visit(const ast::ImportStmt& n)          override;
    void visit(const ast::FromImportStmt& n)      override;
    void visit(const ast::InterfaceDef& n)        override;

private:
    std::string filename_;
    SymbolTable symbols_;
    bool insideClass_ = false;
    TypeRef currentReturnType_;        // expected return type of the current function
    bool hasExplicitReturnType_ = false;
    std::string currentClassName_;     // name of the class being checked

    // ── Result slot for expression visits ─────────────────────────────────
    // Set by each visit(ExprNode) override; read back by checkExpr().
    TypeRef exprResult_;

    // ── Type parameter tracking ────────────────────────────────────
    std::vector<std::string> currentTypeParams_;

    // ── Class and interface tables ─────────────────────────────────
    struct MethodInfo {
        std::string name;
        TypeRef returnType;
        std::vector<TypeRef> paramTypes;
    };
    struct ClassInfo {
        std::string name;
        std::string baseClass;
        std::vector<std::string> extraBases;  // multiple inheritance
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
    // Maps function name → minimum required arg count (for defaults/kwargs/variadic).
    std::unordered_map<std::string, size_t> funcMinArgs_;
    std::vector<TypeError> errors_;

    // ── Scope helpers ──────────────────────────────────────────────────
    void enterScope();
    void exitScope();
    void declare(const std::string& name, const TypeRef& type, int line, int col);
    TypeRef lookup(const std::string& name, int line, int col);

    // ── Type resolution ────────────────────────────────────────────────
    TypeRef resolveTypeName(const std::string& name) const;
    TypeRef unify(const TypeRef& a, const TypeRef& b, int line, int col);
    TypeRef promoteNumeric(const TypeRef& a, const TypeRef& b);
    bool isSubtype(const TypeRef& sub, const TypeRef& super) const;
    bool isAssignableTo(const TypeRef& from, const TypeRef& to) const;
    bool classImplementsInterface(const std::string& className, const std::string& ifaceName) const;

    // ── AST traversal helpers ──────────────────────────────────────────
    // checkStmt / checkExpr are the public-facing entry points used internally.
    // They call node.accept(*this) and return/carry the result.
    void checkStmt(const ast::Stmt& stmt);
    void checkStmtList(const ast::StmtList& stmts);
    TypeRef checkExpr(const ast::Expr& expr);

    void error(const std::string& msg, int line, int col,
               const std::string& hint = "");
};

} // namespace visuall
