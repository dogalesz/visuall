#include "typechecker.h"
#include "builtins.h"
#include <sstream>
#include <cassert>
#include <climits>
#include <algorithm>

namespace visuall {

// ════════════════════════════════════════════════════════════════════════════
// Levenshtein distance helper
// ════════════════════════════════════════════════════════════════════════════

static int levenshtein(const std::string& a, const std::string& b) {
    size_t na = a.size(), nb = b.size();
    if (na == 0) return static_cast<int>(nb);
    if (nb == 0) return static_cast<int>(na);
    // Use two-row rolling DP.
    std::vector<int> prev(nb + 1), curr(nb + 1);
    for (size_t j = 0; j <= nb; ++j) prev[j] = static_cast<int>(j);
    for (size_t i = 1; i <= na; ++i) {
        curr[0] = static_cast<int>(i);
        for (size_t j = 1; j <= nb; ++j) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            int c1 = std::min(prev[j] + 1, curr[j - 1] + 1);
            curr[j] = std::min(c1, prev[j - 1] + cost);
        }
        std::swap(prev, curr);
    }
    return prev[nb];
}

// Helper: scan stmts (non-recursively) for any ReturnStmt.
static bool stmtListHasReturn(const ast::StmtList& stmts) {
    for (const auto& s : stmts) {
        if (dynamic_cast<const ast::ReturnStmt*>(s.get())) return true;
        if (auto* ifStmt = dynamic_cast<const ast::IfStmt*>(s.get())) {
            if (stmtListHasReturn(ifStmt->thenBranch)) return true;
        }
        if (auto* whileStmt = dynamic_cast<const ast::WhileStmt*>(s.get())) {
            if (stmtListHasReturn(whileStmt->body)) return true;
        }
        if (auto* forStmt = dynamic_cast<const ast::ForStmt*>(s.get())) {
            if (stmtListHasReturn(forStmt->body)) return true;
        }
    }
    return false;
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Type
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•


// â”€â”€â”€ TypeNode base â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
static const std::string& emptyStr() { static std::string s; return s; }
const std::string& TypeNode::typeName() const { return emptyStr(); }
bool TypeNode::isAssignableTo(const TypeNode& /*other*/) const { return false; }

bool typeEquals(const TypeRef& a, const TypeRef& b) {
    if (!a && !b) return true; if (!a || !b) return false;
    return a->equals(*b);
}

// â”€â”€â”€ PrimitiveType â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
std::string PrimitiveType::toString() const {
    switch (kind) {
        case Int: return "int"; case Float: return "float"; case Str: return "str";
        case Bool: return "bool"; case Void: return "void"; case Null: return "null";
        default: return "unknown";
    }
}
bool PrimitiveType::equals(const TypeNode& o) const { return kind == o.kind; }

// â”€â”€â”€ ListType â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
std::string ListType::toString() const { return "list[" + (elem ? elem->toString() : "unknown") + "]"; }
bool ListType::equals(const TypeNode& o) const {
    if (o.kind != List) return false;
    return typeEquals(elem, static_cast<const ListType&>(o).elem);
}

// â”€â”€â”€ DictType â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
std::string DictType::toString() const {
    return "dict[" + (key?key->toString():"unknown") + ", " + (value?value->toString():"unknown") + "]";
}
bool DictType::equals(const TypeNode& o) const {
    if (o.kind != Dict) return false;
    auto& d = static_cast<const DictType&>(o);
    return typeEquals(key, d.key) && typeEquals(value, d.value);
}

// â”€â”€â”€ TupleType â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
std::string TupleType::toString() const {
    std::string s = "tuple[";
    for (size_t i = 0; i < elems.size(); i++) { if (i) s += ", "; s += elems[i]?elems[i]->toString():"unknown"; }
    return s + "]";
}
bool TupleType::equals(const TypeNode& o) const {
    if (o.kind != Tuple) return false;
    auto& t = static_cast<const TupleType&>(o);
    if (elems.size() != t.elems.size()) return false;
    for (size_t i = 0; i < elems.size(); i++) if (!typeEquals(elems[i], t.elems[i])) return false;
    return true;
}

// â”€â”€â”€ FuncType â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
std::string FuncType::toString() const {
    std::string s = "func " + name + "(";
    for (size_t i = 0; i < paramTypes.size(); i++) { if (i) s += ", "; s += paramTypes[i]?paramTypes[i]->toString():"unknown"; }
    return s + ") -> " + (retType?retType->toString():"void");
}
std::string FuncType::toUserString() const {
    std::string s = "(";
    for (size_t i = 0; i < paramTypes.size(); i++) { if (i) s += ", "; s += paramTypes[i]?paramTypes[i]->toUserString():"unknown"; }
    return s + ") -> " + (retType?retType->toUserString():"void");
}
bool FuncType::equals(const TypeNode& o) const {
    if (o.kind != Func) return false;
    auto& f = static_cast<const FuncType&>(o);
    if (name != f.name || !typeEquals(retType, f.retType)) return false;
    if (paramTypes.size() != f.paramTypes.size()) return false;
    for (size_t i = 0; i < paramTypes.size(); i++) if (!typeEquals(paramTypes[i], f.paramTypes[i])) return false;
    return true;
}

// â”€â”€â”€ Other TypeNode subclasses â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
bool ClassType::equals(const TypeNode& o) const {
    return o.kind == Class && name == static_cast<const ClassType&>(o).name;
}
bool InterfaceTypeNode::equals(const TypeNode& o) const {
    return o.kind == Interface && name == static_cast<const InterfaceTypeNode&>(o).name;
}
bool TypeVarNode::equals(const TypeNode& o) const {
    return o.kind == TypeVar && name == static_cast<const TypeVarNode&>(o).name;
}
std::string UnionType::toString() const {
    std::string s;
    for (size_t i = 0; i < members.size(); i++) { if (i) s += " | "; s += members[i]?members[i]->toString():"unknown"; }
    return s;
}
bool UnionType::equals(const TypeNode& o) const {
    if (o.kind != Union) return false;
    auto& u = static_cast<const UnionType&>(o);
    if (members.size() != u.members.size()) return false;
    for (size_t i = 0; i < members.size(); i++) if (!typeEquals(members[i], u.members[i])) return false;
    return true;
}
std::string NullableType::toString() const { return (inner?inner->toString():"unknown") + " | null"; }
bool NullableType::equals(const TypeNode& o) const {
    if (o.kind != Nullable) return false;
    return typeEquals(inner, static_cast<const NullableType&>(o).inner);
}

// â”€â”€â”€ Factory functions â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
TypeRef makeInt()     { return std::make_shared<PrimitiveType>(TypeNode::Int); }
TypeRef makeFloat()   { return std::make_shared<PrimitiveType>(TypeNode::Float); }
TypeRef makeStr()     { return std::make_shared<PrimitiveType>(TypeNode::Str); }
TypeRef makeBool()    { return std::make_shared<PrimitiveType>(TypeNode::Bool); }
TypeRef makeVoid()    { return std::make_shared<PrimitiveType>(TypeNode::Void); }
TypeRef makeNull()    { return std::make_shared<PrimitiveType>(TypeNode::Null); }
TypeRef makeUnknown() { return std::make_shared<PrimitiveType>(TypeNode::Unknown); }
TypeRef makeList(TypeRef e) { return std::make_shared<ListType>(std::move(e)); }
TypeRef makeDict(TypeRef k, TypeRef v) { return std::make_shared<DictType>(std::move(k),std::move(v)); }
TypeRef makeTuple(std::vector<TypeRef> e) { return std::make_shared<TupleType>(std::move(e)); }
TypeRef makeFunc(const std::string& n, TypeRef r, std::vector<TypeRef> p) {
    return std::make_shared<FuncType>(n,std::move(r),std::move(p));
}
TypeRef makeClass(const std::string& n)     { return std::make_shared<ClassType>(n); }
TypeRef makeInterface(const std::string& n) { return std::make_shared<InterfaceTypeNode>(n); }
TypeRef makeTypeVar(const std::string& n)   { return std::make_shared<TypeVarNode>(n); }
TypeRef makeUnion(std::vector<TypeRef> m) { return std::make_shared<UnionType>(std::move(m)); }
TypeRef makeNullable(TypeRef i) { return std::make_shared<NullableType>(std::move(i)); }
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// SymbolTable
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

void SymbolTable::enterScope() {
    scopes_.emplace_back();
}

void SymbolTable::exitScope() {
    if (!scopes_.empty()) scopes_.pop_back();
}

void SymbolTable::declare(const std::string& name, const TypeRef& type) {
    if (!scopes_.empty()) {
        scopes_.back()[name] = type;
    }
}

bool SymbolTable::isDeclared(const std::string& name) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        if (it->count(name)) return true;
    }
    return false;
}

TypeRef SymbolTable::lookup(const std::string& name) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return found->second;
    }
    return makeUnknown();
}

bool SymbolTable::lookupInCurrentScope(const std::string& name, TypeRef& out) const {
    if (scopes_.empty()) return false;
    auto it = scopes_.back().find(name);
    if (it != scopes_.back().end()) {
        out = it->second;
        return true;
    }
    return false;
}

std::vector<std::string> SymbolTable::allNames() const {
    std::vector<std::string> names;
    for (const auto& scope : scopes_) {
        for (const auto& kv : scope) {
            names.push_back(kv.first);
        }
    }
    return names;
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// TypeChecker â€” construction
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

TypeChecker::TypeChecker(const std::string& filename)
    : filename_(filename), currentReturnType_(makeVoid()) {}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Scope helpers
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

void TypeChecker::enterScope() { symbols_.enterScope(); }
void TypeChecker::exitScope()  { symbols_.exitScope(); }

void TypeChecker::declare(const std::string& name, const TypeRef& type, int line, int col) {
    (void)line; (void)col;
    symbols_.declare(name, type);
}

TypeRef TypeChecker::lookup(const std::string& name, int line, int col) {
    if (!symbols_.isDeclared(name)) {
        // Builtin functions are always valid as callees — don't report them as undefined.
        if (isBuiltinFunction(name)) {
            return makeUnknown();
        }
        // Suggest a similar name in scope via Levenshtein distance.
        std::string hint;
        auto candidates = symbols_.allNames();
        std::string bestName;
        int bestDist = INT_MAX;
        for (const auto& candidate : candidates) {
            int dist = levenshtein(name, candidate);
            if (dist <= 2 && dist < bestDist) {
                bestDist = dist;
                bestName = candidate;
            }
        }
        if (!bestName.empty()) {
            hint = "did you mean '" + bestName + "'?";
        }
        error("Undefined variable '" + name + "'", line, col, hint);
    }
    return symbols_.lookup(name);
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Type resolution from annotation strings
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

TypeRef TypeChecker::resolveTypeName(const std::string& name) const {
    // Function types: (int,str)->bool
    if (!name.empty() && name[0] == '(') {
        auto arrowPos = name.find(")->");
        if (arrowPos != std::string::npos) {
            std::string paramPart = name.substr(1, arrowPos - 1);
            std::string retPart = name.substr(arrowPos + 3);
            TypeRef retType = resolveTypeName(retPart);

            std::vector<TypeRef> paramTypes;
            if (!paramPart.empty()) {
                // Split on commas (simple split â€” no nested commas expected)
                size_t start = 0;
                while (start < paramPart.size()) {
                    auto comma = paramPart.find(',', start);
                    if (comma == std::string::npos) comma = paramPart.size();
                    paramTypes.push_back(resolveTypeName(paramPart.substr(start, comma - start)));
                    start = comma + 1;
                }
            }
            return makeFunc("<fn_type>", retType, std::move(paramTypes));
        }
    }

    // Union types: "int|null"
    auto pipePos = name.find('|');
    if (pipePos != std::string::npos) {
        std::vector<TypeRef> members;
        size_t start = 0;
        while (start < name.size()) {
            auto end = name.find('|', start);
            if (end == std::string::npos) end = name.size();
            members.push_back(resolveTypeName(name.substr(start, end - start)));
            start = end + 1;
        }
        return makeUnion(std::move(members));
    }

    // Generic type args: list[int]
    auto bracketPos = name.find('[');
    if (bracketPos != std::string::npos) {
        std::string base = name.substr(0, bracketPos);
        auto closeBracket = name.rfind(']');
        if (closeBracket != std::string::npos && closeBracket > bracketPos) {
            std::string inner = name.substr(bracketPos + 1, closeBracket - bracketPos - 1);
            if (base == "list") {
                return makeList(resolveTypeName(inner));
            }
            if (base == "dict") {
                auto comma = inner.find(',');
                if (comma != std::string::npos) {
                    return makeDict(
                        resolveTypeName(inner.substr(0, comma)),
                        resolveTypeName(inner.substr(comma + 1)));
                }
            }
        }
        return makeClass(base);
    }

    // Check for type variable
    for (const auto& tp : currentTypeParams_) {
        if (tp == name) return makeTypeVar(name);
    }

    if (name == "int")   return makeInt();
    if (name == "float") return makeFloat();
    if (name == "str")   return makeStr();
    if (name == "bool")  return makeBool();
    if (name == "void")  return makeVoid();
    if (name == "null")  return makeNull();

    // Check for interface
    if (interfaceTable_.count(name)) {
        return makeInterface(name);
    }

    // Treat anything else as a class type.
    return makeClass(name);
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Unification â€” resolve two types to a common type, or error.
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

TypeRef TypeChecker::unify(const TypeRef& a, const TypeRef& b, int line, int col) {
    if (a->isUnknown()) return b;
    if (b->isUnknown()) return a;
    if (typeEquals(a, b)) return a;

    // Numeric promotion: int + float â†’ float
    if (a->isNumeric() && b->isNumeric()) {
        return promoteNumeric(a, b);
    }

    // null is assignable to any reference type
    if (a->kind == TypeNode::Null || b->kind == TypeNode::Null) {
        return a->kind == TypeNode::Null ? b : a;
    }

    // TypeVar matches anything
    if (a->kind == TypeNode::TypeVar || b->kind == TypeNode::TypeVar) {
        return a->kind == TypeNode::TypeVar ? b : a;
    }

    // Subtype relationships
    if (isAssignableTo(a, b)) return b;
    if (isAssignableTo(b, a)) return a;

    // Suggest a conversion function when types are simple primitives.
    std::string hint;
    if (b->kind == TypeNode::Int && !a->isUnknown())
        hint = "did you mean to call int(x)?";
    else if (b->kind == TypeNode::Float && !a->isUnknown())
        hint = "did you mean to call float(x)?";
    else if (b->kind == TypeNode::Str && !a->isUnknown())
        hint = "did you mean to call str(x)?";
    else if (b->kind == TypeNode::Bool && !a->isUnknown())
        hint = "did you mean to call bool(x)?";
    error("Cannot assign " + a->toUserString() + " to " + b->toUserString(), line, col, hint);
    return makeUnknown();
}

TypeRef TypeChecker::promoteNumeric(const TypeRef& a, const TypeRef& b) {
    if (a->kind == TypeNode::Float || b->kind == TypeNode::Float)
        return makeFloat();
    return makeInt();
}

bool TypeChecker::isSubtype(const TypeRef& sub, const TypeRef& super) const {
    if (typeEquals(sub, super)) return true;
    if (sub->kind == TypeNode::Class && super->kind == TypeNode::Class) {
        std::string current = static_cast<const ClassType*>(sub.get())->name;
        while (!current.empty()) {
            if (current == static_cast<const ClassType*>(super.get())->name) return true;
            auto it = classTable_.find(current);
            if (it == classTable_.end()) break;
            current = it->second.baseClass;
        }
    }
    return false;
}

bool TypeChecker::isAssignableTo(const TypeRef& from, const TypeRef& to) const {
    if (typeEquals(from, to)) return true;
    if (from->isUnknown() || to->isUnknown()) return true;
    if (from->kind == TypeNode::Null) return true; // null assignable to any ref

    // TypeVar matches anything
    if (from->kind == TypeNode::TypeVar || to->kind == TypeNode::TypeVar) return true;

    // Numeric promotion (int â†’ float only)
    if (from->isNumeric() && to->isNumeric()) {
        if (from->kind == TypeNode::Int && to->kind == TypeNode::Float) return true;
    }

    // Union target: check if from is any member
    if (to->kind == TypeNode::Union) {
        for (const auto& m : static_cast<const UnionType*>(to.get())->members) {
            if (isAssignableTo(from, m)) return true;
        }
        return false;
    }

    // Class subtyping
    if (from->kind == TypeNode::Class && to->kind == TypeNode::Class) {
        return isSubtype(from, to);
    }

    // Function type compatibility: two Func types are compatible if they have
    // the same number of params and compatible param/return types.
    if (from->kind == TypeNode::Func && to->kind == TypeNode::Func) {
        if (static_cast<const FuncType*>(from.get())->paramTypes.size() != static_cast<const FuncType*>(to.get())->paramTypes.size()) return false;
        // params[0] = return type, params[1..] = parameter types
        for (size_t i = 0; i < static_cast<const FuncType*>(from.get())->paramTypes.size(); i++) {
            if (static_cast<const FuncType*>(from.get())->paramTypes[i]->isUnknown() || static_cast<const FuncType*>(to.get())->paramTypes[i]->isUnknown()) continue;
            if (!typeEquals(static_cast<const FuncType*>(from.get())->paramTypes[i], static_cast<const FuncType*>(to.get())->paramTypes[i])) return false;
        }
        return true;
    }

    // Interface: class implements interface
    if (from->kind == TypeNode::Class && to->kind == TypeNode::Interface) {
        return classImplementsInterface(static_cast<const ClassType*>(from.get())->name, static_cast<const ClassType*>(to.get())->name);
    }

    return false;
}

bool TypeChecker::classImplementsInterface(const std::string& className, const std::string& ifaceName) const {
    auto cit = classTable_.find(className);
    if (cit == classTable_.end()) return false;
    for (const auto& iface : cit->second.interfaces) {
        if (iface == ifaceName) return true;
    }
    // Check parent classes (primary base + extra bases for multiple inheritance).
    if (!cit->second.baseClass.empty()) {
        if (classImplementsInterface(cit->second.baseClass, ifaceName)) return true;
    }
    for (const auto& eb : cit->second.extraBases) {
        if (classImplementsInterface(eb, ifaceName)) return true;
    }
    return false;
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Error reporting
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

void TypeChecker::error(const std::string& msg, int line, int col,
                        const std::string& hint) {
    errors_.emplace_back(msg, filename_, line, col, hint);
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Entry point
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

void TypeChecker::check(const ast::Program& program) {
    errors_.clear();
    enterScope();  // file scope
    // First pass: register all top-level function, class, and interface declarations.
    for (const auto& stmt : program.statements) {
        if (auto* f = dynamic_cast<const ast::FuncDef*>(stmt.get())) {
            auto savedTP = currentTypeParams_;
            currentTypeParams_ = f->typeParams;
            std::vector<TypeRef> paramTypes;
            for (const auto& p : f->params) {
                paramTypes.push_back(
                    p.typeAnnotation.empty() ? makeUnknown()
                                             : resolveTypeName(p.typeAnnotation));
            }
            TypeRef retType = f->returnType.empty() ? makeUnknown()
                                                  : resolveTypeName(f->returnType);
            declare(f->name, makeFunc(f->name, retType, std::move(paramTypes)),
                    f->line, f->column);
            // Track minimum required args (excluding default, variadic, kwargs).
            size_t minArgs = 0;
            for (const auto& p : f->params)
                if (!p.defaultValue && !p.isVariadic && !p.isKwargs) minArgs++;
            funcMinArgs_[f->name] = minArgs;
            if (!f->typeParams.empty()) {
                funcTypeParams_[f->name] = f->typeParams;
            }
            currentTypeParams_ = savedTP;
        } else if (auto* c = dynamic_cast<const ast::ClassDef*>(stmt.get())) {
            declare(c->name, makeClass(c->name), c->line, c->column);
            ClassInfo info;
            info.name = c->name;
            info.baseClass = c->baseClass.value_or("");
            info.extraBases = c->extraBases;
            info.interfaces = c->interfaces;
            info.typeParams = c->typeParams;
            // Scan methods
            auto savedTP = currentTypeParams_;
            currentTypeParams_ = c->typeParams;
            for (const auto& s : c->body) {
                if (auto* m = dynamic_cast<const ast::FuncDef*>(s.get())) {
                    MethodInfo method;
                    method.name = m->name;
                    method.returnType = m->returnType.empty() ? makeUnknown()
                                                               : resolveTypeName(m->returnType);
                    for (const auto& p : m->params) {
                        method.paramTypes.push_back(
                            p.typeAnnotation.empty() ? makeUnknown()
                                                     : resolveTypeName(p.typeAnnotation));
                    }
                    info.methods.push_back(method);
                }
            }
            currentTypeParams_ = savedTP;
            classTable_[c->name] = info;
        } else if (auto* i = dynamic_cast<const ast::InterfaceDef*>(stmt.get())) {
            declare(i->name, makeInterface(i->name), i->line, i->column);
            InterfaceInfo info;
            info.name = i->name;
            for (const auto& m : i->methods) {
                MethodInfo method;
                method.name = m.name;
                method.returnType = m.returnType.empty() ? makeUnknown()
                                                          : resolveTypeName(m.returnType);
                for (const auto& p : m.params) {
                    method.paramTypes.push_back(
                        p.typeAnnotation.empty() ? makeUnknown()
                                                 : resolveTypeName(p.typeAnnotation));
                }
                info.methods.push_back(method);
            }
            interfaceTable_[i->name] = info;
        }
    }
    // Second pass: check all statements.
    checkStmtList(program.statements);
    exitScope();

    // After checking, re-throw the first error so callers see it.
    if (!errors_.empty()) throw errors_.front();
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Statement visitors
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

void TypeChecker::checkStmtList(const ast::StmtList& stmts) {
    for (const auto& s : stmts) {
        checkStmt(*s);
    }
}

// â”€â”€ checkStmt â€” delegates to visit() via double dispatch â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
void TypeChecker::checkStmt(const ast::Stmt& stmt) {
    stmt.accept(*this);
}

// â”€â”€ checkExpr â€” delegates to visit() via double dispatch â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
TypeRef TypeChecker::checkExpr(const ast::Expr& expr) {
    exprResult_ = makeUnknown();
    expr.accept(*this);
    return exprResult_;
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Statement visit() overrides
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

void TypeChecker::visit(const ast::ExprStmt& s) {
    checkExpr(*s.expr);
}

void TypeChecker::visit(const ast::AssignStmt& s) {
    TypeRef rhsType = checkExpr(*s.value);

    if (auto* id = dynamic_cast<const ast::Identifier*>(s.target.get())) {
        if (symbols_.isDeclared(id->name)) {
            TypeRef existing = symbols_.lookup(id->name);
            if (!existing->isUnknown() && !rhsType->isUnknown() && !typeEquals(existing, rhsType)) {
                if (existing->isNumeric() && rhsType->isNumeric()) {
                    if (existing->kind == TypeNode::Int && rhsType->kind == TypeNode::Float) {
                        error("Cannot assign " + rhsType->toUserString() +
                              " to " + existing->toUserString(), s.line, s.column);
                    }
                } else {
                    std::string hint;
                    if (existing->kind == TypeNode::Int) hint = "did you mean to call int(x)?";
                    else if (existing->kind == TypeNode::Str) hint = "did you mean to call str(x)?";
                    else if (existing->kind == TypeNode::Float) hint = "did you mean to call float(x)?";
                    error("Cannot assign " + rhsType->toUserString() +
                          " to " + existing->toUserString(), s.line, s.column, hint);
                }
            }
        } else {
            declare(id->name, rhsType, s.line, s.column);
        }
    } else {
        checkExpr(*s.target);
    }
}

void TypeChecker::visit(const ast::ReturnStmt& s) {
    TypeRef retType = makeVoid();
    if (s.value) {
        retType = checkExpr(*s.value);
    }
    if (hasExplicitReturnType_ && !currentReturnType_->isUnknown()) {
        if (!typeEquals(retType, currentReturnType_) && !retType->isUnknown()) {
            if (retType->isNumeric() && currentReturnType_->isNumeric()) {
                if (currentReturnType_->kind == TypeNode::Int && retType->kind == TypeNode::Float) {
                    error("Return type mismatch: expected " + currentReturnType_->toUserString() +
                          ", got " + retType->toUserString(), s.line, s.column);
                }
            } else {
                std::string hint;
                if (currentReturnType_->kind == TypeNode::Int)
                    hint = "did you mean to call int(x)?";
                else if (currentReturnType_->kind == TypeNode::Str)
                    hint = "did you mean to call str(x)?";
                else if (currentReturnType_->kind == TypeNode::Float)
                    hint = "did you mean to call float(x)?";
                error("Return type mismatch: expected " + currentReturnType_->toUserString() +
                      ", got " + retType->toUserString(), s.line, s.column, hint);
            }
        }
    }
}

void TypeChecker::visit(const ast::FuncDef& s) {
    for (const auto& dec : s.decorators) {
        // Skip type-checking decorator expressions — they are compile-time
        // annotations, not runtime expressions.  Unknown ones produce a
        // codegen-level warning instead of a type error.
        (void)dec;
        continue;
    }

    TypeRef savedRetType = currentReturnType_;
    bool savedHasExplicit = hasExplicitReturnType_;
    auto savedTP = currentTypeParams_;
    currentTypeParams_.insert(currentTypeParams_.end(),
                               s.typeParams.begin(), s.typeParams.end());

    if (!s.returnType.empty()) {
        currentReturnType_ = resolveTypeName(s.returnType);
        hasExplicitReturnType_ = true;
    } else {
        currentReturnType_ = makeUnknown();
        hasExplicitReturnType_ = false;
    }

    enterScope();
    for (const auto& tp : s.typeParams) {
        declare(tp, makeTypeVar(tp), s.line, s.column);
    }
    for (const auto& p : s.params) {
        TypeRef pType = p.typeAnnotation.empty() ? makeUnknown()
                                               : resolveTypeName(p.typeAnnotation);
        if (p.defaultValue) {
            checkExpr(*p.defaultValue);
        }
        declare(p.name, pType, s.line, s.column);
    }
    checkStmtList(s.body);

    // Check for missing return in non-void functions.
    if (hasExplicitReturnType_ &&
        currentReturnType_->kind != TypeNode::Void &&
        !currentReturnType_->isUnknown() &&
        !stmtListHasReturn(s.body)) {
        error("function '" + s.name + "' missing return statement; "
              "return type is '" + currentReturnType_->toUserString() + "'",
              s.line, s.column,
              "add 'return <value>' at the end of the function");
    }

    exitScope();

    currentReturnType_ = savedRetType;
    hasExplicitReturnType_ = savedHasExplicit;
    currentTypeParams_ = savedTP;
}

void TypeChecker::visit(const ast::InitDef& s) {
    TypeRef savedRetType = currentReturnType_;
    bool savedHasExplicit = hasExplicitReturnType_;
    currentReturnType_ = makeVoid();
    hasExplicitReturnType_ = false;

    enterScope();
    for (const auto& p : s.params) {
        TypeRef pType = p.typeAnnotation.empty() ? makeUnknown()
                                               : resolveTypeName(p.typeAnnotation);
        if (p.defaultValue) {
            checkExpr(*p.defaultValue);
        }
        declare(p.name, pType, s.line, s.column);
    }
    checkStmtList(s.body);
    exitScope();

    currentReturnType_ = savedRetType;
    hasExplicitReturnType_ = savedHasExplicit;
}

void TypeChecker::visit(const ast::ClassDef& s) {
    bool savedInsideClass = insideClass_;
    std::string savedClassName = currentClassName_;
    auto savedTP = currentTypeParams_;

    insideClass_ = true;
    currentClassName_ = s.name;
    currentTypeParams_ = s.typeParams;

    enterScope();
    declare("this", makeClass(s.name), s.line, s.column);

    for (const auto& tp : s.typeParams) {
        declare(tp, makeTypeVar(tp), s.line, s.column);
    }

    if (s.baseClass) {
        declare("super", makeClass(*s.baseClass), s.line, s.column);
    }

    checkStmtList(s.body);

    // Check override signatures
    if (s.baseClass) {
        auto baseIt = classTable_.find(*s.baseClass);
        auto childIt = classTable_.find(s.name);
        if (baseIt != classTable_.end() && childIt != classTable_.end()) {
            for (const auto& childMethod : childIt->second.methods) {
                for (const auto& baseMethod : baseIt->second.methods) {
                    if (childMethod.name == baseMethod.name) {
                        if (childMethod.paramTypes.size() != baseMethod.paramTypes.size()) {
                            error("Override '" + childMethod.name +
                                  "' has different parameter count",
                                  s.line, s.column);
                        }
                        for (size_t i = 0; i < childMethod.paramTypes.size() &&
                             i < baseMethod.paramTypes.size(); i++) {
                            if (!childMethod.paramTypes[i]->isUnknown() &&
                                !baseMethod.paramTypes[i]->isUnknown() &&
                                !typeEquals(childMethod.paramTypes[i], baseMethod.paramTypes[i])) {
                                error("Override '" + childMethod.name +
                                      "' has incompatible parameter type: expected " +
                                      baseMethod.paramTypes[i]->toString() +
                                      ", got " + childMethod.paramTypes[i]->toString(),
                                      s.line, s.column);
                            }
                        }
                        if (!childMethod.returnType->isUnknown() &&
                            !baseMethod.returnType->isUnknown() &&
                            !typeEquals(childMethod.returnType, baseMethod.returnType)) {
                            error("Override '" + childMethod.name +
                                  "' has incompatible return type: expected " +
                                  baseMethod.returnType->toString() +
                                  ", got " + childMethod.returnType->toString(),
                                  s.line, s.column);
                        }
                    }
                }
            }
        }
    }

    // Check interface implementation
    for (const auto& ifaceName : s.interfaces) {
        auto ifaceIt = interfaceTable_.find(ifaceName);
        if (ifaceIt != interfaceTable_.end()) {
            auto classIt = classTable_.find(s.name);
            if (classIt != classTable_.end()) {
                for (const auto& ifaceMethod : ifaceIt->second.methods) {
                    bool found = false;
                    for (const auto& classMethod : classIt->second.methods) {
                        if (classMethod.name == ifaceMethod.name) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        error("Class '" + s.name +
                              "' does not implement method '" +
                              ifaceMethod.name +
                              "' required by interface '" + ifaceName + "'",
                              s.line, s.column);
                    }
                }
            }
        }
    }

    exitScope();
    insideClass_ = savedInsideClass;
    currentClassName_ = savedClassName;
    currentTypeParams_ = savedTP;
}

void TypeChecker::visit(const ast::IfStmt& s) {
    TypeRef condType = checkExpr(*s.condition);
    (void)condType;

    // Nullable narrowing: analyze "x != null" or "x == null" conditions
    std::string narrowVar;
    TypeRef narrowType;      // for then-branch
    TypeRef elseNarrowType;  // for else-branch
    bool hasNarrowing = false;
    if (auto* bin = dynamic_cast<const ast::BinaryExpr*>(s.condition.get())) {
        if (bin->op == ast::BinOp::Neq || bin->op == ast::BinOp::Eq) {
            auto* ident = dynamic_cast<const ast::Identifier*>(bin->left.get());
            auto* null_ = dynamic_cast<const ast::NullLiteral*>(bin->right.get());
            if (ident && null_) {
                TypeRef varType = symbols_.lookup(ident->name);
                if (varType->kind == TypeNode::Union) {
                    std::vector<TypeRef> nonNullMembers;
                    for (const auto& m : static_cast<const UnionType*>(varType.get())->members) {
                        if (m->kind != TypeNode::Null) nonNullMembers.push_back(m);
                    }
                    TypeRef nonNullType;
                    if (nonNullMembers.size() == 1) {
                        nonNullType = nonNullMembers[0];
                    } else if (!nonNullMembers.empty()) {
                        nonNullType = makeUnion(nonNullMembers);
                    }
                    TypeRef nullType = makeNull();
                    if (bin->op == ast::BinOp::Neq) {
                        // x != null → then: non-null, else: null
                        narrowType = nonNullType;
                        elseNarrowType = nullType;
                    } else {
                        // x == null → then: null, else: non-null
                        narrowType = nullType;
                        elseNarrowType = nonNullType;
                    }
                    narrowVar = ident->name;
                    hasNarrowing = true;
                }
            }
        }
    }

    enterScope();
    if (hasNarrowing && narrowType) {
        declare(narrowVar, narrowType, s.condition->line, s.condition->column);
    }
    checkStmtList(s.thenBranch);
    exitScope();

    for (const auto& elsif : s.elsifBranches) {
        checkExpr(*elsif.first);
        enterScope();
        checkStmtList(elsif.second);
        exitScope();
    }

    if (!s.elseBranch.empty()) {
        enterScope();
        if (hasNarrowing && elseNarrowType) {
            declare(narrowVar, elseNarrowType, s.condition->line, s.condition->column);
        }
        checkStmtList(s.elseBranch);
        exitScope();
    }
}

void TypeChecker::visit(const ast::ForStmt& s) {
    TypeRef iterType = checkExpr(*s.iterable);
    enterScope();
    TypeRef elemType = makeUnknown();
    if (iterType->kind == TypeNode::List && true) {
        elemType = static_cast<const ListType*>(iterType.get())->elem;
    }
    declare(s.variable, elemType, s.line, s.column);
    checkStmtList(s.body);
    exitScope();
}

void TypeChecker::visit(const ast::WhileStmt& s) {
    checkExpr(*s.condition);
    enterScope();
    checkStmtList(s.body);
    exitScope();
}

void TypeChecker::visit(const ast::WithStmt& s) {
    // Type-check the context expression.
    checkExpr(*s.expr);

    enterScope();
    // If there is an 'as' binding, declare it with unknown type
    // (we can't know __enter__'s return type without full method lookup).
    if (!s.asName.empty()) {
        declare(s.asName, makeUnknown(), s.line, s.column);
    }
    checkStmtList(s.body);
    exitScope();
}

void TypeChecker::visit(const ast::TryStmt& s) {
    enterScope();
    checkStmtList(s.tryBody);
    exitScope();

    for (const auto& c : s.catchClauses) {
        enterScope();
        if (!c.varName.empty()) {
            TypeRef excType = c.exceptionType.empty()
                               ? makeUnknown()
                               : resolveTypeName(c.exceptionType);
            declare(c.varName, excType, s.line, s.column);
        }
        checkStmtList(c.body);
        exitScope();
    }

    if (!s.finallyBody.empty()) {
        enterScope();
        checkStmtList(s.finallyBody);
        exitScope();
    }
}

void TypeChecker::visit(const ast::ThrowStmt& s) {
    if (s.expr)
        checkExpr(*s.expr.value());
}

void TypeChecker::visit(const ast::AssertStmt& s) {
    TypeRef condType = checkExpr(*s.condition);
    if (condType->kind != TypeNode::Kind::Bool && condType->kind != TypeNode::Kind::Unknown) {
        throw TypeError("Assert condition must be bool, got '" + condType->toString() + "'",
                        filename_, s.line, s.column);
    }
    if (s.message) {
        TypeRef msgType = checkExpr(*s.message);
        if (msgType->kind != TypeNode::Kind::Str && msgType->kind != TypeNode::Kind::Unknown) {
            throw TypeError("Assert message must be str, got '" + msgType->toString() + "'",
                            filename_, s.line, s.column);
        }
    }
}

void TypeChecker::visit(const ast::DelStmt& s) {
    // Target must be an Identifier, IndexExpr, or MemberExpr.
    if (!dynamic_cast<const ast::Identifier*>(s.target.get()) &&
        !dynamic_cast<const ast::IndexExpr*>(s.target.get()) &&
        !dynamic_cast<const ast::MemberExpr*>(s.target.get())) {
        throw TypeError("del target must be an identifier, index expression, or member expression",
                        filename_, s.line, s.column);
    }
    // Typecheck the target expression (any type is allowed).
    checkExpr(*s.target);
}

void TypeChecker::visit(const ast::MatchStmt& s) {
    TypeRef subjectType = checkExpr(*s.subject);

    for (const auto& c : s.cases) {
        if (c.pattern) {
            // Non-wildcard: pattern must be a literal compatible with subject type
            TypeRef patternType = checkExpr(*c.pattern);
            if (subjectType->kind != TypeNode::Kind::Unknown &&
                patternType->kind != TypeNode::Kind::Unknown &&
                subjectType->kind != patternType->kind) {
                throw TypeError(
                    "Match case pattern type '" + patternType->toString() +
                    "' is incompatible with subject type '" + subjectType->toString() + "'",
                    filename_, s.line, s.column);
            }
        }
        enterScope();
        checkStmtList(c.body);
        exitScope();
    }
}

void TypeChecker::visit(const ast::ImportStmt& s) {
    std::string modName = s.module;
    auto dotPos = modName.find('.');
    if (dotPos != std::string::npos) modName = modName.substr(0, dotPos);
    declare(modName, makeUnknown(), s.line, s.column);
}

void TypeChecker::visit(const ast::FromImportStmt& s) {
    for (const auto& name : s.names) {
        declare(name, makeUnknown(), s.line, s.column);
    }
}

void TypeChecker::visit(const ast::BreakStmt&)    { /* nothing to check */ }
void TypeChecker::visit(const ast::ContinueStmt&) { /* nothing to check */ }
void TypeChecker::visit(const ast::PassStmt&)     { /* nothing to check */ }
void TypeChecker::visit(const ast::InterfaceDef&) { /* registered in first pass */ }

void TypeChecker::visit(const ast::TupleUnpackStmt& s) {
    TypeRef rhsType = checkExpr(*s.value);
    for (const auto& target : s.targets) {
        TypeRef elemType = makeUnknown();
        (void)rhsType;
        declare(target, elemType, s.line, s.column);
    }
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Expression visit() overrides
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

void TypeChecker::visit(const ast::IntLiteral&)    { exprResult_ = makeInt(); }
void TypeChecker::visit(const ast::FloatLiteral&)  { exprResult_ = makeFloat(); }
void TypeChecker::visit(const ast::StringLiteral&) { exprResult_ = makeStr(); }
void TypeChecker::visit(const ast::FStringLiteral&){ exprResult_ = makeStr(); }
void TypeChecker::visit(const ast::FStringExpr&)   { exprResult_ = makeUnknown(); }
void TypeChecker::visit(const ast::BoolLiteral&)   { exprResult_ = makeBool(); }
void TypeChecker::visit(const ast::NullLiteral&)   { exprResult_ = makeNull(); }

void TypeChecker::visit(const ast::Identifier& e) {
    exprResult_ = lookup(e.name, e.line, e.column);
}

void TypeChecker::visit(const ast::ThisExpr& e) {
    if (!insideClass_) {
        error("'this' used outside of class", e.line, e.column);
    }
    exprResult_ = lookup("this", e.line, e.column);
}

void TypeChecker::visit(const ast::SuperExpr& e) {
    if (!insideClass_) {
        error("'super' used outside of class", e.line, e.column);
        exprResult_ = makeUnknown();
        return;
    }
    auto clsIt = classTable_.find(currentClassName_);
    if (currentClassName_.empty() || clsIt == classTable_.end() ||
        clsIt->second.baseClass.empty()) {
        error("'super' used in class without base class", e.line, e.column);
        exprResult_ = makeUnknown();
        return;
    }
    exprResult_ = makeClass(clsIt->second.baseClass);
}

void TypeChecker::visit(const ast::BinaryExpr& e) {
    TypeRef lt = checkExpr(*e.left);
    TypeRef rt = checkExpr(*e.right);

    switch (e.op) {
        case ast::BinOp::Add:
            if (lt->kind == TypeNode::Str && rt->kind == TypeNode::Str) { exprResult_ = makeStr(); return; }
            if (lt->isNumeric() && rt->isNumeric()) { exprResult_ = promoteNumeric(lt, rt); return; }
            if (!lt->isUnknown() && !rt->isUnknown()) {
                error("Cannot apply '+' to " + lt->toString() + " and " + rt->toString(),
                      e.line, e.column);
            }
            exprResult_ = makeUnknown(); return;

        case ast::BinOp::Sub:
        case ast::BinOp::Mul:
        case ast::BinOp::Div:
        case ast::BinOp::Mod:
        case ast::BinOp::Pow:
            if (lt->isNumeric() && rt->isNumeric()) { exprResult_ = promoteNumeric(lt, rt); return; }
            if (!lt->isUnknown() && !rt->isUnknown()) {
                error("Cannot apply arithmetic to " +
                      lt->toString() + " and " + rt->toString(), e.line, e.column);
            }
            exprResult_ = makeUnknown(); return;

        case ast::BinOp::IntDiv:
            if (lt->isNumeric() && rt->isNumeric()) { exprResult_ = makeInt(); return; }
            if (!lt->isUnknown() && !rt->isUnknown()) {
                error("Cannot apply '//' to " + lt->toString() + " and " + rt->toString(),
                      e.line, e.column);
            }
            exprResult_ = makeUnknown(); return;

        case ast::BinOp::Eq:
        case ast::BinOp::Neq:
            exprResult_ = makeBool(); return;

        case ast::BinOp::Lt:
        case ast::BinOp::Gt:
        case ast::BinOp::Lte:
        case ast::BinOp::Gte:
            if (lt->isNumeric() && rt->isNumeric()) { exprResult_ = makeBool(); return; }
            if (lt->kind == TypeNode::Str && rt->kind == TypeNode::Str) { exprResult_ = makeBool(); return; }
            if (!lt->isUnknown() && !rt->isUnknown()) {
                error("Cannot compare " + lt->toString() + " and " + rt->toString(),
                      e.line, e.column);
            }
            exprResult_ = makeBool(); return;

        case ast::BinOp::And:
        case ast::BinOp::Or:
            exprResult_ = makeBool(); return;

        case ast::BinOp::In:
        case ast::BinOp::NotIn:
            exprResult_ = makeBool(); return;

        case ast::BinOp::BitAnd:
        case ast::BinOp::BitOr:
        case ast::BinOp::BitXor:
        case ast::BinOp::Shl:
        case ast::BinOp::Shr:
            if ((lt->kind == TypeNode::Int || lt->kind == TypeNode::Bool) &&
                (rt->kind == TypeNode::Int || rt->kind == TypeNode::Bool)) {
                exprResult_ = makeInt(); return;
            }
            if (lt->kind == TypeNode::Float || rt->kind == TypeNode::Float) {
                error("Cannot apply bitwise operator to float", e.line, e.column);
            } else if (!lt->isUnknown() && !rt->isUnknown()) {
                error("Cannot apply bitwise operator to " +
                      lt->toString() + " and " + rt->toString(), e.line, e.column);
            }
            exprResult_ = makeUnknown(); return;

        case ast::BinOp::NullCoalesce:
            // LHS ?? RHS → result is LHS (non-null) or RHS type
            exprResult_ = rt; return;
    }
    exprResult_ = makeUnknown();
}

void TypeChecker::visit(const ast::UnaryExpr& e) {
    TypeRef operandType = checkExpr(*e.operand);
    if (e.op == ast::UnaryOp::Neg) {
        if (operandType->isNumeric()) { exprResult_ = operandType; return; }
        if (!operandType->isUnknown()) {
            error("Cannot negate " + operandType->toString(), e.line, e.column);
        }
        exprResult_ = makeUnknown(); return;
    }
    if (e.op == ast::UnaryOp::BitNot) {
        if (operandType->kind == TypeNode::Int) { exprResult_ = makeInt(); return; }
        if (operandType->kind == TypeNode::Float) {
            error("Cannot apply ~ to float", e.line, e.column);
        } else if (!operandType->isUnknown()) {
            error("Cannot apply ~ to " + operandType->toString(), e.line, e.column);
        }
        exprResult_ = makeUnknown(); return;
    }
    // Not
    exprResult_ = makeBool();
}

void TypeChecker::visit(const ast::CallExpr& e) {
    // ── Handle builtin functions before resolving the callee identifier ──
    if (auto* id = dynamic_cast<const ast::Identifier*>(e.callee.get())) {
        // map(fn, list) -> list[unknown]
        if (id->name == "map") {
            for (const auto& a : e.args) checkExpr(*a);
            exprResult_ = makeList(makeUnknown());
            return;
        }
        // filter(fn, list) -> list[unknown]
        if (id->name == "filter") {
            for (const auto& a : e.args) checkExpr(*a);
            exprResult_ = makeList(makeUnknown());
            return;
        }
        if (id->name == "input") {
            if (e.args.size() > 1) {
                error("input() accepts 0 or 1 str argument, got " +
                      std::to_string(e.args.size()), e.line, e.column);
            }
            for (const auto& a : e.args) {
                TypeRef argType = checkExpr(*a);
                if (!argType->isUnknown() && argType->kind != TypeNode::Str) {
                    error("input() argument must be str, got " + argType->toString(),
                          e.line, e.column);
                }
            }
            exprResult_ = makeStr();
            return;
        }
    }

    TypeRef calleeType = checkExpr(*e.callee);

    std::vector<TypeRef> argTypes;
    for (const auto& a : e.args) {
        argTypes.push_back(checkExpr(*a));
    }

    if (calleeType->kind == TypeNode::Func) {
        std::string funcName = static_cast<const ClassType*>(calleeType.get())->name;
        auto tpIt = funcTypeParams_.find(funcName);

        if (tpIt != funcTypeParams_.end() && !tpIt->second.empty()) {
            const auto& typeParamNames = tpIt->second;
            std::unordered_map<std::string, TypeRef> typeArgMap;

            if (!e.typeArgs.empty()) {
                for (size_t i = 0; i < typeParamNames.size() && i < e.typeArgs.size(); i++) {
                    typeArgMap[typeParamNames[i]] = resolveTypeName(e.typeArgs[i]);
                }
            } else {
                for (size_t i = 0; i < argTypes.size() && i + 1 < (1 + static_cast<const FuncType*>(calleeType.get())->paramTypes.size()); i++) {
                    const TypeRef& paramType = static_cast<const FuncType*>(calleeType.get())->paramTypes[i];
                    if (paramType->kind == TypeNode::TypeVar) {
                        typeArgMap[static_cast<const TypeVarNode*>(paramType.get())->name] = argTypes[i];
                    }
                }
            }

            TypeRef retType = static_cast<const FuncType*>(calleeType.get())->retType;
            if (retType->kind == TypeNode::TypeVar) {
                auto it = typeArgMap.find(static_cast<const TypeVarNode*>(retType.get())->name);
                if (it != typeArgMap.end()) retType = it->second;
            }

            size_t expectedArgs = (static_cast<const FuncType*>(calleeType.get())->paramTypes.size());
            size_t minArgs = funcMinArgs_.count(funcName) ? funcMinArgs_.at(funcName) : expectedArgs;
            if (argTypes.size() < minArgs || argTypes.size() > expectedArgs) {
                error("Function '" + funcName + "' expects " +
                      std::to_string(expectedArgs) + " argument(s), got " +
                      std::to_string(argTypes.size()), e.line, e.column);
            }

            exprResult_ = retType; return;
        }

        // Non-generic function
        size_t expectedArgs = (static_cast<const FuncType*>(calleeType.get())->paramTypes.size());
        size_t minArgs = funcMinArgs_.count(funcName) ? funcMinArgs_.at(funcName) : expectedArgs;
        if (argTypes.size() < minArgs || argTypes.size() > expectedArgs) {
            error("Function '" + static_cast<const ClassType*>(calleeType.get())->name + "' expects " +
                  std::to_string(expectedArgs) + " argument(s), got " +
                  std::to_string(argTypes.size()), e.line, e.column);
        }
        for (size_t i = 0; i < argTypes.size() && i + 1 < (1 + static_cast<const FuncType*>(calleeType.get())->paramTypes.size()); i++) {
            const TypeRef& expected = static_cast<const FuncType*>(calleeType.get())->paramTypes[i];
            const TypeRef& actual = argTypes[i];
            if (!expected->isUnknown() && !actual->isUnknown() && expected != actual) {
                if (!isAssignableTo(actual, expected)) {
                    error("Function '" + static_cast<const ClassType*>(calleeType.get())->name + "' expects " +
                          expected->toString() + ", got " + actual->toString(),
                          e.line, e.column);
                }
            }
        }
        exprResult_ = static_cast<const FuncType*>(calleeType.get())->retType;
        return;
    }

    if (calleeType->kind == TypeNode::Class) {
        exprResult_ = calleeType; return;
    }

    if (!calleeType->isUnknown()) {
        if (auto* id = dynamic_cast<const ast::Identifier*>(e.callee.get())) {
            error("Cannot call non-function '" + id->name + "'", e.line, e.column);
        } else {
            error("Cannot call non-function type " + calleeType->toString(), e.line, e.column);
        }
    }

    exprResult_ = makeUnknown();
}

void TypeChecker::visit(const ast::MemberExpr& e) {
    checkExpr(*e.object);
    exprResult_ = makeUnknown();
}

void TypeChecker::visit(const ast::IndexExpr& e) {
    TypeRef objType = checkExpr(*e.object);
    TypeRef idxType = checkExpr(*e.index);
    (void)idxType;

    if (objType->kind == TypeNode::List) {
        exprResult_ = static_cast<const ListType*>(objType.get())->elem; return;
    }
    if (objType->kind == TypeNode::Dict && (objType->kind == TypeNode::Dict)) {
        exprResult_ = static_cast<const DictType*>(objType.get())->value; return;
    }
    if (objType->kind == TypeNode::Str) {
        exprResult_ = makeStr(); return;
    }
    exprResult_ = makeUnknown();
}

void TypeChecker::visit(const ast::LambdaExpr& e) {
    enterScope();
    std::vector<TypeRef> paramTypes;
    for (const auto& p : e.params) {
        declare(p, makeUnknown(), e.line, e.column);
        paramTypes.push_back(makeUnknown());
    }
    TypeRef bodyType = checkExpr(*e.body);
    exitScope();
    exprResult_ = makeFunc("<lambda>", bodyType, std::move(paramTypes));
}

void TypeChecker::visit(const ast::ListExpr& e) {
    if (e.elements.empty()) {
        exprResult_ = makeList(makeUnknown()); return;
    }
    TypeRef elemType = checkExpr(*e.elements[0]);
    for (size_t i = 1; i < e.elements.size(); i++) {
        TypeRef t = checkExpr(*e.elements[i]);
        elemType = unify(elemType, t, e.line, e.column);
    }
    exprResult_ = makeList(elemType);
}

void TypeChecker::visit(const ast::DictExpr& e) {
    if (e.entries.empty()) {
        exprResult_ = makeDict(makeUnknown(), makeUnknown()); return;
    }
    TypeRef keyType = checkExpr(*e.entries[0].first);
    TypeRef valType = checkExpr(*e.entries[0].second);
    for (size_t i = 1; i < e.entries.size(); i++) {
        TypeRef kt = checkExpr(*e.entries[i].first);
        TypeRef vt = checkExpr(*e.entries[i].second);
        keyType = unify(keyType, kt, e.line, e.column);
        valType = unify(valType, vt, e.line, e.column);
    }
    exprResult_ = makeDict(keyType, valType);
}

void TypeChecker::visit(const ast::TupleExpr& e) {
    std::vector<TypeRef> elemTypes;
    for (const auto& el : e.elements) {
        elemTypes.push_back(checkExpr(*el));
    }
    exprResult_ = makeTuple(std::move(elemTypes));
}

void TypeChecker::visit(const ast::TernaryExpr& e) {
    checkExpr(*e.condition);
    TypeRef thenType = checkExpr(*e.thenExpr);
    TypeRef elseType = checkExpr(*e.elseExpr);
    exprResult_ = unify(thenType, elseType, e.line, e.column);
}

void TypeChecker::visit(const ast::SliceExpr& e) {
    TypeRef objType = checkExpr(*e.object);
    if (e.start) checkExpr(*e.start);
    if (e.stop)  checkExpr(*e.stop);
    if (e.step)  checkExpr(*e.step);
    if (objType->kind == TypeNode::List) { exprResult_ = objType; return; }
    if (objType->kind == TypeNode::Str)  { exprResult_ = makeStr(); return; }
    exprResult_ = makeUnknown();
}

void TypeChecker::visit(const ast::ListComprehension& e) {
    TypeRef iterType = checkExpr(*e.iterable);
    enterScope();
    TypeRef elemType = makeUnknown();
    if (iterType->kind == TypeNode::List && true) {
        elemType = static_cast<const ListType*>(iterType.get())->elem;
    }
    declare(e.variable, elemType, e.line, e.column);
    if (e.condition) checkExpr(*e.condition);
    TypeRef bodyType = checkExpr(*e.body);
    exitScope();
    exprResult_ = makeList(bodyType);
}

void TypeChecker::visit(const ast::DictComprehension& e) {
    TypeRef iterType = checkExpr(*e.iterable);
    enterScope();
    TypeRef elemType = makeUnknown();
    if (iterType->kind == TypeNode::List && true) {
        elemType = static_cast<const ListType*>(iterType.get())->elem;
    }
    declare(e.variable, elemType, e.line, e.column);
    if (e.condition) checkExpr(*e.condition);
    TypeRef keyType = checkExpr(*e.key);
    TypeRef valType = checkExpr(*e.value);
    exitScope();
    exprResult_ = makeDict(keyType, valType);
}

void TypeChecker::visit(const ast::SpreadExpr& e) {
    exprResult_ = checkExpr(*e.operand);
}

void TypeChecker::visit(const ast::DictSpreadExpr& e) {
    exprResult_ = checkExpr(*e.operand);
}

void TypeChecker::visit(const ast::WalrusExpr& e) {
    TypeRef valType = checkExpr(*e.value);
    // Declare/update the target variable in the current scope.
    declare(e.target, valType, e.line, e.column);
    exprResult_ = valType;
}

} // namespace visuall
