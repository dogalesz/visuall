#pragma once

#include "ast.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>

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

    std::string toString() const;

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
// Format: "TypeError: [message] at [filename]:[line]:[col]"
// ════════════════════════════════════════════════════════════════════════════
class TypeError : public std::runtime_error {
public:
    std::string filename;
    int line;
    int column;

    TypeError(const std::string& msg, const std::string& file, int ln, int col)
        : std::runtime_error("TypeError: " + msg + " at " +
                             file + ":" + std::to_string(ln) + ":" +
                             std::to_string(col)),
          filename(file), line(ln), column(col) {}
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

private:
    std::vector<std::unordered_map<std::string, Type>> scopes_;
};

// ════════════════════════════════════════════════════════════════════════════
// TypeChecker — walks the AST and verifies type correctness.
// ════════════════════════════════════════════════════════════════════════════
class TypeChecker {
public:
    explicit TypeChecker(const std::string& filename);

    void check(const ast::Program& program);
    const std::vector<TypeError>& errors() const { return errors_; };

private:
    std::string filename_;
    SymbolTable symbols_;
    bool insideClass_ = false;
    Type currentReturnType_;           // expected return type of the current function
    bool hasExplicitReturnType_ = false;
    std::string currentClassName_;     // name of the class being checked

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

    // ── AST visitors ───────────────────────────────────────────────────
    void checkStmt(const ast::Stmt& stmt);
    void checkStmtList(const ast::StmtList& stmts);
    Type checkExpr(const ast::Expr& expr);

    void error(const std::string& msg, int line, int col);
};

} // namespace visuall
