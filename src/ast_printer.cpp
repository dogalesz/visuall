#include "ast_printer.h"
#include "ast_visitor.h"
#include <sstream>

namespace visuall {

// â”€â”€ Box-drawing constants â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
static const char* kBranch = "\xe2\x94\x9c\xe2\x94\x80\xe2\x94\x80 ";  // â”œâ”€â”€
static const char* kCorner = "\xe2\x94\x94\xe2\x94\x80\xe2\x94\x80 ";  // â””â”€â”€
static const char* kPipe   = "\xe2\x94\x82   ";                          // â”‚
static const char* kSpace  = "    ";                                      //

static std::string connector(bool isLast) { return isLast ? kCorner : kBranch; }
static std::string extension(bool isLast) { return isLast ? kSpace  : kPipe;   }

// â”€â”€ BinOp â†’ string â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
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

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Local visitor structs
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
namespace {

// â”€â”€ CompactVisitor â€” produces a one-liner or empty string (too complex). â”€â”€
struct CompactVisitor : public ASTVisitorBase {
    std::string result;

    void visit(const ast::IntLiteral& e)     override { result = std::to_string(e.value); }
    void visit(const ast::FloatLiteral& e)   override { std::ostringstream ss; ss << e.value; result = ss.str(); }
    void visit(const ast::StringLiteral& e)  override { result = "\"" + e.value + "\""; }
    void visit(const ast::FStringLiteral& e) override { result = "f\"" + e.raw + "\""; }
    void visit(const ast::BoolLiteral& e)    override { result = e.value ? "true" : "false"; }
    void visit(const ast::NullLiteral&)      override { result = "null"; }
    void visit(const ast::Identifier& e)     override { result = e.name; }
    void visit(const ast::ThisExpr&)         override { result = "this"; }
    void visit(const ast::SuperExpr&)        override { result = "super"; }

    void visit(const ast::BinaryExpr& e) override {
        CompactVisitor lv, rv;
        e.left->accept(lv);
        e.right->accept(rv);
        if (!lv.result.empty() && !rv.result.empty()) {
            std::string s = std::string("BinaryOp(") +
                            AstPrinter::binOpStr(e.op) + ", " +
                            lv.result + ", " + rv.result + ")";
            if (s.size() < 72) result = s;
        }
    }

    void visit(const ast::UnaryExpr& e) override {
        CompactVisitor ov;
        e.operand->accept(ov);
        if (!ov.result.empty()) {
            const char* opStr = e.op == ast::UnaryOp::Neg ? "-" :
                                e.op == ast::UnaryOp::BitNot ? "~" : "not";
            result = std::string("UnaryOp(") + opStr + ", " + ov.result + ")";
        }
    }

    void visit(const ast::CallExpr& e) override {
        CompactVisitor cv;
        e.callee->accept(cv);
        if (!cv.result.empty() && e.args.empty()) { result = "Call: " + cv.result; return; }
        if (!cv.result.empty()) {
            std::string buf;
            for (const auto& a : e.args) {
                CompactVisitor av;
                a->accept(av);
                if (av.result.empty()) return;
                if (!buf.empty()) buf += ", ";
                buf += av.result;
            }
            std::string s = "Call: " + cv.result + "(" + buf + ")";
            if (s.size() < 80) result = s;
        }
    }

    void visit(const ast::MemberExpr& e) override {
        CompactVisitor ov;
        e.object->accept(ov);
        if (!ov.result.empty()) result = ov.result + "." + e.member;
    }

    void visit(const ast::IndexExpr& e) override {
        CompactVisitor ov, iv;
        e.object->accept(ov);
        e.index->accept(iv);
        if (!ov.result.empty() && !iv.result.empty()) result = ov.result + "[" + iv.result + "]";
    }

    void visit(const ast::SpreadExpr& e) override {
        CompactVisitor ov;
        e.operand->accept(ov);
        if (!ov.result.empty()) result = "*" + ov.result;
    }
    void visit(const ast::DictSpreadExpr& e) override {
        CompactVisitor ov;
        e.operand->accept(ov);
        if (!ov.result.empty()) result = "**" + ov.result;
    }
    void visit(const ast::WalrusExpr& e) override {
        CompactVisitor ov;
        e.value->accept(ov);
        result = e.target + " := " + ov.result;
    }
    // All other nodes leave result empty (too complex for inline)
};

// â”€â”€ PrintVisitor â€” full recursive tree printer. â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
struct PrintVisitor : public ASTVisitorBase {
    std::ostream& os_;
    std::string   prefix_;
    bool          isLast_;

    PrintVisitor(std::ostream& os, std::string pfx, bool last)
        : os_(os), prefix_(std::move(pfx)), isLast_(last) {}

    std::string childPfx() const { return prefix_ + extension(isLast_); }

    void printChildExpr(const ast::Expr& e, const std::string& pfx, bool last) {
        // Try compact first
        CompactVisitor cv;
        e.accept(cv);
        if (!cv.result.empty()) {
            os_ << pfx << connector(last) << cv.result << "\n";
            return;
        }
        PrintVisitor pv(os_, pfx, last);
        e.accept(pv);
    }

    void printChildStmt(const ast::Stmt& s, const std::string& pfx, bool last) {
        PrintVisitor pv(os_, pfx, last);
        s.accept(pv);
    }

    void printStmtListSection(const ast::StmtList& stmts,
                               const std::string& pfx, bool sectionLast,
                               const std::string& label) {
        os_ << pfx << connector(sectionLast) << label << "\n";
        std::string cp = pfx + extension(sectionLast);
        for (size_t i = 0; i < stmts.size(); i++) {
            printChildStmt(*stmts[i], cp, i + 1 == stmts.size());
        }
    }

    // â”€â”€ Expression visitors â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    void visit(const ast::BinaryExpr& e) override {
        os_ << prefix_ << connector(isLast_) << "BinaryOp(" << AstPrinter::binOpStr(e.op) << ")\n";
        printChildExpr(*e.left,  childPfx(), false);
        printChildExpr(*e.right, childPfx(), true);
    }

    void visit(const ast::UnaryExpr& e) override {
        const char* opStr = e.op == ast::UnaryOp::Neg ? "-" :
                            e.op == ast::UnaryOp::BitNot ? "~" : "not";
        os_ << prefix_ << connector(isLast_) << "UnaryOp(" << opStr << ")\n";
        printChildExpr(*e.operand, childPfx(), true);
    }

    void visit(const ast::CallExpr& e) override {
        os_ << prefix_ << connector(isLast_) << "Call\n";
        bool argsEmpty = e.args.empty();
        printChildExpr(*e.callee, childPfx(), argsEmpty);
        for (size_t i = 0; i < e.args.size(); i++) {
            printChildExpr(*e.args[i], childPfx(), i + 1 == e.args.size());
        }
    }

    void visit(const ast::MemberExpr& e) override {
        os_ << prefix_ << connector(isLast_) << "Member(." << e.member << ")\n";
        printChildExpr(*e.object, childPfx(), true);
    }

    void visit(const ast::IndexExpr& e) override {
        os_ << prefix_ << connector(isLast_) << "Index\n";
        printChildExpr(*e.object, childPfx(), false);
        printChildExpr(*e.index,  childPfx(), true);
    }

    void visit(const ast::LambdaExpr& e) override {
        std::string params;
        for (size_t i = 0; i < e.params.size(); i++) {
            if (i > 0) params += ", ";
            params += e.params[i];
        }
        os_ << prefix_ << connector(isLast_) << "Lambda(" << params << ")\n";
        printChildExpr(*e.body, childPfx(), true);
    }

    void visit(const ast::ListExpr& e) override {
        os_ << prefix_ << connector(isLast_) << "List\n";
        for (size_t i = 0; i < e.elements.size(); i++) {
            printChildExpr(*e.elements[i], childPfx(), i + 1 == e.elements.size());
        }
    }

    void visit(const ast::DictExpr& e) override {
        os_ << prefix_ << connector(isLast_) << "Dict\n";
        for (size_t i = 0; i < e.entries.size(); i++) {
            bool last = (i + 1 == e.entries.size());
            os_ << childPfx() << connector(last) << "Entry\n";
            std::string entryPfx = childPfx() + extension(last);
            printChildExpr(*e.entries[i].first,  entryPfx, false);
            printChildExpr(*e.entries[i].second, entryPfx, true);
        }
    }

    void visit(const ast::TupleExpr& e) override {
        os_ << prefix_ << connector(isLast_) << "Tuple\n";
        for (size_t i = 0; i < e.elements.size(); i++) {
            printChildExpr(*e.elements[i], childPfx(), i + 1 == e.elements.size());
        }
    }

    void visit(const ast::TernaryExpr& e) override {
        os_ << prefix_ << connector(isLast_) << "Ternary\n";
        printChildExpr(*e.condition, childPfx(), false);
        printChildExpr(*e.thenExpr,  childPfx(), false);
        printChildExpr(*e.elseExpr,  childPfx(), true);
    }

    void visit(const ast::SliceExpr& e) override {
        os_ << prefix_ << connector(isLast_) << "Slice\n";
        printChildExpr(*e.object, childPfx(), false);
        if (e.start) printChildExpr(*e.start, childPfx(), !e.stop && !e.step);
        if (e.stop)  printChildExpr(*e.stop,  childPfx(), !e.step);
        if (e.step)  printChildExpr(*e.step,  childPfx(), true);
    }

    void visit(const ast::ListComprehension& e) override {
        os_ << prefix_ << connector(isLast_) << "ListComp: " << e.variable << "\n";
        printChildExpr(*e.body,     childPfx(), false);
        printChildExpr(*e.iterable, childPfx(), !e.condition);
        if (e.condition) printChildExpr(*e.condition, childPfx(), true);
    }

    void visit(const ast::DictComprehension& e) override {
        os_ << prefix_ << connector(isLast_) << "DictComp: " << e.variable << "\n";
        printChildExpr(*e.key,      childPfx(), false);
        printChildExpr(*e.value,    childPfx(), false);
        printChildExpr(*e.iterable, childPfx(), !e.condition);
        if (e.condition) printChildExpr(*e.condition, childPfx(), true);
    }

    void visit(const ast::SpreadExpr& e) override {
        os_ << prefix_ << connector(isLast_) << "Spread\n";
        printChildExpr(*e.operand, childPfx(), true);
    }

    void visit(const ast::DictSpreadExpr& e) override {
        os_ << prefix_ << connector(isLast_) << "DictSpread\n";
        printChildExpr(*e.operand, childPfx(), true);
    }

    void visit(const ast::WalrusExpr& e) override {
        os_ << prefix_ << connector(isLast_) << "Walrus: " << e.target << "\n";
        printChildExpr(*e.value, childPfx(), true);
    }

    void visit(const ast::FStringExpr&) override {
        os_ << prefix_ << connector(isLast_) << "FStringExpr\n";
    }

    // Fallback for simple expression nodes (compact always works, but
    // if somehow we land here, print a placeholder).
    void visit(const ast::IntLiteral& e)     override { os_ << prefix_ << connector(isLast_) << std::to_string(e.value) << "\n"; }
    void visit(const ast::FloatLiteral& e)   override { std::ostringstream ss; ss << e.value; os_ << prefix_ << connector(isLast_) << ss.str() << "\n"; }
    void visit(const ast::StringLiteral& e)  override { os_ << prefix_ << connector(isLast_) << "\"" << e.value << "\"\n"; }
    void visit(const ast::FStringLiteral& e) override { os_ << prefix_ << connector(isLast_) << "f\"" << e.raw << "\"\n"; }
    void visit(const ast::BoolLiteral& e)    override { os_ << prefix_ << connector(isLast_) << (e.value ? "true" : "false") << "\n"; }
    void visit(const ast::NullLiteral&)      override { os_ << prefix_ << connector(isLast_) << "null\n"; }
    void visit(const ast::Identifier& e)     override { os_ << prefix_ << connector(isLast_) << e.name << "\n"; }
    void visit(const ast::ThisExpr&)         override { os_ << prefix_ << connector(isLast_) << "this\n"; }
    void visit(const ast::SuperExpr&)        override { os_ << prefix_ << connector(isLast_) << "super\n"; }

    // â”€â”€ Statement visitors â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    void visit(const ast::FuncDef& p) override {
        os_ << prefix_ << connector(isLast_) << "FunctionDef: " << p.name;
        if (!p.typeParams.empty()) {
            os_ << "<";
            for (size_t i = 0; i < p.typeParams.size(); i++) {
                if (i > 0) os_ << ", ";
                os_ << p.typeParams[i];
            }
            os_ << ">";
        }
        os_ << "\n";
        std::string cp = childPfx();
        for (size_t i = 0; i < p.decorators.size(); i++) {
            os_ << cp << connector(false) << "Decorator\n";
            printChildExpr(*p.decorators[i], cp + kPipe, true);
        }
        size_t childCount = p.params.size() + (p.returnType.empty() ? 0 : 1) + 1;
        size_t idx = 0;
        for (const auto& param : p.params) {
            idx++;
            os_ << cp << connector(idx == childCount) << "Param: " << param.name;
            if (!param.typeAnnotation.empty()) os_ << " (" << param.typeAnnotation << ")";
            if (param.isVariadic) os_ << " [variadic]";
            if (param.defaultValue) os_ << " [default]";
            os_ << "\n";
        }
        if (!p.returnType.empty()) {
            idx++;
            os_ << cp << connector(idx == childCount) << "ReturnType: " << p.returnType << "\n";
        }
        printStmtListSection(p.body, cp, true, "Body");
    }

    void visit(const ast::ClassDef& p) override {
        os_ << prefix_ << connector(isLast_) << "ClassDef: " << p.name;
        if (!p.typeParams.empty()) {
            os_ << "<";
            for (size_t i = 0; i < p.typeParams.size(); i++) {
                if (i > 0) os_ << ", ";
                os_ << p.typeParams[i];
            }
            os_ << ">";
        }
        if (p.baseClass) os_ << " extends " << *p.baseClass;
        if (!p.interfaces.empty()) {
            os_ << " implements ";
            for (size_t i = 0; i < p.interfaces.size(); i++) {
                if (i > 0) os_ << ", ";
                os_ << p.interfaces[i];
            }
        }
        os_ << "\n";
        std::string cp = childPfx();
        for (size_t i = 0; i < p.body.size(); i++) {
            printChildStmt(*p.body[i], cp, i + 1 == p.body.size());
        }
    }

    void visit(const ast::InitDef& p) override {
        os_ << prefix_ << connector(isLast_) << "InitDef\n";
        std::string cp = childPfx();
        size_t childCount = p.params.size() + 1;
        size_t idx = 0;
        for (const auto& param : p.params) {
            idx++;
            os_ << cp << connector(idx == childCount) << "Param: " << param.name;
            if (!param.typeAnnotation.empty()) os_ << " (" << param.typeAnnotation << ")";
            if (param.isVariadic) os_ << " [variadic]";
            if (param.defaultValue) os_ << " [default]";
            os_ << "\n";
        }
        printStmtListSection(p.body, cp, true, "Body");
    }

    void visit(const ast::IfStmt& p) override {
        os_ << prefix_ << connector(isLast_) << "If\n";
        std::string cp = childPfx();
        bool hasMore = !p.elsifBranches.empty() || !p.elseBranch.empty();

        os_ << cp << connector(false) << "Condition\n";
        printChildExpr(*p.condition, cp + kPipe, true);

        printStmtListSection(p.thenBranch, cp, !hasMore, "Then");

        for (size_t i = 0; i < p.elsifBranches.size(); i++) {
            bool elsifLast = (i + 1 == p.elsifBranches.size()) && p.elseBranch.empty();
            os_ << cp << connector(elsifLast) << "Elsif\n";
            std::string ep = cp + extension(elsifLast);
            printChildExpr(*p.elsifBranches[i].first, ep, false);
            printStmtListSection(p.elsifBranches[i].second, ep, true, "Body");
        }

        if (!p.elseBranch.empty()) {
            printStmtListSection(p.elseBranch, cp, true, "Else");
        }
    }

    void visit(const ast::ForStmt& p) override {
        os_ << prefix_ << connector(isLast_) << "For: " << p.variable << "\n";
        std::string cp = childPfx();
        os_ << cp << connector(false) << "Iterable\n";
        printChildExpr(*p.iterable, cp + kPipe, true);
        printStmtListSection(p.body, cp, true, "Body");
    }

    void visit(const ast::WhileStmt& p) override {
        os_ << prefix_ << connector(isLast_) << "While\n";
        std::string cp = childPfx();
        os_ << cp << connector(false) << "Condition\n";
        printChildExpr(*p.condition, cp + kPipe, true);
        printStmtListSection(p.body, cp, true, "Body");
    }

    void visit(const ast::ReturnStmt& p) override {
        os_ << prefix_ << connector(isLast_) << "Return\n";
        if (p.value) printChildExpr(*p.value, childPfx(), true);
    }

    void visit(const ast::ThrowStmt& p) override {
        os_ << prefix_ << connector(isLast_) << "Throw\n";
        if (p.expr)
            printChildExpr(*p.expr.value(), childPfx(), true);
    }

    void visit(const ast::TryStmt& p) override {
        os_ << prefix_ << connector(isLast_) << "Try\n";
        std::string cp = childPfx();
        bool hasCatches = !p.catchClauses.empty();
        bool hasFinally = !p.finallyBody.empty();

        printStmtListSection(p.tryBody, cp, !hasCatches && !hasFinally, "Body");

        for (size_t i = 0; i < p.catchClauses.size(); i++) {
            bool catchLast = (i + 1 == p.catchClauses.size()) && !hasFinally;
            const auto& cl = p.catchClauses[i];
            std::string label = "Catch: " + cl.exceptionType;
            if (!cl.varName.empty()) label += " as " + cl.varName;
            printStmtListSection(cl.body, cp, catchLast, label);
        }

        if (hasFinally) {
            printStmtListSection(p.finallyBody, cp, true, "Finally");
        }
    }

    void visit(const ast::ImportStmt& p) override {
        os_ << prefix_ << connector(isLast_) << "Import: " << p.module << "\n";
    }

    void visit(const ast::WithStmt& p) override {
        std::string label = "With";
        if (!p.asName.empty()) label += " as " + p.asName;
        os_ << prefix_ << connector(isLast_) << label << "\n";
        std::string cp = childPfx();
        os_ << cp << connector(false) << "Expr\n";
        printChildExpr(*p.expr, cp + kPipe, true);
        printStmtListSection(p.body, cp, true, "Body");
    }

    void visit(const ast::FromImportStmt& p) override {
        os_ << prefix_ << connector(isLast_) << "FromImport: " << p.module << " [";
        for (size_t i = 0; i < p.names.size(); i++) {
            if (i > 0) os_ << ", ";
            os_ << p.names[i];
        }
        os_ << "]\n";
    }

    void visit(const ast::AssignStmt& p) override {
        os_ << prefix_ << connector(isLast_) << "Assign\n";
        printChildExpr(*p.target, childPfx(), false);
        printChildExpr(*p.value,  childPfx(), true);
    }

    void visit(const ast::ExprStmt& p) override {
        os_ << prefix_ << connector(isLast_) << "ExprStmt\n";
        printChildExpr(*p.expr, childPfx(), true);
    }

    void visit(const ast::BreakStmt&)    override { os_ << prefix_ << connector(isLast_) << "Break\n"; }
    void visit(const ast::ContinueStmt&) override { os_ << prefix_ << connector(isLast_) << "Continue\n"; }
    void visit(const ast::PassStmt&)     override { os_ << prefix_ << connector(isLast_) << "Pass\n"; }

    void visit(const ast::TupleUnpackStmt& p) override {
        std::string targets;
        for (size_t i = 0; i < p.targets.size(); i++) {
            if (i > 0) targets += ", ";
            targets += p.targets[i];
        }
        os_ << prefix_ << connector(isLast_) << "TupleUnpack: " << targets << "\n";
        printChildExpr(*p.value, childPfx(), true);
    }

    void visit(const ast::InterfaceDef& p) override {
        os_ << prefix_ << connector(isLast_) << "InterfaceDef: " << p.name << "\n";
        std::string cp = childPfx();
        for (size_t i = 0; i < p.methods.size(); i++) {
            const auto& m = p.methods[i];
            os_ << cp << connector(i + 1 == p.methods.size())
                << "MethodSig: " << m.name << " -> " << m.returnType << "\n";
        }
    }
};

} // anonymous namespace

// â”€â”€ Compact expression (one-liner) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
std::string AstPrinter::compactExpr(const ast::Expr& expr) {
    CompactVisitor cv;
    expr.accept(cv);
    return cv.result;
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Top-level print
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
void AstPrinter::print(const ast::Program& program, std::ostream& os) {
    os << "Program\n";
    for (size_t i = 0; i < program.statements.size(); i++) {
        printStmt(*program.statements[i], os, "",
                  i + 1 == program.statements.size());
    }
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Statement list helper (Body / Then / Else sections)
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
void AstPrinter::printStmtList(const ast::StmtList& stmts, std::ostream& os,
                               const std::string& prefix, bool isLast,
                               const std::string& label) {
    os << prefix << connector(isLast) << label << "\n";
    std::string childPfx = prefix + extension(isLast);
    for (size_t i = 0; i < stmts.size(); i++) {
        printStmt(*stmts[i], os, childPfx, i + 1 == stmts.size());
    }
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Expression printer
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
void AstPrinter::printExpr(const ast::Expr& expr, std::ostream& os,
                           const std::string& prefix, bool isLast) {
    auto compact = compactExpr(expr);
    if (!compact.empty()) {
        os << prefix << connector(isLast) << compact << "\n";
        return;
    }
    PrintVisitor pv(os, prefix, isLast);
    expr.accept(pv);
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Statement printer
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
void AstPrinter::printStmt(const ast::Stmt& stmt, std::ostream& os,
                           const std::string& prefix, bool isLast) {
    PrintVisitor pv(os, prefix, isLast);
    stmt.accept(pv);
}

} // namespace visuall
