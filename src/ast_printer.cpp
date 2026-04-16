#include "ast_printer.h"
#include <sstream>

namespace visuall {

// ── Box-drawing constants ──────────────────────────────────────────────────
static const char* kBranch = "\xe2\x94\x9c\xe2\x94\x80\xe2\x94\x80 ";  // ├──
static const char* kCorner = "\xe2\x94\x94\xe2\x94\x80\xe2\x94\x80 ";  // └──
static const char* kPipe   = "\xe2\x94\x82   ";                          // │
static const char* kSpace  = "    ";                                      //

static std::string connector(bool isLast) { return isLast ? kCorner : kBranch; }
static std::string extension(bool isLast) { return isLast ? kSpace  : kPipe;   }

// ── BinOp → string ────────────────────────────────────────────────────────
const char* AstPrinter::binOpStr(ast::BinOp op) {
    switch (op) {
        case ast::BinOp::Add:   return "+";
        case ast::BinOp::Sub:   return "-";
        case ast::BinOp::Mul:   return "*";
        case ast::BinOp::Div:   return "/";
        case ast::BinOp::IntDiv:return "//";
        case ast::BinOp::Mod:   return "%";
        case ast::BinOp::Pow:   return "**";
        case ast::BinOp::Eq:    return "==";
        case ast::BinOp::Neq:   return "!=";
        case ast::BinOp::Lt:    return "<";
        case ast::BinOp::Gt:    return ">";
        case ast::BinOp::Lte:   return "<=";
        case ast::BinOp::Gte:   return ">=";
        case ast::BinOp::And:   return "and";
        case ast::BinOp::Or:    return "or";
        case ast::BinOp::In:    return "in";
        case ast::BinOp::NotIn: return "not in";
        case ast::BinOp::BitAnd: return "&";
        case ast::BinOp::BitOr:  return "|";
        case ast::BinOp::BitXor: return "^";
        case ast::BinOp::Shl:    return "<<";
        case ast::BinOp::Shr:    return ">>";
    }
    return "?";
}

// ── Compact expression (one-liner) ─────────────────────────────────────────
std::string AstPrinter::compactExpr(const ast::Expr& expr) {
    if (auto* p = dynamic_cast<const ast::IntLiteral*>(&expr))
        return std::to_string(p->value);
    if (auto* p = dynamic_cast<const ast::FloatLiteral*>(&expr)) {
        std::ostringstream ss; ss << p->value; return ss.str();
    }
    if (auto* p = dynamic_cast<const ast::StringLiteral*>(&expr))
        return "\"" + p->value + "\"";
    if (auto* p = dynamic_cast<const ast::FStringLiteral*>(&expr))
        return "f\"" + p->raw + "\"";
    if (auto* p = dynamic_cast<const ast::BoolLiteral*>(&expr))
        return p->value ? "true" : "false";
    if (dynamic_cast<const ast::NullLiteral*>(&expr))
        return "null";
    if (auto* p = dynamic_cast<const ast::Identifier*>(&expr))
        return p->name;
    if (dynamic_cast<const ast::ThisExpr*>(&expr))
        return "this";
    if (dynamic_cast<const ast::SuperExpr*>(&expr))
        return "super";
    if (auto* p = dynamic_cast<const ast::BinaryExpr*>(&expr)) {
        auto l = compactExpr(*p->left);
        auto r = compactExpr(*p->right);
        if (!l.empty() && !r.empty()) {
            auto s = std::string("BinaryOp(") + binOpStr(p->op) + ", " + l + ", " + r + ")";
            if (s.size() < 72) return s;
        }
    }
    if (auto* p = dynamic_cast<const ast::UnaryExpr*>(&expr)) {
        auto o = compactExpr(*p->operand);
        if (!o.empty()) {
            const char* opStr = p->op == ast::UnaryOp::Neg ? "-" :
                                p->op == ast::UnaryOp::BitNot ? "~" : "not";
            return std::string("UnaryOp(") + opStr + ", " + o + ")";
        }
    }
    if (auto* p = dynamic_cast<const ast::CallExpr*>(&expr)) {
        auto c = compactExpr(*p->callee);
        if (!c.empty() && p->args.empty()) return "Call: " + c;
        if (!c.empty()) {
            std::string buf;
            for (const auto& a : p->args) {
                auto ac = compactExpr(*a);
                if (ac.empty()) return "";
                if (!buf.empty()) buf += ", ";
                buf += ac;
            }
            auto s = "Call: " + c + "(" + buf + ")";
            if (s.size() < 80) return s;
        }
    }
    if (auto* p = dynamic_cast<const ast::MemberExpr*>(&expr)) {
        auto o = compactExpr(*p->object);
        if (!o.empty()) return o + "." + p->member;
    }
    if (auto* p = dynamic_cast<const ast::IndexExpr*>(&expr)) {
        auto o = compactExpr(*p->object);
        auto i = compactExpr(*p->index);
        if (!o.empty() && !i.empty()) return o + "[" + i + "]";
    }
    if (auto* p = dynamic_cast<const ast::SpreadExpr*>(&expr)) {
        auto o = compactExpr(*p->operand);
        if (!o.empty()) return "*" + o;
    }
    return ""; // too complex for inline
}

// ════════════════════════════════════════════════════════════════════════════
// Top-level print
// ════════════════════════════════════════════════════════════════════════════
void AstPrinter::print(const ast::Program& program, std::ostream& os) {
    os << "Program\n";
    for (size_t i = 0; i < program.statements.size(); i++) {
        printStmt(*program.statements[i], os, "",
                  i + 1 == program.statements.size());
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Statement list helper (Body / Then / Else sections)
// ════════════════════════════════════════════════════════════════════════════
void AstPrinter::printStmtList(const ast::StmtList& stmts, std::ostream& os,
                               const std::string& prefix, bool isLast,
                               const std::string& label) {
    os << prefix << connector(isLast) << label << "\n";
    std::string childPfx = prefix + extension(isLast);
    for (size_t i = 0; i < stmts.size(); i++) {
        printStmt(*stmts[i], os, childPfx, i + 1 == stmts.size());
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Expression printer
// ════════════════════════════════════════════════════════════════════════════
void AstPrinter::printExpr(const ast::Expr& expr, std::ostream& os,
                           const std::string& prefix, bool isLast) {
    // Try compact form first
    auto compact = compactExpr(expr);
    if (!compact.empty()) {
        os << prefix << connector(isLast) << compact << "\n";
        return;
    }

    std::string childPfx = prefix + extension(isLast);

    // ── Binary ─────────────────────────────────────────────────────────
    if (auto* p = dynamic_cast<const ast::BinaryExpr*>(&expr)) {
        os << prefix << connector(isLast) << "BinaryOp(" << binOpStr(p->op) << ")\n";
        printExpr(*p->left,  os, childPfx, false);
        printExpr(*p->right, os, childPfx, true);
        return;
    }
    // ── Unary ──────────────────────────────────────────────────────────
    if (auto* p = dynamic_cast<const ast::UnaryExpr*>(&expr)) {
        const char* opStr = p->op == ast::UnaryOp::Neg ? "-" :
                            p->op == ast::UnaryOp::BitNot ? "~" : "not";
        os << prefix << connector(isLast) << "UnaryOp(" << opStr << ")\n";
        printExpr(*p->operand, os, childPfx, true);
        return;
    }
    // ── Call ───────────────────────────────────────────────────────────
    if (auto* p = dynamic_cast<const ast::CallExpr*>(&expr)) {
        os << prefix << connector(isLast) << "Call\n";
        bool argsEmpty = p->args.empty();
        printExpr(*p->callee, os, childPfx, argsEmpty);
        for (size_t i = 0; i < p->args.size(); i++) {
            printExpr(*p->args[i], os, childPfx, i + 1 == p->args.size());
        }
        return;
    }
    // ── Member ─────────────────────────────────────────────────────────
    if (auto* p = dynamic_cast<const ast::MemberExpr*>(&expr)) {
        os << prefix << connector(isLast) << "Member(." << p->member << ")\n";
        printExpr(*p->object, os, childPfx, true);
        return;
    }
    // ── Index ──────────────────────────────────────────────────────────
    if (auto* p = dynamic_cast<const ast::IndexExpr*>(&expr)) {
        os << prefix << connector(isLast) << "Index\n";
        printExpr(*p->object, os, childPfx, false);
        printExpr(*p->index,  os, childPfx, true);
        return;
    }
    // ── Lambda ─────────────────────────────────────────────────────────
    if (auto* p = dynamic_cast<const ast::LambdaExpr*>(&expr)) {
        std::string params;
        for (size_t i = 0; i < p->params.size(); i++) {
            if (i > 0) params += ", ";
            params += p->params[i];
        }
        os << prefix << connector(isLast) << "Lambda(" << params << ")\n";
        printExpr(*p->body, os, childPfx, true);
        return;
    }
    // ── List ───────────────────────────────────────────────────────────
    if (auto* p = dynamic_cast<const ast::ListExpr*>(&expr)) {
        os << prefix << connector(isLast) << "List\n";
        for (size_t i = 0; i < p->elements.size(); i++) {
            printExpr(*p->elements[i], os, childPfx, i + 1 == p->elements.size());
        }
        return;
    }
    // ── Dict ───────────────────────────────────────────────────────────
    if (auto* p = dynamic_cast<const ast::DictExpr*>(&expr)) {
        os << prefix << connector(isLast) << "Dict\n";
        for (size_t i = 0; i < p->entries.size(); i++) {
            bool last = (i + 1 == p->entries.size());
            os << childPfx << connector(last) << "Entry\n";
            std::string entryPfx = childPfx + extension(last);
            printExpr(*p->entries[i].first,  os, entryPfx, false);
            printExpr(*p->entries[i].second, os, entryPfx, true);
        }
        return;
    }
    // ── Tuple ──────────────────────────────────────────────────────────
    if (auto* p = dynamic_cast<const ast::TupleExpr*>(&expr)) {
        os << prefix << connector(isLast) << "Tuple\n";
        for (size_t i = 0; i < p->elements.size(); i++) {
            printExpr(*p->elements[i], os, childPfx, i + 1 == p->elements.size());
        }
        return;
    }
    // ── Ternary ────────────────────────────────────────────────────────
    if (auto* p = dynamic_cast<const ast::TernaryExpr*>(&expr)) {
        os << prefix << connector(isLast) << "Ternary\n";
        printExpr(*p->condition, os, childPfx, false);
        printExpr(*p->thenExpr,  os, childPfx, false);
        printExpr(*p->elseExpr,  os, childPfx, true);
        return;
    }
    // ── Slice ──────────────────────────────────────────────────────────
    if (auto* p = dynamic_cast<const ast::SliceExpr*>(&expr)) {
        os << prefix << connector(isLast) << "Slice\n";
        printExpr(*p->object, os, childPfx, false);
        if (p->start) printExpr(*p->start, os, childPfx, !p->stop && !p->step);
        if (p->stop)  printExpr(*p->stop,  os, childPfx, !p->step);
        if (p->step)  printExpr(*p->step,  os, childPfx, true);
        return;
    }
    // ── ListComprehension ──────────────────────────────────────────────
    if (auto* p = dynamic_cast<const ast::ListComprehension*>(&expr)) {
        os << prefix << connector(isLast) << "ListComp: " << p->variable << "\n";
        printExpr(*p->body, os, childPfx, false);
        printExpr(*p->iterable, os, childPfx, !p->condition);
        if (p->condition) printExpr(*p->condition, os, childPfx, true);
        return;
    }
    // ── DictComprehension ──────────────────────────────────────────────
    if (auto* p = dynamic_cast<const ast::DictComprehension*>(&expr)) {
        os << prefix << connector(isLast) << "DictComp: " << p->variable << "\n";
        printExpr(*p->key, os, childPfx, false);
        printExpr(*p->value, os, childPfx, false);
        printExpr(*p->iterable, os, childPfx, !p->condition);
        if (p->condition) printExpr(*p->condition, os, childPfx, true);
        return;
    }
    // ── Spread ─────────────────────────────────────────────────────────
    if (auto* p = dynamic_cast<const ast::SpreadExpr*>(&expr)) {
        os << prefix << connector(isLast) << "Spread\n";
        printExpr(*p->operand, os, childPfx, true);
        return;
    }

    // Fallback
    os << prefix << connector(isLast) << "<expr>\n";
}

// ════════════════════════════════════════════════════════════════════════════
// Statement printer
// ════════════════════════════════════════════════════════════════════════════
void AstPrinter::printStmt(const ast::Stmt& stmt, std::ostream& os,
                           const std::string& prefix, bool isLast) {
    std::string childPfx = prefix + extension(isLast);

    // ── FuncDef ────────────────────────────────────────────────────────
    if (auto* p = dynamic_cast<const ast::FuncDef*>(&stmt)) {
        os << prefix << connector(isLast) << "FunctionDef: " << p->name;
        if (!p->typeParams.empty()) {
            os << "<";
            for (size_t i = 0; i < p->typeParams.size(); i++) {
                if (i > 0) os << ", ";
                os << p->typeParams[i];
            }
            os << ">";
        }
        os << "\n";
        // Children: decorators, params, optional return type, body
        for (size_t i = 0; i < p->decorators.size(); i++) {
            os << childPfx << connector(false) << "Decorator\n";
            printExpr(*p->decorators[i], os, childPfx + kPipe, true);
        }
        size_t childCount = p->params.size() + (p->returnType.empty() ? 0 : 1) + 1;
        size_t idx = 0;
        for (const auto& param : p->params) {
            idx++;
            os << childPfx << connector(idx == childCount) << "Param: " << param.name;
            if (!param.typeAnnotation.empty()) os << " (" << param.typeAnnotation << ")";
            if (param.isVariadic) os << " [variadic]";
            if (param.defaultValue) os << " [default]";
            os << "\n";
        }
        if (!p->returnType.empty()) {
            idx++;
            os << childPfx << connector(idx == childCount) << "ReturnType: " << p->returnType << "\n";
        }
        printStmtList(p->body, os, childPfx, true, "Body");
        return;
    }
    // ── ClassDef ───────────────────────────────────────────────────────
    if (auto* p = dynamic_cast<const ast::ClassDef*>(&stmt)) {
        os << prefix << connector(isLast) << "ClassDef: " << p->name;
        if (!p->typeParams.empty()) {
            os << "<";
            for (size_t i = 0; i < p->typeParams.size(); i++) {
                if (i > 0) os << ", ";
                os << p->typeParams[i];
            }
            os << ">";
        }
        if (p->baseClass) os << " extends " << *p->baseClass;
        if (!p->interfaces.empty()) {
            os << " implements ";
            for (size_t i = 0; i < p->interfaces.size(); i++) {
                if (i > 0) os << ", ";
                os << p->interfaces[i];
            }
        }
        os << "\n";
        for (size_t i = 0; i < p->body.size(); i++) {
            printStmt(*p->body[i], os, childPfx, i + 1 == p->body.size());
        }
        return;
    }
    // ── InitDef ────────────────────────────────────────────────────────
    if (auto* p = dynamic_cast<const ast::InitDef*>(&stmt)) {
        os << prefix << connector(isLast) << "InitDef\n";
        size_t childCount = p->params.size() + 1;
        size_t idx = 0;
        for (const auto& param : p->params) {
            idx++;
            os << childPfx << connector(idx == childCount) << "Param: " << param.name;
            if (!param.typeAnnotation.empty()) os << " (" << param.typeAnnotation << ")";
            if (param.isVariadic) os << " [variadic]";
            if (param.defaultValue) os << " [default]";
            os << "\n";
        }
        printStmtList(p->body, os, childPfx, true, "Body");
        return;
    }
    // ── IfStmt ─────────────────────────────────────────────────────────
    if (auto* p = dynamic_cast<const ast::IfStmt*>(&stmt)) {
        os << prefix << connector(isLast) << "If\n";
        bool hasMore = !p->elsifBranches.empty() || !p->elseBranch.empty();

        // Condition
        os << childPfx << connector(false) << "Condition\n";
        printExpr(*p->condition, os, childPfx + kPipe, true);

        // Then
        printStmtList(p->thenBranch, os, childPfx,
                      !hasMore, "Then");

        // Elsifs
        for (size_t i = 0; i < p->elsifBranches.size(); i++) {
            bool elsifLast = (i + 1 == p->elsifBranches.size()) && p->elseBranch.empty();
            os << childPfx << connector(elsifLast) << "Elsif\n";
            std::string elsifPfx = childPfx + extension(elsifLast);
            printExpr(*p->elsifBranches[i].first, os, elsifPfx, false);
            printStmtList(p->elsifBranches[i].second, os, elsifPfx, true, "Body");
        }

        // Else
        if (!p->elseBranch.empty()) {
            printStmtList(p->elseBranch, os, childPfx, true, "Else");
        }
        return;
    }
    // ── ForStmt ────────────────────────────────────────────────────────
    if (auto* p = dynamic_cast<const ast::ForStmt*>(&stmt)) {
        os << prefix << connector(isLast) << "For: " << p->variable << "\n";
        os << childPfx << connector(false) << "Iterable\n";
        printExpr(*p->iterable, os, childPfx + kPipe, true);
        printStmtList(p->body, os, childPfx, true, "Body");
        return;
    }
    // ── WhileStmt ──────────────────────────────────────────────────────
    if (auto* p = dynamic_cast<const ast::WhileStmt*>(&stmt)) {
        os << prefix << connector(isLast) << "While\n";
        os << childPfx << connector(false) << "Condition\n";
        printExpr(*p->condition, os, childPfx + kPipe, true);
        printStmtList(p->body, os, childPfx, true, "Body");
        return;
    }
    // ── ReturnStmt ─────────────────────────────────────────────────────
    if (auto* p = dynamic_cast<const ast::ReturnStmt*>(&stmt)) {
        if (p->value) {
            os << prefix << connector(isLast) << "Return\n";
            printExpr(*p->value, os, childPfx, true);
        } else {
            os << prefix << connector(isLast) << "Return\n";
        }
        return;
    }
    // ── ThrowStmt ──────────────────────────────────────────────────────
    if (auto* p = dynamic_cast<const ast::ThrowStmt*>(&stmt)) {
        os << prefix << connector(isLast) << "Throw\n";
        printExpr(*p->expr, os, childPfx, true);
        return;
    }
    // ── TryStmt ────────────────────────────────────────────────────────
    if (auto* p = dynamic_cast<const ast::TryStmt*>(&stmt)) {
        os << prefix << connector(isLast) << "Try\n";
        bool hasCatches = !p->catchClauses.empty();
        bool hasFinally = !p->finallyBody.empty();

        printStmtList(p->tryBody, os, childPfx,
                      !hasCatches && !hasFinally, "Body");

        for (size_t i = 0; i < p->catchClauses.size(); i++) {
            bool catchLast = (i + 1 == p->catchClauses.size()) && !hasFinally;
            const auto& cl = p->catchClauses[i];
            std::string label = "Catch: " + cl.exceptionType;
            if (!cl.varName.empty()) label += " as " + cl.varName;
            printStmtList(cl.body, os, childPfx, catchLast, label);
        }

        if (hasFinally) {
            printStmtList(p->finallyBody, os, childPfx, true, "Finally");
        }
        return;
    }
    // ── ImportStmt ─────────────────────────────────────────────────────
    if (auto* p = dynamic_cast<const ast::ImportStmt*>(&stmt)) {
        os << prefix << connector(isLast) << "Import: " << p->module << "\n";
        return;
    }
    // ── FromImportStmt ─────────────────────────────────────────────────
    if (auto* p = dynamic_cast<const ast::FromImportStmt*>(&stmt)) {
        os << prefix << connector(isLast) << "FromImport: " << p->module << " [";
        for (size_t i = 0; i < p->names.size(); i++) {
            if (i > 0) os << ", ";
            os << p->names[i];
        }
        os << "]\n";
        return;
    }
    // ── AssignStmt ─────────────────────────────────────────────────────
    if (auto* p = dynamic_cast<const ast::AssignStmt*>(&stmt)) {
        os << prefix << connector(isLast) << "Assign\n";
        printExpr(*p->target, os, childPfx, false);
        printExpr(*p->value,  os, childPfx, true);
        return;
    }
    // ── ExprStmt ───────────────────────────────────────────────────────
    if (auto* p = dynamic_cast<const ast::ExprStmt*>(&stmt)) {
        os << prefix << connector(isLast) << "ExprStmt\n";
        printExpr(*p->expr, os, childPfx, true);
        return;
    }
    // ── Simple statements ──────────────────────────────────────────────
    if (dynamic_cast<const ast::BreakStmt*>(&stmt)) {
        os << prefix << connector(isLast) << "Break\n";
        return;
    }
    if (dynamic_cast<const ast::ContinueStmt*>(&stmt)) {
        os << prefix << connector(isLast) << "Continue\n";
        return;
    }
    if (dynamic_cast<const ast::PassStmt*>(&stmt)) {
        os << prefix << connector(isLast) << "Pass\n";
        return;
    }
    // ── TupleUnpackStmt ────────────────────────────────────────────────
    if (auto* p = dynamic_cast<const ast::TupleUnpackStmt*>(&stmt)) {
        std::string targets;
        for (size_t i = 0; i < p->targets.size(); i++) {
            if (i > 0) targets += ", ";
            targets += p->targets[i];
        }
        os << prefix << connector(isLast) << "TupleUnpack: " << targets << "\n";
        printExpr(*p->value, os, childPfx, true);
        return;
    }

    // ── InterfaceDef ────────────────────────────────────────────────────
    if (auto* p = dynamic_cast<const ast::InterfaceDef*>(&stmt)) {
        os << prefix << connector(isLast) << "InterfaceDef: " << p->name << "\n";
        for (size_t i = 0; i < p->methods.size(); i++) {
            const auto& m = p->methods[i];
            os << childPfx << connector(i + 1 == p->methods.size())
               << "MethodSig: " << m.name << " -> " << m.returnType << "\n";
        }
        return;
    }

    os << prefix << connector(isLast) << "<unknown stmt>\n";
}

} // namespace visuall
