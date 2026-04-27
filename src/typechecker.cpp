#include "typechecker.h"
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

bool Type::operator==(const Type& o) const {
    if (kind != o.kind) return false;
    if (kind == Class || kind == Func || kind == Interface || kind == TypeVar) {
        if (name != o.name) return false;
    }
    if (params.size() != o.params.size()) return false;
    for (size_t i = 0; i < params.size(); i++) {
        if (!(params[i] == o.params[i])) return false;
    }
    return true;
}

std::string Type::toString() const {
    switch (kind) {
        case Int:     return "int";
        case Float:   return "float";
        case Str:     return "str";
        case Bool:    return "bool";
        case Void:    return "void";
        case Null:    return "null";
        case Unknown: return "unknown";
        case List:
            return "list[" + (params.empty() ? "unknown" : params[0].toString()) + "]";
        case Dict:
            return "dict[" +
                   (params.size() > 0 ? params[0].toString() : "unknown") + ", " +
                   (params.size() > 1 ? params[1].toString() : "unknown") + "]";
        case Tuple: {
            std::string s = "tuple[";
            for (size_t i = 0; i < params.size(); i++) {
                if (i > 0) s += ", ";
                s += params[i].toString();
            }
            return s + "]";
        }
        case Func: {
            std::string s = "func " + name + "(";
            for (size_t i = 1; i < params.size(); i++) {
                if (i > 1) s += ", ";
                s += params[i].toString();
            }
            s += ") -> ";
            s += params.empty() ? "void" : params[0].toString();
            return s;
        }
        case Class:
            return name;
        case Union: {
            std::string s;
            for (size_t i = 0; i < params.size(); i++) {
                if (i > 0) s += " | ";
                s += params[i].toString();
            }
            return s;
        }
        case Interface:
            return name;
        case TypeVar:
            return name;
    }
    return "unknown";
}

// toUserString — user-facing Visuall type syntax.
// Identical to toString() except Func uses (p1, p2) -> ret notation.
std::string Type::toUserString() const {
    switch (kind) {
        case Int:     return "int";
        case Float:   return "float";
        case Str:     return "str";
        case Bool:    return "bool";
        case Void:    return "void";
        case Null:    return "null";
        case Unknown: return "unknown";
        case List:
            return "list[" + (params.empty() ? "unknown" : params[0].toUserString()) + "]";
        case Dict:
            return "dict[" +
                   (params.size() > 0 ? params[0].toUserString() : "unknown") + ", " +
                   (params.size() > 1 ? params[1].toUserString() : "unknown") + "]";
        case Tuple: {
            std::string s = "tuple[";
            for (size_t i = 0; i < params.size(); i++) {
                if (i > 0) s += ", ";
                s += params[i].toUserString();
            }
            return s + "]";
        }
        case Func: {
            // User-facing: (int, str) -> bool  (omit internal name)
            std::string s = "(";
            for (size_t i = 1; i < params.size(); i++) {
                if (i > 1) s += ", ";
                s += params[i].toUserString();
            }
            s += ") -> ";
            s += params.empty() ? "void" : params[0].toUserString();
            return s;
        }
        case Class:
            return name;
        case Union: {
            std::string s;
            for (size_t i = 0; i < params.size(); i++) {
                if (i > 0) s += " | ";
                s += params[i].toUserString();
            }
            return s;
        }
        case Interface:
            return name;
        case TypeVar:
            return name;
    }
    return "unknown";
}

Type Type::makeFunc(const std::string& n, Type ret, std::vector<Type> paramTypes) {
    // params[0] = return type, params[1..] = parameter types
    std::vector<Type> all;
    all.push_back(std::move(ret));
    for (auto& p : paramTypes) all.push_back(std::move(p));
    return Type(Func, n, std::move(all));
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// SymbolTable
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

void SymbolTable::enterScope() {
    scopes_.emplace_back();
}

void SymbolTable::exitScope() {
    if (!scopes_.empty()) scopes_.pop_back();
}

void SymbolTable::declare(const std::string& name, const Type& type) {
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

Type SymbolTable::lookup(const std::string& name) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return found->second;
    }
    return Type::makeUnknown();
}

bool SymbolTable::lookupInCurrentScope(const std::string& name, Type& out) const {
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
    : filename_(filename), currentReturnType_(Type::makeVoid()) {}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Scope helpers
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

void TypeChecker::enterScope() { symbols_.enterScope(); }
void TypeChecker::exitScope()  { symbols_.exitScope(); }

void TypeChecker::declare(const std::string& name, const Type& type, int line, int col) {
    (void)line; (void)col;
    symbols_.declare(name, type);
}

Type TypeChecker::lookup(const std::string& name, int line, int col) {
    if (!symbols_.isDeclared(name)) {
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

Type TypeChecker::resolveTypeName(const std::string& name) const {
    // Function types: (int,str)->bool
    if (!name.empty() && name[0] == '(') {
        auto arrowPos = name.find(")->");
        if (arrowPos != std::string::npos) {
            std::string paramPart = name.substr(1, arrowPos - 1);
            std::string retPart = name.substr(arrowPos + 3);
            Type retType = resolveTypeName(retPart);

            std::vector<Type> paramTypes;
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
            return Type::makeFunc("<fn_type>", retType, std::move(paramTypes));
        }
    }

    // Union types: "int|null"
    auto pipePos = name.find('|');
    if (pipePos != std::string::npos) {
        std::vector<Type> members;
        size_t start = 0;
        while (start < name.size()) {
            auto end = name.find('|', start);
            if (end == std::string::npos) end = name.size();
            members.push_back(resolveTypeName(name.substr(start, end - start)));
            start = end + 1;
        }
        return Type(Type::Union, std::move(members));
    }

    // Generic type args: list[int]
    auto bracketPos = name.find('[');
    if (bracketPos != std::string::npos) {
        std::string base = name.substr(0, bracketPos);
        auto closeBracket = name.rfind(']');
        if (closeBracket != std::string::npos && closeBracket > bracketPos) {
            std::string inner = name.substr(bracketPos + 1, closeBracket - bracketPos - 1);
            if (base == "list") {
                return Type::makeList(resolveTypeName(inner));
            }
            if (base == "dict") {
                auto comma = inner.find(',');
                if (comma != std::string::npos) {
                    return Type::makeDict(
                        resolveTypeName(inner.substr(0, comma)),
                        resolveTypeName(inner.substr(comma + 1)));
                }
            }
        }
        return Type::makeClass(base);
    }

    // Check for type variable
    for (const auto& tp : currentTypeParams_) {
        if (tp == name) return Type::makeTypeVar(name);
    }

    if (name == "int")   return Type::makeInt();
    if (name == "float") return Type::makeFloat();
    if (name == "str")   return Type::makeStr();
    if (name == "bool")  return Type::makeBool();
    if (name == "void")  return Type::makeVoid();
    if (name == "null")  return Type::makeNull();

    // Check for interface
    if (interfaceTable_.count(name)) {
        return Type::makeInterface(name);
    }

    // Treat anything else as a class type.
    return Type::makeClass(name);
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Unification â€” resolve two types to a common type, or error.
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

Type TypeChecker::unify(const Type& a, const Type& b, int line, int col) {
    if (a.isUnknown()) return b;
    if (b.isUnknown()) return a;
    if (a == b) return a;

    // Numeric promotion: int + float â†’ float
    if (a.isNumeric() && b.isNumeric()) {
        return promoteNumeric(a, b);
    }

    // null is assignable to any reference type
    if (a.kind == Type::Null || b.kind == Type::Null) {
        return a.kind == Type::Null ? b : a;
    }

    // TypeVar matches anything
    if (a.kind == Type::TypeVar || b.kind == Type::TypeVar) {
        return a.kind == Type::TypeVar ? b : a;
    }

    // Subtype relationships
    if (isAssignableTo(a, b)) return b;
    if (isAssignableTo(b, a)) return a;

    // Suggest a conversion function when types are simple primitives.
    std::string hint;
    if (b.kind == Type::Int && !a.isUnknown())
        hint = "did you mean to call int(x)?";
    else if (b.kind == Type::Float && !a.isUnknown())
        hint = "did you mean to call float(x)?";
    else if (b.kind == Type::Str && !a.isUnknown())
        hint = "did you mean to call str(x)?";
    else if (b.kind == Type::Bool && !a.isUnknown())
        hint = "did you mean to call bool(x)?";
    error("Cannot assign " + a.toUserString() + " to " + b.toUserString(), line, col, hint);
    return Type::makeUnknown();
}

Type TypeChecker::promoteNumeric(const Type& a, const Type& b) {
    if (a.kind == Type::Float || b.kind == Type::Float)
        return Type::makeFloat();
    return Type::makeInt();
}

bool TypeChecker::isSubtype(const Type& sub, const Type& super) const {
    if (sub == super) return true;
    if (sub.kind == Type::Class && super.kind == Type::Class) {
        std::string current = sub.name;
        while (!current.empty()) {
            if (current == super.name) return true;
            auto it = classTable_.find(current);
            if (it == classTable_.end()) break;
            current = it->second.baseClass;
        }
    }
    return false;
}

bool TypeChecker::isAssignableTo(const Type& from, const Type& to) const {
    if (from == to) return true;
    if (from.isUnknown() || to.isUnknown()) return true;
    if (from.kind == Type::Null) return true; // null assignable to any ref

    // TypeVar matches anything
    if (from.kind == Type::TypeVar || to.kind == Type::TypeVar) return true;

    // Numeric promotion (int â†’ float only)
    if (from.isNumeric() && to.isNumeric()) {
        if (from.kind == Type::Int && to.kind == Type::Float) return true;
    }

    // Union target: check if from is any member
    if (to.kind == Type::Union) {
        for (const auto& m : to.params) {
            if (isAssignableTo(from, m)) return true;
        }
        return false;
    }

    // Class subtyping
    if (from.kind == Type::Class && to.kind == Type::Class) {
        return isSubtype(from, to);
    }

    // Function type compatibility: two Func types are compatible if they have
    // the same number of params and compatible param/return types.
    if (from.kind == Type::Func && to.kind == Type::Func) {
        if (from.params.size() != to.params.size()) return false;
        // params[0] = return type, params[1..] = parameter types
        for (size_t i = 0; i < from.params.size(); i++) {
            if (from.params[i].isUnknown() || to.params[i].isUnknown()) continue;
            if (from.params[i] != to.params[i]) return false;
        }
        return true;
    }

    // Interface: class implements interface
    if (from.kind == Type::Class && to.kind == Type::Interface) {
        return classImplementsInterface(from.name, to.name);
    }

    return false;
}

bool TypeChecker::classImplementsInterface(const std::string& className, const std::string& ifaceName) const {
    auto cit = classTable_.find(className);
    if (cit == classTable_.end()) return false;
    for (const auto& iface : cit->second.interfaces) {
        if (iface == ifaceName) return true;
    }
    // Also check parent classes
    if (!cit->second.baseClass.empty()) {
        return classImplementsInterface(cit->second.baseClass, ifaceName);
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
            std::vector<Type> paramTypes;
            for (const auto& p : f->params) {
                paramTypes.push_back(
                    p.typeAnnotation.empty() ? Type::makeUnknown()
                                             : resolveTypeName(p.typeAnnotation));
            }
            Type retType = f->returnType.empty() ? Type::makeUnknown()
                                                  : resolveTypeName(f->returnType);
            declare(f->name, Type::makeFunc(f->name, retType, std::move(paramTypes)),
                    f->line, f->column);
            if (!f->typeParams.empty()) {
                funcTypeParams_[f->name] = f->typeParams;
            }
            currentTypeParams_ = savedTP;
        } else if (auto* c = dynamic_cast<const ast::ClassDef*>(stmt.get())) {
            declare(c->name, Type::makeClass(c->name), c->line, c->column);
            ClassInfo info;
            info.name = c->name;
            info.baseClass = c->baseClass.value_or("");
            info.interfaces = c->interfaces;
            info.typeParams = c->typeParams;
            // Scan methods
            auto savedTP = currentTypeParams_;
            currentTypeParams_ = c->typeParams;
            for (const auto& s : c->body) {
                if (auto* m = dynamic_cast<const ast::FuncDef*>(s.get())) {
                    MethodInfo method;
                    method.name = m->name;
                    method.returnType = m->returnType.empty() ? Type::makeUnknown()
                                                               : resolveTypeName(m->returnType);
                    for (const auto& p : m->params) {
                        method.paramTypes.push_back(
                            p.typeAnnotation.empty() ? Type::makeUnknown()
                                                     : resolveTypeName(p.typeAnnotation));
                    }
                    info.methods.push_back(method);
                }
            }
            currentTypeParams_ = savedTP;
            classTable_[c->name] = info;
        } else if (auto* i = dynamic_cast<const ast::InterfaceDef*>(stmt.get())) {
            declare(i->name, Type::makeInterface(i->name), i->line, i->column);
            InterfaceInfo info;
            info.name = i->name;
            for (const auto& m : i->methods) {
                MethodInfo method;
                method.name = m.name;
                method.returnType = m.returnType.empty() ? Type::makeUnknown()
                                                          : resolveTypeName(m.returnType);
                for (const auto& p : m.params) {
                    method.paramTypes.push_back(
                        p.typeAnnotation.empty() ? Type::makeUnknown()
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
Type TypeChecker::checkExpr(const ast::Expr& expr) {
    exprResult_ = Type::makeUnknown();
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
    Type rhsType = checkExpr(*s.value);

    if (auto* id = dynamic_cast<const ast::Identifier*>(s.target.get())) {
        if (symbols_.isDeclared(id->name)) {
            Type existing = symbols_.lookup(id->name);
            if (!existing.isUnknown() && !rhsType.isUnknown() && existing != rhsType) {
                if (existing.isNumeric() && rhsType.isNumeric()) {
                    if (existing.kind == Type::Int && rhsType.kind == Type::Float) {
                        error("Cannot assign " + rhsType.toUserString() +
                              " to " + existing.toUserString(), s.line, s.column);
                    }
                } else {
                    std::string hint;
                    if (existing.kind == Type::Int) hint = "did you mean to call int(x)?";
                    else if (existing.kind == Type::Str) hint = "did you mean to call str(x)?";
                    else if (existing.kind == Type::Float) hint = "did you mean to call float(x)?";
                    error("Cannot assign " + rhsType.toUserString() +
                          " to " + existing.toUserString(), s.line, s.column, hint);
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
    Type retType = Type::makeVoid();
    if (s.value) {
        retType = checkExpr(*s.value);
    }
    if (hasExplicitReturnType_ && !currentReturnType_.isUnknown()) {
        if (retType != currentReturnType_ && !retType.isUnknown()) {
            if (retType.isNumeric() && currentReturnType_.isNumeric()) {
                if (currentReturnType_.kind == Type::Int && retType.kind == Type::Float) {
                    error("Return type mismatch: expected " + currentReturnType_.toUserString() +
                          ", got " + retType.toUserString(), s.line, s.column);
                }
            } else {
                std::string hint;
                if (currentReturnType_.kind == Type::Int)
                    hint = "did you mean to call int(x)?";
                else if (currentReturnType_.kind == Type::Str)
                    hint = "did you mean to call str(x)?";
                else if (currentReturnType_.kind == Type::Float)
                    hint = "did you mean to call float(x)?";
                error("Return type mismatch: expected " + currentReturnType_.toUserString() +
                      ", got " + retType.toUserString(), s.line, s.column, hint);
            }
        }
    }
}

void TypeChecker::visit(const ast::FuncDef& s) {
    for (const auto& dec : s.decorators) {
        checkExpr(*dec);
    }

    Type savedRetType = currentReturnType_;
    bool savedHasExplicit = hasExplicitReturnType_;
    auto savedTP = currentTypeParams_;
    currentTypeParams_.insert(currentTypeParams_.end(),
                               s.typeParams.begin(), s.typeParams.end());

    if (!s.returnType.empty()) {
        currentReturnType_ = resolveTypeName(s.returnType);
        hasExplicitReturnType_ = true;
    } else {
        currentReturnType_ = Type::makeUnknown();
        hasExplicitReturnType_ = false;
    }

    enterScope();
    for (const auto& tp : s.typeParams) {
        declare(tp, Type::makeTypeVar(tp), s.line, s.column);
    }
    for (const auto& p : s.params) {
        Type pType = p.typeAnnotation.empty() ? Type::makeUnknown()
                                               : resolveTypeName(p.typeAnnotation);
        if (p.defaultValue) {
            checkExpr(*p.defaultValue);
        }
        declare(p.name, pType, s.line, s.column);
    }
    checkStmtList(s.body);

    // Check for missing return in non-void functions.
    if (hasExplicitReturnType_ &&
        currentReturnType_.kind != Type::Void &&
        !currentReturnType_.isUnknown() &&
        !stmtListHasReturn(s.body)) {
        error("function '" + s.name + "' missing return statement; "
              "return type is '" + currentReturnType_.toUserString() + "'",
              s.line, s.column,
              "add 'return <value>' at the end of the function");
    }

    exitScope();

    currentReturnType_ = savedRetType;
    hasExplicitReturnType_ = savedHasExplicit;
    currentTypeParams_ = savedTP;
}

void TypeChecker::visit(const ast::InitDef& s) {
    Type savedRetType = currentReturnType_;
    bool savedHasExplicit = hasExplicitReturnType_;
    currentReturnType_ = Type::makeVoid();
    hasExplicitReturnType_ = false;

    enterScope();
    for (const auto& p : s.params) {
        Type pType = p.typeAnnotation.empty() ? Type::makeUnknown()
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
    declare("this", Type::makeClass(s.name), s.line, s.column);

    for (const auto& tp : s.typeParams) {
        declare(tp, Type::makeTypeVar(tp), s.line, s.column);
    }

    if (s.baseClass) {
        declare("super", Type::makeClass(*s.baseClass), s.line, s.column);
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
                            if (!childMethod.paramTypes[i].isUnknown() &&
                                !baseMethod.paramTypes[i].isUnknown() &&
                                childMethod.paramTypes[i] != baseMethod.paramTypes[i]) {
                                error("Override '" + childMethod.name +
                                      "' has incompatible parameter type: expected " +
                                      baseMethod.paramTypes[i].toString() +
                                      ", got " + childMethod.paramTypes[i].toString(),
                                      s.line, s.column);
                            }
                        }
                        if (!childMethod.returnType.isUnknown() &&
                            !baseMethod.returnType.isUnknown() &&
                            childMethod.returnType != baseMethod.returnType) {
                            error("Override '" + childMethod.name +
                                  "' has incompatible return type: expected " +
                                  baseMethod.returnType.toString() +
                                  ", got " + childMethod.returnType.toString(),
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
    Type condType = checkExpr(*s.condition);
    (void)condType;

    // Nullable narrowing: analyze "x != null" conditions
    std::string narrowVar;
    Type narrowType;
    bool hasNarrowing = false;
    if (auto* bin = dynamic_cast<const ast::BinaryExpr*>(s.condition.get())) {
        if (bin->op == ast::BinOp::Neq) {
            auto* ident = dynamic_cast<const ast::Identifier*>(bin->left.get());
            auto* null_ = dynamic_cast<const ast::NullLiteral*>(bin->right.get());
            if (ident && null_) {
                Type varType = symbols_.lookup(ident->name);
                if (varType.kind == Type::Union) {
                    std::vector<Type> remaining;
                    for (const auto& m : varType.params) {
                        if (m.kind != Type::Null) remaining.push_back(m);
                    }
                    if (remaining.size() == 1) {
                        narrowType = remaining[0];
                    } else if (!remaining.empty()) {
                        narrowType = Type(Type::Union, std::move(remaining));
                    }
                    narrowVar = ident->name;
                    hasNarrowing = true;
                }
            }
        }
    }

    enterScope();
    if (hasNarrowing) {
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
        checkStmtList(s.elseBranch);
        exitScope();
    }
}

void TypeChecker::visit(const ast::ForStmt& s) {
    Type iterType = checkExpr(*s.iterable);
    enterScope();
    Type elemType = Type::makeUnknown();
    if (iterType.kind == Type::List && !iterType.params.empty()) {
        elemType = iterType.params[0];
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

void TypeChecker::visit(const ast::TryStmt& s) {
    enterScope();
    checkStmtList(s.tryBody);
    exitScope();

    for (const auto& c : s.catchClauses) {
        enterScope();
        if (!c.varName.empty()) {
            Type excType = c.exceptionType.empty()
                               ? Type::makeUnknown()
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
    checkExpr(*s.expr);
}

void TypeChecker::visit(const ast::ImportStmt& s) {
    std::string modName = s.module;
    auto dotPos = modName.find('.');
    if (dotPos != std::string::npos) modName = modName.substr(0, dotPos);
    declare(modName, Type::makeUnknown(), s.line, s.column);
}

void TypeChecker::visit(const ast::FromImportStmt& s) {
    for (const auto& name : s.names) {
        declare(name, Type::makeUnknown(), s.line, s.column);
    }
}

void TypeChecker::visit(const ast::BreakStmt&)    { /* nothing to check */ }
void TypeChecker::visit(const ast::ContinueStmt&) { /* nothing to check */ }
void TypeChecker::visit(const ast::PassStmt&)     { /* nothing to check */ }
void TypeChecker::visit(const ast::InterfaceDef&) { /* registered in first pass */ }

void TypeChecker::visit(const ast::TupleUnpackStmt& s) {
    Type rhsType = checkExpr(*s.value);
    for (const auto& target : s.targets) {
        Type elemType = Type::makeUnknown();
        (void)rhsType;
        declare(target, elemType, s.line, s.column);
    }
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Expression visit() overrides
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

void TypeChecker::visit(const ast::IntLiteral&)    { exprResult_ = Type::makeInt(); }
void TypeChecker::visit(const ast::FloatLiteral&)  { exprResult_ = Type::makeFloat(); }
void TypeChecker::visit(const ast::StringLiteral&) { exprResult_ = Type::makeStr(); }
void TypeChecker::visit(const ast::FStringLiteral&){ exprResult_ = Type::makeStr(); }
void TypeChecker::visit(const ast::FStringExpr&)   { exprResult_ = Type::makeUnknown(); }
void TypeChecker::visit(const ast::BoolLiteral&)   { exprResult_ = Type::makeBool(); }
void TypeChecker::visit(const ast::NullLiteral&)   { exprResult_ = Type::makeNull(); }

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
        exprResult_ = Type::makeUnknown();
        return;
    }
    auto clsIt = classTable_.find(currentClassName_);
    if (currentClassName_.empty() || clsIt == classTable_.end() ||
        clsIt->second.baseClass.empty()) {
        error("'super' used in class without base class", e.line, e.column);
        exprResult_ = Type::makeUnknown();
        return;
    }
    exprResult_ = Type::makeClass(clsIt->second.baseClass);
}

void TypeChecker::visit(const ast::BinaryExpr& e) {
    Type lt = checkExpr(*e.left);
    Type rt = checkExpr(*e.right);

    switch (e.op) {
        case ast::BinOp::Add:
            if (lt.kind == Type::Str && rt.kind == Type::Str) { exprResult_ = Type::makeStr(); return; }
            if (lt.isNumeric() && rt.isNumeric()) { exprResult_ = promoteNumeric(lt, rt); return; }
            if (!lt.isUnknown() && !rt.isUnknown()) {
                error("Cannot apply '+' to " + lt.toString() + " and " + rt.toString(),
                      e.line, e.column);
            }
            exprResult_ = Type::makeUnknown(); return;

        case ast::BinOp::Sub:
        case ast::BinOp::Mul:
        case ast::BinOp::Div:
        case ast::BinOp::Mod:
        case ast::BinOp::Pow:
            if (lt.isNumeric() && rt.isNumeric()) { exprResult_ = promoteNumeric(lt, rt); return; }
            if (!lt.isUnknown() && !rt.isUnknown()) {
                error("Cannot apply arithmetic to " +
                      lt.toString() + " and " + rt.toString(), e.line, e.column);
            }
            exprResult_ = Type::makeUnknown(); return;

        case ast::BinOp::IntDiv:
            if (lt.isNumeric() && rt.isNumeric()) { exprResult_ = Type::makeInt(); return; }
            if (!lt.isUnknown() && !rt.isUnknown()) {
                error("Cannot apply '//' to " + lt.toString() + " and " + rt.toString(),
                      e.line, e.column);
            }
            exprResult_ = Type::makeUnknown(); return;

        case ast::BinOp::Eq:
        case ast::BinOp::Neq:
            exprResult_ = Type::makeBool(); return;

        case ast::BinOp::Lt:
        case ast::BinOp::Gt:
        case ast::BinOp::Lte:
        case ast::BinOp::Gte:
            if (lt.isNumeric() && rt.isNumeric()) { exprResult_ = Type::makeBool(); return; }
            if (lt.kind == Type::Str && rt.kind == Type::Str) { exprResult_ = Type::makeBool(); return; }
            if (!lt.isUnknown() && !rt.isUnknown()) {
                error("Cannot compare " + lt.toString() + " and " + rt.toString(),
                      e.line, e.column);
            }
            exprResult_ = Type::makeBool(); return;

        case ast::BinOp::And:
        case ast::BinOp::Or:
            exprResult_ = Type::makeBool(); return;

        case ast::BinOp::In:
        case ast::BinOp::NotIn:
            exprResult_ = Type::makeBool(); return;

        case ast::BinOp::BitAnd:
        case ast::BinOp::BitOr:
        case ast::BinOp::BitXor:
        case ast::BinOp::Shl:
        case ast::BinOp::Shr:
            if ((lt.kind == Type::Int || lt.kind == Type::Bool) &&
                (rt.kind == Type::Int || rt.kind == Type::Bool)) {
                exprResult_ = Type::makeInt(); return;
            }
            if (lt.kind == Type::Float || rt.kind == Type::Float) {
                error("Cannot apply bitwise operator to float", e.line, e.column);
            } else if (!lt.isUnknown() && !rt.isUnknown()) {
                error("Cannot apply bitwise operator to " +
                      lt.toString() + " and " + rt.toString(), e.line, e.column);
            }
            exprResult_ = Type::makeUnknown(); return;
    }
    exprResult_ = Type::makeUnknown();
}

void TypeChecker::visit(const ast::UnaryExpr& e) {
    Type operandType = checkExpr(*e.operand);
    if (e.op == ast::UnaryOp::Neg) {
        if (operandType.isNumeric()) { exprResult_ = operandType; return; }
        if (!operandType.isUnknown()) {
            error("Cannot negate " + operandType.toString(), e.line, e.column);
        }
        exprResult_ = Type::makeUnknown(); return;
    }
    if (e.op == ast::UnaryOp::BitNot) {
        if (operandType.kind == Type::Int) { exprResult_ = Type::makeInt(); return; }
        if (operandType.kind == Type::Float) {
            error("Cannot apply ~ to float", e.line, e.column);
        } else if (!operandType.isUnknown()) {
            error("Cannot apply ~ to " + operandType.toString(), e.line, e.column);
        }
        exprResult_ = Type::makeUnknown(); return;
    }
    // Not
    exprResult_ = Type::makeBool();
}

void TypeChecker::visit(const ast::CallExpr& e) {
    Type calleeType = checkExpr(*e.callee);

    std::vector<Type> argTypes;
    for (const auto& a : e.args) {
        argTypes.push_back(checkExpr(*a));
    }

    if (calleeType.kind == Type::Func) {
        std::string funcName = calleeType.name;
        auto tpIt = funcTypeParams_.find(funcName);

        if (tpIt != funcTypeParams_.end() && !tpIt->second.empty()) {
            const auto& typeParamNames = tpIt->second;
            std::unordered_map<std::string, Type> typeArgMap;

            if (!e.typeArgs.empty()) {
                for (size_t i = 0; i < typeParamNames.size() && i < e.typeArgs.size(); i++) {
                    typeArgMap[typeParamNames[i]] = resolveTypeName(e.typeArgs[i]);
                }
            } else {
                for (size_t i = 0; i < argTypes.size() && i + 1 < calleeType.params.size(); i++) {
                    const Type& paramType = calleeType.params[i + 1];
                    if (paramType.kind == Type::TypeVar) {
                        typeArgMap[paramType.name] = argTypes[i];
                    }
                }
            }

            Type retType = calleeType.params.empty() ? Type::makeVoid() : calleeType.params[0];
            if (retType.kind == Type::TypeVar) {
                auto it = typeArgMap.find(retType.name);
                if (it != typeArgMap.end()) retType = it->second;
            }

            size_t expectedArgs = calleeType.params.size() - 1;
            if (argTypes.size() != expectedArgs) {
                error("Function '" + funcName + "' expects " +
                      std::to_string(expectedArgs) + " argument(s), got " +
                      std::to_string(argTypes.size()), e.line, e.column);
            }

            exprResult_ = retType; return;
        }

        // Non-generic function
        size_t expectedArgs = calleeType.params.size() - 1;
        if (argTypes.size() != expectedArgs) {
            error("Function '" + calleeType.name + "' expects " +
                  std::to_string(expectedArgs) + " argument(s), got " +
                  std::to_string(argTypes.size()), e.line, e.column);
        }
        for (size_t i = 0; i < argTypes.size() && i + 1 < calleeType.params.size(); i++) {
            const Type& expected = calleeType.params[i + 1];
            const Type& actual = argTypes[i];
            if (!expected.isUnknown() && !actual.isUnknown() && expected != actual) {
                if (!isAssignableTo(actual, expected)) {
                    error("Function '" + calleeType.name + "' expects " +
                          expected.toString() + ", got " + actual.toString(),
                          e.line, e.column);
                }
            }
        }
        exprResult_ = calleeType.params.empty() ? Type::makeVoid() : calleeType.params[0];
        return;
    }

    if (calleeType.kind == Type::Class) {
        exprResult_ = calleeType; return;
    }

    if (!calleeType.isUnknown()) {
        if (auto* id = dynamic_cast<const ast::Identifier*>(e.callee.get())) {
            error("Cannot call non-function '" + id->name + "'", e.line, e.column);
        } else {
            error("Cannot call non-function type " + calleeType.toString(), e.line, e.column);
        }
    }

    exprResult_ = Type::makeUnknown();
}

void TypeChecker::visit(const ast::MemberExpr& e) {
    checkExpr(*e.object);
    exprResult_ = Type::makeUnknown();
}

void TypeChecker::visit(const ast::IndexExpr& e) {
    Type objType = checkExpr(*e.object);
    Type idxType = checkExpr(*e.index);
    (void)idxType;

    if (objType.kind == Type::List && !objType.params.empty()) {
        exprResult_ = objType.params[0]; return;
    }
    if (objType.kind == Type::Dict && objType.params.size() >= 2) {
        exprResult_ = objType.params[1]; return;
    }
    if (objType.kind == Type::Str) {
        exprResult_ = Type::makeStr(); return;
    }
    exprResult_ = Type::makeUnknown();
}

void TypeChecker::visit(const ast::LambdaExpr& e) {
    enterScope();
    std::vector<Type> paramTypes;
    for (const auto& p : e.params) {
        declare(p, Type::makeUnknown(), e.line, e.column);
        paramTypes.push_back(Type::makeUnknown());
    }
    Type bodyType = checkExpr(*e.body);
    exitScope();
    exprResult_ = Type::makeFunc("<lambda>", bodyType, std::move(paramTypes));
}

void TypeChecker::visit(const ast::ListExpr& e) {
    if (e.elements.empty()) {
        exprResult_ = Type::makeList(Type::makeUnknown()); return;
    }
    Type elemType = checkExpr(*e.elements[0]);
    for (size_t i = 1; i < e.elements.size(); i++) {
        Type t = checkExpr(*e.elements[i]);
        elemType = unify(elemType, t, e.line, e.column);
    }
    exprResult_ = Type::makeList(elemType);
}

void TypeChecker::visit(const ast::DictExpr& e) {
    if (e.entries.empty()) {
        exprResult_ = Type::makeDict(Type::makeUnknown(), Type::makeUnknown()); return;
    }
    Type keyType = checkExpr(*e.entries[0].first);
    Type valType = checkExpr(*e.entries[0].second);
    for (size_t i = 1; i < e.entries.size(); i++) {
        Type kt = checkExpr(*e.entries[i].first);
        Type vt = checkExpr(*e.entries[i].second);
        keyType = unify(keyType, kt, e.line, e.column);
        valType = unify(valType, vt, e.line, e.column);
    }
    exprResult_ = Type::makeDict(keyType, valType);
}

void TypeChecker::visit(const ast::TupleExpr& e) {
    std::vector<Type> elemTypes;
    for (const auto& el : e.elements) {
        elemTypes.push_back(checkExpr(*el));
    }
    exprResult_ = Type::makeTuple(std::move(elemTypes));
}

void TypeChecker::visit(const ast::TernaryExpr& e) {
    checkExpr(*e.condition);
    Type thenType = checkExpr(*e.thenExpr);
    Type elseType = checkExpr(*e.elseExpr);
    exprResult_ = unify(thenType, elseType, e.line, e.column);
}

void TypeChecker::visit(const ast::SliceExpr& e) {
    Type objType = checkExpr(*e.object);
    if (e.start) checkExpr(*e.start);
    if (e.stop)  checkExpr(*e.stop);
    if (e.step)  checkExpr(*e.step);
    if (objType.kind == Type::List) { exprResult_ = objType; return; }
    if (objType.kind == Type::Str)  { exprResult_ = Type::makeStr(); return; }
    exprResult_ = Type::makeUnknown();
}

void TypeChecker::visit(const ast::ListComprehension& e) {
    Type iterType = checkExpr(*e.iterable);
    enterScope();
    Type elemType = Type::makeUnknown();
    if (iterType.kind == Type::List && !iterType.params.empty()) {
        elemType = iterType.params[0];
    }
    declare(e.variable, elemType, e.line, e.column);
    if (e.condition) checkExpr(*e.condition);
    Type bodyType = checkExpr(*e.body);
    exitScope();
    exprResult_ = Type::makeList(bodyType);
}

void TypeChecker::visit(const ast::DictComprehension& e) {
    Type iterType = checkExpr(*e.iterable);
    enterScope();
    Type elemType = Type::makeUnknown();
    if (iterType.kind == Type::List && !iterType.params.empty()) {
        elemType = iterType.params[0];
    }
    declare(e.variable, elemType, e.line, e.column);
    if (e.condition) checkExpr(*e.condition);
    Type keyType = checkExpr(*e.key);
    Type valType = checkExpr(*e.value);
    exitScope();
    exprResult_ = Type::makeDict(keyType, valType);
}

void TypeChecker::visit(const ast::SpreadExpr& e) {
    exprResult_ = checkExpr(*e.operand);
}

} // namespace visuall
