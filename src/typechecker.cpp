#include "typechecker.h"
#include <sstream>
#include <cassert>

namespace visuall {

// ════════════════════════════════════════════════════════════════════════════
// Type
// ════════════════════════════════════════════════════════════════════════════

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

Type Type::makeFunc(const std::string& n, Type ret, std::vector<Type> paramTypes) {
    // params[0] = return type, params[1..] = parameter types
    std::vector<Type> all;
    all.push_back(std::move(ret));
    for (auto& p : paramTypes) all.push_back(std::move(p));
    return Type(Func, n, std::move(all));
}

// ════════════════════════════════════════════════════════════════════════════
// SymbolTable
// ════════════════════════════════════════════════════════════════════════════

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

// ════════════════════════════════════════════════════════════════════════════
// TypeChecker — construction
// ════════════════════════════════════════════════════════════════════════════

TypeChecker::TypeChecker(const std::string& filename)
    : filename_(filename), currentReturnType_(Type::makeVoid()) {}

// ════════════════════════════════════════════════════════════════════════════
// Scope helpers
// ════════════════════════════════════════════════════════════════════════════

void TypeChecker::enterScope() { symbols_.enterScope(); }
void TypeChecker::exitScope()  { symbols_.exitScope(); }

void TypeChecker::declare(const std::string& name, const Type& type, int line, int col) {
    (void)line; (void)col;
    symbols_.declare(name, type);
}

Type TypeChecker::lookup(const std::string& name, int line, int col) {
    if (!symbols_.isDeclared(name)) {
        error("Undefined variable '" + name + "'", line, col);
    }
    return symbols_.lookup(name);
}

// ════════════════════════════════════════════════════════════════════════════
// Type resolution from annotation strings
// ════════════════════════════════════════════════════════════════════════════

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
                // Split on commas (simple split — no nested commas expected)
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

// ════════════════════════════════════════════════════════════════════════════
// Unification — resolve two types to a common type, or error.
// ════════════════════════════════════════════════════════════════════════════

Type TypeChecker::unify(const Type& a, const Type& b, int line, int col) {
    if (a.isUnknown()) return b;
    if (b.isUnknown()) return a;
    if (a == b) return a;

    // Numeric promotion: int + float → float
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

    error("Cannot assign " + a.toString() + " to " + b.toString(), line, col);
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

    // Numeric promotion (int → float only)
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

// ════════════════════════════════════════════════════════════════════════════
// Error reporting
// ════════════════════════════════════════════════════════════════════════════

[[noreturn]] void TypeChecker::error(const std::string& msg, int line, int col) const {
    throw TypeError(msg, filename_, line, col);
}

// ════════════════════════════════════════════════════════════════════════════
// Entry point
// ════════════════════════════════════════════════════════════════════════════

void TypeChecker::check(const ast::Program& program) {
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
}

// ════════════════════════════════════════════════════════════════════════════
// Statement visitors
// ════════════════════════════════════════════════════════════════════════════

void TypeChecker::checkStmtList(const ast::StmtList& stmts) {
    for (const auto& s : stmts) {
        checkStmt(*s);
    }
}

void TypeChecker::checkStmt(const ast::Stmt& stmt) {
    // ── ExprStmt ───────────────────────────────────────────────────────
    if (auto* s = dynamic_cast<const ast::ExprStmt*>(&stmt)) {
        checkExpr(*s->expr);
        return;
    }

    // ── AssignStmt ─────────────────────────────────────────────────────
    if (auto* s = dynamic_cast<const ast::AssignStmt*>(&stmt)) {
        Type rhsType = checkExpr(*s->value);

        // If target is an identifier, declare or check.
        if (auto* id = dynamic_cast<const ast::Identifier*>(s->target.get())) {
            if (symbols_.isDeclared(id->name)) {
                Type existing = symbols_.lookup(id->name);
                if (!existing.isUnknown() && !rhsType.isUnknown() && existing != rhsType) {
                    // Allow numeric promotion on assignment
                    if (existing.isNumeric() && rhsType.isNumeric()) {
                        // Promote: assigning float to int is narrowing → error
                        if (existing.kind == Type::Int && rhsType.kind == Type::Float) {
                            error("Cannot assign " + rhsType.toString() +
                                  " to " + existing.toString(), stmt.line, stmt.column);
                        }
                        // int to float is fine
                    } else {
                        error("Cannot assign " + rhsType.toString() +
                              " to " + existing.toString(), stmt.line, stmt.column);
                    }
                }
            } else {
                declare(id->name, rhsType, stmt.line, stmt.column);
            }
        } else {
            // Member/index assignment — just type-check both sides.
            checkExpr(*s->target);
        }
        return;
    }

    // ── ReturnStmt ─────────────────────────────────────────────────────
    if (auto* s = dynamic_cast<const ast::ReturnStmt*>(&stmt)) {
        Type retType = Type::makeVoid();
        if (s->value) {
            retType = checkExpr(*s->value);
        }
        if (hasExplicitReturnType_ && !currentReturnType_.isUnknown()) {
            if (retType != currentReturnType_ && !retType.isUnknown()) {
                // Allow numeric promotion
                if (retType.isNumeric() && currentReturnType_.isNumeric()) {
                    if (currentReturnType_.kind == Type::Int && retType.kind == Type::Float) {
                        error("Return type mismatch: expected " + currentReturnType_.toString() +
                              ", got " + retType.toString(), stmt.line, stmt.column);
                    }
                } else {
                    error("Return type mismatch: expected " + currentReturnType_.toString() +
                          ", got " + retType.toString(), stmt.line, stmt.column);
                }
            }
        }
        return;
    }

    // ── FuncDef ────────────────────────────────────────────────────────
    if (auto* s = dynamic_cast<const ast::FuncDef*>(&stmt)) {
        // Check decorator expressions
        for (const auto& dec : s->decorators) {
            checkExpr(*dec);
        }

        Type savedRetType = currentReturnType_;
        bool savedHasExplicit = hasExplicitReturnType_;
        auto savedTP = currentTypeParams_;
        currentTypeParams_.insert(currentTypeParams_.end(),
                                   s->typeParams.begin(), s->typeParams.end());

        if (!s->returnType.empty()) {
            currentReturnType_ = resolveTypeName(s->returnType);
            hasExplicitReturnType_ = true;
        } else {
            currentReturnType_ = Type::makeUnknown();
            hasExplicitReturnType_ = false;
        }

        enterScope();
        // Declare type params
        for (const auto& tp : s->typeParams) {
            declare(tp, Type::makeTypeVar(tp), stmt.line, stmt.column);
        }
        for (const auto& p : s->params) {
            Type pType = p.typeAnnotation.empty() ? Type::makeUnknown()
                                                   : resolveTypeName(p.typeAnnotation);
            if (p.defaultValue) {
                checkExpr(*p.defaultValue);
            }
            declare(p.name, pType, stmt.line, stmt.column);
        }
        checkStmtList(s->body);
        exitScope();

        currentReturnType_ = savedRetType;
        hasExplicitReturnType_ = savedHasExplicit;
        currentTypeParams_ = savedTP;
        return;
    }

    // ── InitDef ────────────────────────────────────────────────────────
    if (auto* s = dynamic_cast<const ast::InitDef*>(&stmt)) {
        Type savedRetType = currentReturnType_;
        bool savedHasExplicit = hasExplicitReturnType_;
        currentReturnType_ = Type::makeVoid();
        hasExplicitReturnType_ = false;

        enterScope();
        for (const auto& p : s->params) {
            Type pType = p.typeAnnotation.empty() ? Type::makeUnknown()
                                                   : resolveTypeName(p.typeAnnotation);
            if (p.defaultValue) {
                checkExpr(*p.defaultValue);
            }
            declare(p.name, pType, stmt.line, stmt.column);
        }
        checkStmtList(s->body);
        exitScope();

        currentReturnType_ = savedRetType;
        hasExplicitReturnType_ = savedHasExplicit;
        return;
    }

    // ── ClassDef ───────────────────────────────────────────────────────
    if (auto* s = dynamic_cast<const ast::ClassDef*>(&stmt)) {
        bool savedInsideClass = insideClass_;
        std::string savedClassName = currentClassName_;
        auto savedTP = currentTypeParams_;

        insideClass_ = true;
        currentClassName_ = s->name;
        currentTypeParams_ = s->typeParams;

        enterScope();
        // Declare 'this' as the class type
        declare("this", Type::makeClass(s->name), stmt.line, stmt.column);

        // Declare type params
        for (const auto& tp : s->typeParams) {
            declare(tp, Type::makeTypeVar(tp), stmt.line, stmt.column);
        }

        // If extends, declare super
        if (s->baseClass) {
            declare("super", Type::makeClass(*s->baseClass), stmt.line, stmt.column);
        }

        checkStmtList(s->body);

        // Check override signatures
        if (s->baseClass) {
            auto baseIt = classTable_.find(*s->baseClass);
            auto childIt = classTable_.find(s->name);
            if (baseIt != classTable_.end() && childIt != classTable_.end()) {
                for (const auto& childMethod : childIt->second.methods) {
                    for (const auto& baseMethod : baseIt->second.methods) {
                        if (childMethod.name == baseMethod.name) {
                            // Check param count
                            if (childMethod.paramTypes.size() != baseMethod.paramTypes.size()) {
                                error("Override '" + childMethod.name +
                                      "' has different parameter count",
                                      stmt.line, stmt.column);
                            }
                            // Check param types
                            for (size_t i = 0; i < childMethod.paramTypes.size() &&
                                 i < baseMethod.paramTypes.size(); i++) {
                                if (!childMethod.paramTypes[i].isUnknown() &&
                                    !baseMethod.paramTypes[i].isUnknown() &&
                                    childMethod.paramTypes[i] != baseMethod.paramTypes[i]) {
                                    error("Override '" + childMethod.name +
                                          "' has incompatible parameter type: expected " +
                                          baseMethod.paramTypes[i].toString() +
                                          ", got " + childMethod.paramTypes[i].toString(),
                                          stmt.line, stmt.column);
                                }
                            }
                            // Check return type
                            if (!childMethod.returnType.isUnknown() &&
                                !baseMethod.returnType.isUnknown() &&
                                childMethod.returnType != baseMethod.returnType) {
                                error("Override '" + childMethod.name +
                                      "' has incompatible return type: expected " +
                                      baseMethod.returnType.toString() +
                                      ", got " + childMethod.returnType.toString(),
                                      stmt.line, stmt.column);
                            }
                        }
                    }
                }
            }
        }

        // Check interface implementation
        for (const auto& ifaceName : s->interfaces) {
            auto ifaceIt = interfaceTable_.find(ifaceName);
            if (ifaceIt != interfaceTable_.end()) {
                auto classIt = classTable_.find(s->name);
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
                            error("Class '" + s->name +
                                  "' does not implement method '" +
                                  ifaceMethod.name +
                                  "' required by interface '" + ifaceName + "'",
                                  stmt.line, stmt.column);
                        }
                    }
                }
            }
        }

        exitScope();
        insideClass_ = savedInsideClass;
        currentClassName_ = savedClassName;
        currentTypeParams_ = savedTP;
        return;
    }

    // ── IfStmt ─────────────────────────────────────────────────────────
    if (auto* s = dynamic_cast<const ast::IfStmt*>(&stmt)) {
        Type condType = checkExpr(*s->condition);
        if (!condType.isUnknown() && condType.kind != Type::Bool &&
            !condType.isNumeric()) {
            // Allow numeric and bool conditions; error on others like str.
            // Actually, be lenient: any type is truthy in Visuall.
        }

        // Analyze condition for nullable narrowing: x != null
        std::string narrowVar;
        Type narrowType;
        bool hasNarrowing = false;
        if (auto* bin = dynamic_cast<const ast::BinaryExpr*>(s->condition.get())) {
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
            declare(narrowVar, narrowType, s->condition->line, s->condition->column);
        }
        checkStmtList(s->thenBranch);
        exitScope();

        for (const auto& elsif : s->elsifBranches) {
            checkExpr(*elsif.first);
            enterScope();
            checkStmtList(elsif.second);
            exitScope();
        }

        if (!s->elseBranch.empty()) {
            enterScope();
            checkStmtList(s->elseBranch);
            exitScope();
        }
        return;
    }

    // ── ForStmt ────────────────────────────────────────────────────────
    if (auto* s = dynamic_cast<const ast::ForStmt*>(&stmt)) {
        Type iterType = checkExpr(*s->iterable);
        enterScope();
        // Infer loop variable type from iterable.
        Type elemType = Type::makeUnknown();
        if (iterType.kind == Type::List && !iterType.params.empty()) {
            elemType = iterType.params[0];
        }
        declare(s->variable, elemType, stmt.line, stmt.column);
        checkStmtList(s->body);
        exitScope();
        return;
    }

    // ── WhileStmt ──────────────────────────────────────────────────────
    if (auto* s = dynamic_cast<const ast::WhileStmt*>(&stmt)) {
        checkExpr(*s->condition);
        enterScope();
        checkStmtList(s->body);
        exitScope();
        return;
    }

    // ── TryStmt ────────────────────────────────────────────────────────
    if (auto* s = dynamic_cast<const ast::TryStmt*>(&stmt)) {
        enterScope();
        checkStmtList(s->tryBody);
        exitScope();

        for (const auto& c : s->catchClauses) {
            enterScope();
            if (!c.varName.empty()) {
                Type excType = c.exceptionType.empty()
                                   ? Type::makeUnknown()
                                   : resolveTypeName(c.exceptionType);
                declare(c.varName, excType, stmt.line, stmt.column);
            }
            checkStmtList(c.body);
            exitScope();
        }

        if (!s->finallyBody.empty()) {
            enterScope();
            checkStmtList(s->finallyBody);
            exitScope();
        }
        return;
    }

    // ── ThrowStmt ──────────────────────────────────────────────────────
    if (auto* s = dynamic_cast<const ast::ThrowStmt*>(&stmt)) {
        checkExpr(*s->expr);
        return;
    }

    // ── ImportStmt / FromImportStmt ────────────────────────────────────
    if (dynamic_cast<const ast::ImportStmt*>(&stmt) ||
        dynamic_cast<const ast::FromImportStmt*>(&stmt)) {
        // Imports introduce names — for now, declare as unknown.
        if (auto* s = dynamic_cast<const ast::ImportStmt*>(&stmt)) {
            // Declare the module name.
            std::string modName = s->module;
            auto dotPos = modName.find('.');
            if (dotPos != std::string::npos) modName = modName.substr(0, dotPos);
            declare(modName, Type::makeUnknown(), stmt.line, stmt.column);
        } else if (auto* s = dynamic_cast<const ast::FromImportStmt*>(&stmt)) {
            for (const auto& name : s->names) {
                declare(name, Type::makeUnknown(), stmt.line, stmt.column);
            }
        }
        return;
    }

    // ── BreakStmt / ContinueStmt / PassStmt — nothing to check ────────
    if (dynamic_cast<const ast::BreakStmt*>(&stmt) ||
        dynamic_cast<const ast::ContinueStmt*>(&stmt) ||
        dynamic_cast<const ast::PassStmt*>(&stmt)) {
        return;
    }

    // ── TupleUnpackStmt ────────────────────────────────────────────────
    if (auto* s = dynamic_cast<const ast::TupleUnpackStmt*>(&stmt)) {
        Type rhsType = checkExpr(*s->value);
        for (const auto& target : s->targets) {
            Type elemType = Type::makeUnknown();
            if (rhsType.kind == Type::Tuple && !rhsType.params.empty()) {
                // Could assign individual tuple element types, but use unknown for now
            }
            declare(target, elemType, stmt.line, stmt.column);
        }
        return;
    }

    // ── InterfaceDef ───────────────────────────────────────────────────
    if (dynamic_cast<const ast::InterfaceDef*>(&stmt)) {
        // Already registered in first pass. Nothing to check.
        return;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Expression visitors — return the inferred type.
// ════════════════════════════════════════════════════════════════════════════

Type TypeChecker::checkExpr(const ast::Expr& expr) {
    // ── Literals ───────────────────────────────────────────────────────
    if (dynamic_cast<const ast::IntLiteral*>(&expr))    return Type::makeInt();
    if (dynamic_cast<const ast::FloatLiteral*>(&expr))  return Type::makeFloat();
    if (dynamic_cast<const ast::StringLiteral*>(&expr)) return Type::makeStr();
    if (dynamic_cast<const ast::FStringLiteral*>(&expr)) return Type::makeStr();
    if (dynamic_cast<const ast::BoolLiteral*>(&expr))   return Type::makeBool();
    if (dynamic_cast<const ast::NullLiteral*>(&expr))   return Type::makeNull();

    // ── Identifier ─────────────────────────────────────────────────────
    if (auto* e = dynamic_cast<const ast::Identifier*>(&expr)) {
        return lookup(e->name, e->line, e->column);
    }

    // ── ThisExpr ───────────────────────────────────────────────────────
    if (dynamic_cast<const ast::ThisExpr*>(&expr)) {
        if (!insideClass_) {
            error("'this' used outside of class", expr.line, expr.column);
        }
        return lookup("this", expr.line, expr.column);
    }

    // ── SuperExpr ──────────────────────────────────────────────────────
    if (dynamic_cast<const ast::SuperExpr*>(&expr)) {
        if (!insideClass_) {
            error("'super' used outside of class", expr.line, expr.column);
        }
        if (currentClassName_.empty() || classTable_.count(currentClassName_) == 0 ||
            classTable_.at(currentClassName_).baseClass.empty()) {
            error("'super' used in class without base class", expr.line, expr.column);
        }
        return Type::makeClass(classTable_.at(currentClassName_).baseClass);
    }

    // ── BinaryExpr ─────────────────────────────────────────────────────
    if (auto* e = dynamic_cast<const ast::BinaryExpr*>(&expr)) {
        Type lt = checkExpr(*e->left);
        Type rt = checkExpr(*e->right);

        switch (e->op) {
            // Arithmetic
            case ast::BinOp::Add:
                // str + str → str (concatenation)
                if (lt.kind == Type::Str && rt.kind == Type::Str) return Type::makeStr();
                if (lt.isNumeric() && rt.isNumeric()) return promoteNumeric(lt, rt);
                if (!lt.isUnknown() && !rt.isUnknown()) {
                    error("Cannot apply '+' to " + lt.toString() + " and " + rt.toString(),
                          expr.line, expr.column);
                }
                return Type::makeUnknown();

            case ast::BinOp::Sub:
            case ast::BinOp::Mul:
            case ast::BinOp::Div:
            case ast::BinOp::Mod:
            case ast::BinOp::Pow:
                if (lt.isNumeric() && rt.isNumeric()) return promoteNumeric(lt, rt);
                if (!lt.isUnknown() && !rt.isUnknown()) {
                    error("Cannot apply arithmetic to " +
                          lt.toString() + " and " + rt.toString(),
                          expr.line, expr.column);
                }
                return Type::makeUnknown();

            case ast::BinOp::IntDiv:
                if (lt.isNumeric() && rt.isNumeric()) return Type::makeInt();
                if (!lt.isUnknown() && !rt.isUnknown()) {
                    error("Cannot apply '//' to " +
                          lt.toString() + " and " + rt.toString(),
                          expr.line, expr.column);
                }
                return Type::makeUnknown();

            // Comparison — always bool
            case ast::BinOp::Eq:
            case ast::BinOp::Neq:
                return Type::makeBool();

            case ast::BinOp::Lt:
            case ast::BinOp::Gt:
            case ast::BinOp::Lte:
            case ast::BinOp::Gte:
                if (lt.isNumeric() && rt.isNumeric()) return Type::makeBool();
                if (lt.kind == Type::Str && rt.kind == Type::Str) return Type::makeBool();
                if (!lt.isUnknown() && !rt.isUnknown()) {
                    error("Cannot compare " + lt.toString() + " and " + rt.toString(),
                          expr.line, expr.column);
                }
                return Type::makeBool();

            // Logical — always bool
            case ast::BinOp::And:
            case ast::BinOp::Or:
                return Type::makeBool();

            // Membership
            case ast::BinOp::In:
            case ast::BinOp::NotIn:
                return Type::makeBool();

            // Bitwise — only valid on int and bool
            case ast::BinOp::BitAnd:
            case ast::BinOp::BitOr:
            case ast::BinOp::BitXor:
            case ast::BinOp::Shl:
            case ast::BinOp::Shr:
                if ((lt.kind == Type::Int || lt.kind == Type::Bool) &&
                    (rt.kind == Type::Int || rt.kind == Type::Bool)) {
                    return Type::makeInt();
                }
                if (lt.kind == Type::Float || rt.kind == Type::Float) {
                    error("Cannot apply bitwise operator to float",
                          expr.line, expr.column);
                }
                if (!lt.isUnknown() && !rt.isUnknown()) {
                    error("Cannot apply bitwise operator to " +
                          lt.toString() + " and " + rt.toString(),
                          expr.line, expr.column);
                }
                return Type::makeUnknown();
        }
        return Type::makeUnknown();
    }

    // ── UnaryExpr ──────────────────────────────────────────────────────
    if (auto* e = dynamic_cast<const ast::UnaryExpr*>(&expr)) {
        Type operandType = checkExpr(*e->operand);
        if (e->op == ast::UnaryOp::Neg) {
            if (operandType.isNumeric()) return operandType;
            if (!operandType.isUnknown()) {
                error("Cannot negate " + operandType.toString(),
                      expr.line, expr.column);
            }
            return Type::makeUnknown();
        }
        if (e->op == ast::UnaryOp::BitNot) {
            if (operandType.kind == Type::Int) return Type::makeInt();
            if (operandType.kind == Type::Float) {
                error("Cannot apply ~ to float", expr.line, expr.column);
            }
            if (!operandType.isUnknown()) {
                error("Cannot apply ~ to " + operandType.toString(),
                      expr.line, expr.column);
            }
            return Type::makeUnknown();
        }
        // Not
        return Type::makeBool();
    }

    // ── CallExpr ───────────────────────────────────────────────────────
    if (auto* e = dynamic_cast<const ast::CallExpr*>(&expr)) {
        Type calleeType = checkExpr(*e->callee);

        // Check arguments regardless.
        std::vector<Type> argTypes;
        for (const auto& a : e->args) {
            argTypes.push_back(checkExpr(*a));
        }

        if (calleeType.kind == Type::Func) {
            // Check if this is a generic function
            std::string funcName = calleeType.name;
            auto tpIt = funcTypeParams_.find(funcName);

            if (tpIt != funcTypeParams_.end() && !tpIt->second.empty()) {
                // Generic function — resolve type args
                const auto& typeParamNames = tpIt->second;
                std::unordered_map<std::string, Type> typeArgMap;

                if (!e->typeArgs.empty()) {
                    // Explicit type args: identity<int>(42)
                    for (size_t i = 0; i < typeParamNames.size() && i < e->typeArgs.size(); i++) {
                        typeArgMap[typeParamNames[i]] = resolveTypeName(e->typeArgs[i]);
                    }
                } else {
                    // Infer type args from argument types
                    for (size_t i = 0; i < argTypes.size() && i + 1 < calleeType.params.size(); i++) {
                        const Type& paramType = calleeType.params[i + 1];
                        if (paramType.kind == Type::TypeVar) {
                            typeArgMap[paramType.name] = argTypes[i];
                        }
                    }
                }

                // Substitute type args in return type
                Type retType = calleeType.params.empty() ? Type::makeVoid() : calleeType.params[0];
                if (retType.kind == Type::TypeVar) {
                    auto it = typeArgMap.find(retType.name);
                    if (it != typeArgMap.end()) retType = it->second;
                }

                // Check argument count
                size_t expectedArgs = calleeType.params.size() - 1;
                if (argTypes.size() != expectedArgs) {
                    error("Function '" + funcName + "' expects " +
                          std::to_string(expectedArgs) + " argument(s), got " +
                          std::to_string(argTypes.size()), expr.line, expr.column);
                }

                return retType;
            }

            // Non-generic function
            // Check argument count.
            size_t expectedArgs = calleeType.params.size() - 1; // params[0] is return type
            if (argTypes.size() != expectedArgs) {
                error("Function '" + calleeType.name + "' expects " +
                      std::to_string(expectedArgs) + " argument(s), got " +
                      std::to_string(argTypes.size()), expr.line, expr.column);
            }
            // Check argument types with subtype/union support.
            for (size_t i = 0; i < argTypes.size() && i + 1 < calleeType.params.size(); i++) {
                const Type& expected = calleeType.params[i + 1];
                const Type& actual = argTypes[i];
                if (!expected.isUnknown() && !actual.isUnknown() && expected != actual) {
                    if (!isAssignableTo(actual, expected)) {
                        error("Function '" + calleeType.name + "' expects " +
                              expected.toString() + ", got " + actual.toString(),
                              expr.line, expr.column);
                    }
                }
            }
            // Return type
            return calleeType.params.empty() ? Type::makeVoid() : calleeType.params[0];
        }

        if (calleeType.kind == Type::Class) {
            // Constructor call — returns an instance of the class.
            return calleeType;
        }

        if (!calleeType.isUnknown()) {
            // Known type but not callable.
            if (auto* id = dynamic_cast<const ast::Identifier*>(e->callee.get())) {
                error("Cannot call non-function '" + id->name + "'",
                      expr.line, expr.column);
            }
            error("Cannot call non-function type " + calleeType.toString(),
                  expr.line, expr.column);
        }

        return Type::makeUnknown();
    }

    // ── MemberExpr ─────────────────────────────────────────────────────
    if (auto* e = dynamic_cast<const ast::MemberExpr*>(&expr)) {
        checkExpr(*e->object);
        // Member type resolution would require a full class table; return unknown.
        return Type::makeUnknown();
    }

    // ── IndexExpr ──────────────────────────────────────────────────────
    if (auto* e = dynamic_cast<const ast::IndexExpr*>(&expr)) {
        Type objType = checkExpr(*e->object);
        Type idxType = checkExpr(*e->index);

        if (objType.kind == Type::List && !objType.params.empty()) {
            return objType.params[0];
        }
        if (objType.kind == Type::Dict && objType.params.size() >= 2) {
            return objType.params[1];
        }
        if (objType.kind == Type::Str) {
            return Type::makeStr();
        }
        (void)idxType;
        return Type::makeUnknown();
    }

    // ── LambdaExpr ─────────────────────────────────────────────────────
    if (auto* e = dynamic_cast<const ast::LambdaExpr*>(&expr)) {
        enterScope();
        std::vector<Type> paramTypes;
        for (const auto& p : e->params) {
            // Lambda params have no annotations — infer as unknown.
            declare(p, Type::makeUnknown(), expr.line, expr.column);
            paramTypes.push_back(Type::makeUnknown());
        }
        Type bodyType = checkExpr(*e->body);
        exitScope();
        return Type::makeFunc("<lambda>", bodyType, std::move(paramTypes));
    }

    // ── ListExpr ───────────────────────────────────────────────────────
    if (auto* e = dynamic_cast<const ast::ListExpr*>(&expr)) {
        if (e->elements.empty()) {
            return Type::makeList(Type::makeUnknown());
        }
        Type elemType = checkExpr(*e->elements[0]);
        for (size_t i = 1; i < e->elements.size(); i++) {
            Type t = checkExpr(*e->elements[i]);
            elemType = unify(elemType, t, expr.line, expr.column);
        }
        return Type::makeList(elemType);
    }

    // ── DictExpr ───────────────────────────────────────────────────────
    if (auto* e = dynamic_cast<const ast::DictExpr*>(&expr)) {
        if (e->entries.empty()) {
            return Type::makeDict(Type::makeUnknown(), Type::makeUnknown());
        }
        Type keyType = checkExpr(*e->entries[0].first);
        Type valType = checkExpr(*e->entries[0].second);
        for (size_t i = 1; i < e->entries.size(); i++) {
            Type kt = checkExpr(*e->entries[i].first);
            Type vt = checkExpr(*e->entries[i].second);
            keyType = unify(keyType, kt, expr.line, expr.column);
            valType = unify(valType, vt, expr.line, expr.column);
        }
        return Type::makeDict(keyType, valType);
    }

    // ── TupleExpr ──────────────────────────────────────────────────────
    if (auto* e = dynamic_cast<const ast::TupleExpr*>(&expr)) {
        std::vector<Type> elemTypes;
        for (const auto& el : e->elements) {
            elemTypes.push_back(checkExpr(*el));
        }
        return Type::makeTuple(std::move(elemTypes));
    }

    // ── TernaryExpr ────────────────────────────────────────────────────
    if (auto* e = dynamic_cast<const ast::TernaryExpr*>(&expr)) {
        checkExpr(*e->condition);
        Type thenType = checkExpr(*e->thenExpr);
        Type elseType = checkExpr(*e->elseExpr);
        return unify(thenType, elseType, expr.line, expr.column);
    }

    // ── SliceExpr ──────────────────────────────────────────────────────
    if (auto* e = dynamic_cast<const ast::SliceExpr*>(&expr)) {
        Type objType = checkExpr(*e->object);
        if (e->start) checkExpr(*e->start);
        if (e->stop) checkExpr(*e->stop);
        if (e->step) checkExpr(*e->step);
        // Slicing a list returns a list, slicing a string returns a string
        if (objType.kind == Type::List) return objType;
        if (objType.kind == Type::Str) return Type::makeStr();
        return Type::makeUnknown();
    }

    // ── ListComprehension ──────────────────────────────────────────────
    if (auto* e = dynamic_cast<const ast::ListComprehension*>(&expr)) {
        Type iterType = checkExpr(*e->iterable);
        enterScope();
        Type elemType = Type::makeUnknown();
        if (iterType.kind == Type::List && !iterType.params.empty()) {
            elemType = iterType.params[0];
        }
        declare(e->variable, elemType, expr.line, expr.column);
        if (e->condition) checkExpr(*e->condition);
        Type bodyType = checkExpr(*e->body);
        exitScope();
        return Type::makeList(bodyType);
    }

    // ── DictComprehension ──────────────────────────────────────────────
    if (auto* e = dynamic_cast<const ast::DictComprehension*>(&expr)) {
        Type iterType = checkExpr(*e->iterable);
        enterScope();
        Type elemType = Type::makeUnknown();
        if (iterType.kind == Type::List && !iterType.params.empty()) {
            elemType = iterType.params[0];
        }
        declare(e->variable, elemType, expr.line, expr.column);
        if (e->condition) checkExpr(*e->condition);
        Type keyType = checkExpr(*e->key);
        Type valType = checkExpr(*e->value);
        exitScope();
        return Type::makeDict(keyType, valType);
    }

    // ── SpreadExpr ─────────────────────────────────────────────────────
    if (auto* e = dynamic_cast<const ast::SpreadExpr*>(&expr)) {
        return checkExpr(*e->operand);
    }

    return Type::makeUnknown();
}

} // namespace visuall
