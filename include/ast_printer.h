#pragma once

#include "ast.h"
#include <ostream>
#include <string>

namespace visuall {

// ════════════════════════════════════════════════════════════════════════════
// AstPrinter — produces indented tree output with box-drawing connectors.
//
// Usage:
//   AstPrinter::print(program, std::cout);
//
// Output example:
//   Program
//   └── FunctionDef: greet_user
//       ├── Param: name (str)
//       ├── ReturnType: void
//       └── Body
//           └── Call: print
// ════════════════════════════════════════════════════════════════════════════
class AstPrinter {
public:
    static void print(const ast::Program& program, std::ostream& os);
    static const char* binOpStr(ast::BinOp op);

private:
    static void printStmt(const ast::Stmt& stmt, std::ostream& os,
                          const std::string& prefix, bool isLast);
    static void printExpr(const ast::Expr& expr, std::ostream& os,
                          const std::string& prefix, bool isLast);
    static void printStmtList(const ast::StmtList& stmts, std::ostream& os,
                              const std::string& prefix, bool isLast,
                              const std::string& label);

    // Try to produce a compact one-line representation of an expression.
    // Returns "" if the expression is too complex for inline display.
    static std::string compactExpr(const ast::Expr& expr);
};

} // namespace visuall
