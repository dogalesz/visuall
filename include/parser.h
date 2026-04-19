#pragma once

#include "ast.h"
#include "token.h"
#include <string>
#include <vector>
#include <stdexcept>

namespace visuall {

// ════════════════════════════════════════════════════════════════════════════
// ParseError — thrown on all parse-time errors.
// Format: "ParseError: [message] at [filename]:[line]:[col]"
// ════════════════════════════════════════════════════════════════════════════
class ParseError : public std::runtime_error {
public:
    std::string filename;
    int line;
    int column;

    ParseError(const std::string& msg, const std::string& file, int ln, int col)
        : std::runtime_error("ParseError: " + msg + " at " +
                             file + ":" + std::to_string(ln) + ":" +
                             std::to_string(col)),
          filename(file), line(ln), column(col) {}
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
