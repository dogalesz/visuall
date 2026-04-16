#pragma once

#include "token.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace visuall {
namespace ast {

// ── Forward declarations ───────────────────────────────────────────────────
struct Expr;
struct Stmt;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;
using ExprList = std::vector<ExprPtr>;
using StmtList = std::vector<StmtPtr>;

// ── Base nodes ─────────────────────────────────────────────────────────────
struct Node {
    int line = 0;
    int column = 0;
    virtual ~Node() = default;
};

struct Expr : Node {
    virtual ~Expr() = default;
};

struct Stmt : Node {
    virtual ~Stmt() = default;
};

// ════════════════════════════════════════════════════════════════════════════
// EXPRESSIONS
// ════════════════════════════════════════════════════════════════════════════

struct IntLiteral : Expr {
    int64_t value;
    explicit IntLiteral(int64_t v) : value(v) {}
};

struct FloatLiteral : Expr {
    double value;
    explicit FloatLiteral(double v) : value(v) {}
};

struct StringLiteral : Expr {
    std::string value;
    explicit StringLiteral(std::string v) : value(std::move(v)) {}
};

struct FStringLiteral : Expr {
    std::string raw; // the full f"..." text for later interpolation lowering
    explicit FStringLiteral(std::string r) : raw(std::move(r)) {}
};

// ── F-String with parsed parts (full interpolation) ────────────────────────
struct FStringPart {
    bool isExpr;           // true = expression, false = literal string
    std::string literal;   // used when isExpr == false
    ExprPtr     expr;      // used when isExpr == true
};

struct FStringExpr : Expr {
    std::vector<FStringPart> parts;
    explicit FStringExpr(std::vector<FStringPart> p) : parts(std::move(p)) {}
};

struct BoolLiteral : Expr {
    bool value;
    explicit BoolLiteral(bool v) : value(v) {}
};

struct NullLiteral : Expr {};

struct Identifier : Expr {
    std::string name;
    explicit Identifier(std::string n) : name(std::move(n)) {}
};

struct ThisExpr : Expr {};

struct SuperExpr : Expr {};

// ── Binary / Unary ─────────────────────────────────────────────────────────
enum class BinOp {
    Add, Sub, Mul, Div, IntDiv, Mod, Pow,
    Eq, Neq, Lt, Gt, Lte, Gte,
    And, Or, In, NotIn,
    BitAnd, BitOr, BitXor, Shl, Shr,
};

struct BinaryExpr : Expr {
    BinOp  op;
    ExprPtr left;
    ExprPtr right;
    BinaryExpr(BinOp o, ExprPtr l, ExprPtr r)
        : op(o), left(std::move(l)), right(std::move(r)) {}
};

enum class UnaryOp { Neg, Not, BitNot };

struct UnaryExpr : Expr {
    UnaryOp op;
    ExprPtr operand;
    UnaryExpr(UnaryOp o, ExprPtr e)
        : op(o), operand(std::move(e)) {}
};

// ── Compound expressions ───────────────────────────────────────────────────
struct CallExpr : Expr {
    ExprPtr   callee;
    ExprList  args;
    std::vector<std::string> typeArgs; // for generic calls: identity<int>(42)
    CallExpr(ExprPtr c, ExprList a)
        : callee(std::move(c)), args(std::move(a)) {}
};

struct MemberExpr : Expr {
    ExprPtr     object;
    std::string member;
    MemberExpr(ExprPtr o, std::string m)
        : object(std::move(o)), member(std::move(m)) {}
};

struct IndexExpr : Expr {
    ExprPtr object;
    ExprPtr index;
    IndexExpr(ExprPtr o, ExprPtr i)
        : object(std::move(o)), index(std::move(i)) {}
};

// ── Capture info for closures ──────────────────────────────────────────────
struct CaptureInfo {
    std::string name;
    bool byReference = false; // true if the lambda assigns to the captured variable
};

struct LambdaExpr : Expr {
    std::vector<std::string> params;
    ExprPtr body;

    // Populated by CaptureAnalyzer before codegen.
    std::vector<CaptureInfo> captures;

    LambdaExpr(std::vector<std::string> p, ExprPtr b)
        : params(std::move(p)), body(std::move(b)) {}
};

struct ListExpr : Expr {
    ExprList elements;
    explicit ListExpr(ExprList e) : elements(std::move(e)) {}
};

struct DictExpr : Expr {
    std::vector<std::pair<ExprPtr, ExprPtr>> entries;
    explicit DictExpr(std::vector<std::pair<ExprPtr, ExprPtr>> e)
        : entries(std::move(e)) {}
};

struct TupleExpr : Expr {
    ExprList elements;
    explicit TupleExpr(ExprList e) : elements(std::move(e)) {}
};

struct TernaryExpr : Expr {
    ExprPtr condition;
    ExprPtr thenExpr;
    ExprPtr elseExpr;
    TernaryExpr(ExprPtr cond, ExprPtr then_, ExprPtr else_)
        : condition(std::move(cond)), thenExpr(std::move(then_)),
          elseExpr(std::move(else_)) {}
};

// ════════════════════════════════════════════════════════════════════════════
// STATEMENTS
// ════════════════════════════════════════════════════════════════════════════

struct ExprStmt : Stmt {
    ExprPtr expr;
    explicit ExprStmt(ExprPtr e) : expr(std::move(e)) {}
};

struct AssignStmt : Stmt {
    ExprPtr target;
    ExprPtr value;
    AssignStmt(ExprPtr t, ExprPtr v)
        : target(std::move(t)), value(std::move(v)) {}
};

// ── Tuple unpacking / destructuring ────────────────────────────────────────
struct TupleUnpackStmt : Stmt {
    std::vector<std::string> targets;
    ExprPtr                  value;
    TupleUnpackStmt(std::vector<std::string> t, ExprPtr v)
        : targets(std::move(t)), value(std::move(v)) {}
};

struct ReturnStmt : Stmt {
    ExprPtr value; // may be nullptr
    explicit ReturnStmt(ExprPtr v = nullptr) : value(std::move(v)) {}
};

struct BreakStmt : Stmt {};
struct ContinueStmt : Stmt {};
struct PassStmt : Stmt {}; // the ... no-op

// ── Slice expression ───────────────────────────────────────────────────────
struct SliceExpr : Expr {
    ExprPtr object;
    ExprPtr start;   // may be nullptr
    ExprPtr stop;    // may be nullptr
    ExprPtr step;    // may be nullptr
    SliceExpr(ExprPtr obj, ExprPtr s, ExprPtr e, ExprPtr st)
        : object(std::move(obj)), start(std::move(s)),
          stop(std::move(e)), step(std::move(st)) {}
};

// ── List comprehension: [expr for var in iter if cond] ─────────────────────
struct ListComprehension : Expr {
    ExprPtr     body;
    std::string variable;
    ExprPtr     iterable;
    ExprPtr     condition; // may be nullptr
    ListComprehension(ExprPtr b, std::string v, ExprPtr iter, ExprPtr cond)
        : body(std::move(b)), variable(std::move(v)),
          iterable(std::move(iter)), condition(std::move(cond)) {}
};

// ── Dict comprehension: {k: v for var in iter if cond} ─────────────────────
struct DictComprehension : Expr {
    ExprPtr     key;
    ExprPtr     value;
    std::string variable;
    ExprPtr     iterable;
    ExprPtr     condition; // may be nullptr
    DictComprehension(ExprPtr k, ExprPtr v, std::string var, ExprPtr iter, ExprPtr cond)
        : key(std::move(k)), value(std::move(v)), variable(std::move(var)),
          iterable(std::move(iter)), condition(std::move(cond)) {}
};

// ── Spread/unpack expression: *expr ────────────────────────────────────────
struct SpreadExpr : Expr {
    ExprPtr operand;
    explicit SpreadExpr(ExprPtr e) : operand(std::move(e)) {}
};

// ── Parameters ─────────────────────────────────────────────────────────────
struct Param {
    std::string name;
    std::string typeAnnotation; // may be empty
    ExprPtr     defaultValue;   // may be nullptr
    bool        isVariadic = false; // *args
};

// ── Function definition ────────────────────────────────────────────────────
struct FuncDef : Stmt {
    std::string         name;
    std::vector<std::string> typeParams; // generic type params: <T, U>
    std::vector<Param>  params;
    std::string         returnType; // may be empty
    StmtList            body;
    std::vector<ExprPtr> decorators; // may be empty
    FuncDef(std::string n, std::vector<Param> p, std::string ret, StmtList b)
        : name(std::move(n)), params(std::move(p)),
          returnType(std::move(ret)), body(std::move(b)) {}
};

// ── Class definition ───────────────────────────────────────────────────────
struct ClassDef : Stmt {
    std::string name;
    std::optional<std::string> baseClass;       // extends Base
    std::vector<std::string>   interfaces;      // implements I1, I2
    std::vector<std::string>   typeParams;      // <T, U>
    StmtList    body; // methods, init, etc.
    ClassDef(std::string n, StmtList b)
        : name(std::move(n)), body(std::move(b)) {}
};

// ── Init (constructor) ─────────────────────────────────────────────────────
struct InitDef : Stmt {
    std::vector<Param> params;
    StmtList           body;
    InitDef(std::vector<Param> p, StmtList b)
        : params(std::move(p)), body(std::move(b)) {}
};

// ── Control flow ───────────────────────────────────────────────────────────
struct IfStmt : Stmt {
    ExprPtr  condition;
    StmtList thenBranch;
    std::vector<std::pair<ExprPtr, StmtList>> elsifBranches;
    StmtList elseBranch; // may be empty
    IfStmt(ExprPtr cond, StmtList then_,
           std::vector<std::pair<ExprPtr, StmtList>> elsifs,
           StmtList else_)
        : condition(std::move(cond)), thenBranch(std::move(then_)),
          elsifBranches(std::move(elsifs)), elseBranch(std::move(else_)) {}
};

struct ForStmt : Stmt {
    std::string variable;
    ExprPtr     iterable;
    StmtList    body;
    ForStmt(std::string var, ExprPtr iter, StmtList b)
        : variable(std::move(var)), iterable(std::move(iter)),
          body(std::move(b)) {}
};

struct WhileStmt : Stmt {
    ExprPtr  condition;
    StmtList body;
    WhileStmt(ExprPtr cond, StmtList b)
        : condition(std::move(cond)), body(std::move(b)) {}
};

// ── Error handling ─────────────────────────────────────────────────────────
struct ThrowStmt : Stmt {
    ExprPtr expr;
    explicit ThrowStmt(ExprPtr e) : expr(std::move(e)) {}
};

struct CatchClause {
    std::string exceptionType; // may be empty for catch-all
    std::string varName;       // may be empty
    StmtList    body;
};

struct TryStmt : Stmt {
    StmtList                   tryBody;
    std::vector<CatchClause>   catchClauses;
    StmtList                   finallyBody; // may be empty
    TryStmt(StmtList try_, std::vector<CatchClause> catches, StmtList finally_)
        : tryBody(std::move(try_)), catchClauses(std::move(catches)),
          finallyBody(std::move(finally_)) {}
};

// ── Imports ────────────────────────────────────────────────────────────────
struct ImportStmt : Stmt {
    std::string module;
    explicit ImportStmt(std::string m) : module(std::move(m)) {}
};

struct FromImportStmt : Stmt {
    std::string              module;
    std::vector<std::string> names;
    FromImportStmt(std::string m, std::vector<std::string> n)
        : module(std::move(m)), names(std::move(n)) {}
};

// ── Interface definition ───────────────────────────────────────────────────
struct MethodSignature {
    std::string name;
    std::vector<Param> params;
    std::string returnType;
};

struct InterfaceDef : Stmt {
    std::string name;
    std::vector<MethodSignature> methods;
    InterfaceDef(std::string n, std::vector<MethodSignature> m)
        : name(std::move(n)), methods(std::move(m)) {}
};

// ── Program (top-level) ────────────────────────────────────────────────────
struct Program : Node {
    StmtList statements;
    explicit Program(StmtList s) : statements(std::move(s)) {}
};

} // namespace ast
} // namespace visuall
