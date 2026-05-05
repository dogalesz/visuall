#pragma once

#include "diagnostic.h"
#include "ast.h"
#include "token.h"
#include <string>
#include <vector>

namespace visuall {

// ════════════════════════════════════════════════════════════════════════════
// ParseError — thrown on all parse-time errors.
// Inherits Diagnostic for clang-style formatting.
// ════════════════════════════════════════════════════════════════════════════
class ParseError : public Diagnostic {
public:
    ParseError(const std::string& msg, const std::string& file, int ln, int col,
               const std::string& src_line = "")
        : Diagnostic(Diagnostic::Severity::Error, msg, "", file, ln, col, src_line) {}
};

class Parser {
public:
    Parser(const std::vector<Token>& tokens, const std::string& filename);
    std::unique_ptr<ast::Program> parse();

    /// Collected parse errors from the last parse() call.
    const std::vector<ParseError>& errors() const { return errors_; }

    /// Parse a single expression from the token stream. Used internally
    /// by f-string interpolation parsing.
    ast::ExprPtr parseExpression();

private:
    std::vector<Token> tokens_;
    std::string        filename_;
    size_t             pos_;
    std::vector<ParseError> errors_;

    // ── Token navigation ───────────────────────────────────────────────
    const Token& current() const;
    const Token& previous() const;
    const Token& advance();
    bool         check(TokenType type) const;
    bool         match(TokenType type);
    const Token& expect(TokenType type, const std::string& msg);
    bool         isAtEnd() const;

    void skipNewlines();

    // ── Statement parsing ──────────────────────────────────────────────
    ast::StmtList parseBlock();
    ast::StmtPtr  parseStatement();
    ast::StmtPtr  parseFuncDef(std::vector<ast::ExprPtr> decorators = {});
    ast::StmtPtr  parseInitDef();
    ast::StmtPtr  parseClassDef();
    ast::StmtPtr  parseIfStmt();
    ast::StmtPtr  parseForStmt();
    ast::StmtPtr  parseWhileStmt();
    ast::StmtPtr  parseReturnStmt();
    ast::StmtPtr  parseTryStmt();
    ast::StmtPtr  parseThrowStmt();
    ast::StmtPtr  parseAssertStmt();
    ast::StmtPtr  parseDelStmt();
    ast::StmtPtr  parseWithStmt();
    ast::StmtPtr  parseMatchStmt();
    ast::StmtPtr  parseImportStmt();
    ast::StmtPtr  parseFromImportStmt();
    ast::StmtPtr  parseBreakStmt();
    ast::StmtPtr  parseContinueStmt();
    ast::StmtPtr  parseExpressionStatement();
    ast::StmtPtr  parseDecorated();
    ast::StmtPtr  parseInterfaceDef();

    std::vector<ast::Param> parseParamList();
    std::string parseTypeAnnotation();

    // ── Expression parsing (precedence climbing) ───────────────────────
    ast::ExprPtr parseTernary();
    ast::ExprPtr parseNullCoalesce();
    ast::ExprPtr parseOr();
    ast::ExprPtr parseAnd();
    ast::ExprPtr parseNot();
    ast::ExprPtr parseBitOr();
    ast::ExprPtr parseBitXor();
    ast::ExprPtr parseBitAnd();
    ast::ExprPtr parseShift();
    ast::ExprPtr parseComparison();
    ast::ExprPtr parseAddSub();
    ast::ExprPtr parseMulDiv();
    ast::ExprPtr parseUnary();
    ast::ExprPtr parsePower();
    ast::ExprPtr parsePostfix();
    ast::ExprPtr parsePrimary();

    // ── Helpers ────────────────────────────────────────────────────────
    [[noreturn]] void error(const std::string& msg) const;
    [[noreturn]] void errorAt(const Token& tok, const std::string& msg) const;
    void synchronize();
};

} // namespace visuall
