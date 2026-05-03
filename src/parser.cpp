#include "parser.h"
#include "lexer.h"
#include <iostream>
#include <sstream>
#include <cassert>

namespace visuall {

// ════════════════════════════════════════════════════════════════════════════
// Deep-copy an expression tree.  Used by chained-comparison desugaring so
// the shared middle operand appears in two ComparisonNodes without any
// aliasing of unique_ptr ownership.
// ════════════════════════════════════════════════════════════════════════════
static ast::ExprPtr cloneExpr(const ast::Expr& expr) {
    if (auto* p = dynamic_cast<const ast::IntLiteral*>(&expr)) {
        auto n = std::make_unique<ast::IntLiteral>(p->value);
        n->line = p->line; n->column = p->column;
        return n;
    }
    if (auto* p = dynamic_cast<const ast::FloatLiteral*>(&expr)) {
        auto n = std::make_unique<ast::FloatLiteral>(p->value);
        n->line = p->line; n->column = p->column;
        return n;
    }
    if (auto* p = dynamic_cast<const ast::StringLiteral*>(&expr)) {
        auto n = std::make_unique<ast::StringLiteral>(p->value);
        n->line = p->line; n->column = p->column;
        return n;
    }
    if (auto* p = dynamic_cast<const ast::FStringLiteral*>(&expr)) {
        auto n = std::make_unique<ast::FStringLiteral>(p->raw);
        n->line = p->line; n->column = p->column;
        return n;
    }
    if (auto* p = dynamic_cast<const ast::BoolLiteral*>(&expr)) {
        auto n = std::make_unique<ast::BoolLiteral>(p->value);
        n->line = p->line; n->column = p->column;
        return n;
    }
    if (dynamic_cast<const ast::NullLiteral*>(&expr)) {
        auto n = std::make_unique<ast::NullLiteral>();
        n->line = expr.line; n->column = expr.column;
        return n;
    }
    if (auto* p = dynamic_cast<const ast::Identifier*>(&expr)) {
        auto n = std::make_unique<ast::Identifier>(p->name);
        n->line = p->line; n->column = p->column;
        return n;
    }
    if (dynamic_cast<const ast::ThisExpr*>(&expr)) {
        auto n = std::make_unique<ast::ThisExpr>();
        n->line = expr.line; n->column = expr.column;
        return n;
    }
    if (dynamic_cast<const ast::SuperExpr*>(&expr)) {
        auto n = std::make_unique<ast::SuperExpr>();
        n->line = expr.line; n->column = expr.column;
        return n;
    }
    if (auto* p = dynamic_cast<const ast::BinaryExpr*>(&expr)) {
        auto n = std::make_unique<ast::BinaryExpr>(
            p->op, cloneExpr(*p->left), cloneExpr(*p->right));
        n->line = p->line; n->column = p->column;
        return n;
    }
    if (auto* p = dynamic_cast<const ast::UnaryExpr*>(&expr)) {
        auto n = std::make_unique<ast::UnaryExpr>(p->op, cloneExpr(*p->operand));
        n->line = p->line; n->column = p->column;
        return n;
    }
    if (auto* p = dynamic_cast<const ast::CallExpr*>(&expr)) {
        ast::ExprList args;
        for (const auto& a : p->args) args.push_back(cloneExpr(*a));
        auto n = std::make_unique<ast::CallExpr>(cloneExpr(*p->callee), std::move(args));
        n->line = p->line; n->column = p->column;
        return n;
    }
    if (auto* p = dynamic_cast<const ast::MemberExpr*>(&expr)) {
        auto n = std::make_unique<ast::MemberExpr>(cloneExpr(*p->object), p->member);
        n->line = p->line; n->column = p->column;
        return n;
    }
    if (auto* p = dynamic_cast<const ast::IndexExpr*>(&expr)) {
        auto n = std::make_unique<ast::IndexExpr>(cloneExpr(*p->object), cloneExpr(*p->index));
        n->line = p->line; n->column = p->column;
        return n;
    }
    if (auto* p = dynamic_cast<const ast::ListExpr*>(&expr)) {
        ast::ExprList elems;
        for (const auto& e : p->elements) elems.push_back(cloneExpr(*e));
        auto n = std::make_unique<ast::ListExpr>(std::move(elems));
        n->line = p->line; n->column = p->column;
        return n;
    }
    if (auto* p = dynamic_cast<const ast::DictExpr*>(&expr)) {
        std::vector<std::pair<ast::ExprPtr, ast::ExprPtr>> entries;
        for (const auto& kv : p->entries)
            entries.emplace_back(cloneExpr(*kv.first), cloneExpr(*kv.second));
        auto n = std::make_unique<ast::DictExpr>(std::move(entries));
        n->line = p->line; n->column = p->column;
        return n;
    }
    if (auto* p = dynamic_cast<const ast::TupleExpr*>(&expr)) {
        ast::ExprList elems;
        for (const auto& e : p->elements) elems.push_back(cloneExpr(*e));
        auto n = std::make_unique<ast::TupleExpr>(std::move(elems));
        n->line = p->line; n->column = p->column;
        return n;
    }
    if (auto* p = dynamic_cast<const ast::TernaryExpr*>(&expr)) {
        auto n = std::make_unique<ast::TernaryExpr>(
            cloneExpr(*p->condition), cloneExpr(*p->thenExpr), cloneExpr(*p->elseExpr));
        n->line = p->line; n->column = p->column;
        return n;
    }
    if (auto* p = dynamic_cast<const ast::LambdaExpr*>(&expr)) {
        auto n = std::make_unique<ast::LambdaExpr>(p->params, cloneExpr(*p->body));
        n->line = p->line; n->column = p->column;
        return n;
    }
    if (auto* p = dynamic_cast<const ast::SliceExpr*>(&expr)) {
        auto n = std::make_unique<ast::SliceExpr>(
            cloneExpr(*p->object),
            p->start ? cloneExpr(*p->start) : nullptr,
            p->stop  ? cloneExpr(*p->stop)  : nullptr,
            p->step  ? cloneExpr(*p->step)  : nullptr);
        n->line = p->line; n->column = p->column;
        return n;
    }
    if (auto* p = dynamic_cast<const ast::ListComprehension*>(&expr)) {
        auto n = std::make_unique<ast::ListComprehension>(
            cloneExpr(*p->body), p->variable,
            cloneExpr(*p->iterable),
            p->condition ? cloneExpr(*p->condition) : nullptr);
        n->line = p->line; n->column = p->column;
        return n;
    }
    if (auto* p = dynamic_cast<const ast::DictComprehension*>(&expr)) {
        auto n = std::make_unique<ast::DictComprehension>(
            cloneExpr(*p->key), cloneExpr(*p->value),
            p->variable, cloneExpr(*p->iterable),
            p->condition ? cloneExpr(*p->condition) : nullptr);
        n->line = p->line; n->column = p->column;
        return n;
    }
    if (auto* p = dynamic_cast<const ast::SpreadExpr*>(&expr)) {
        auto n = std::make_unique<ast::SpreadExpr>(cloneExpr(*p->operand));
        n->line = p->line; n->column = p->column;
        return n;
    }
    if (auto* p = dynamic_cast<const ast::DictSpreadExpr*>(&expr)) {
        auto n = std::make_unique<ast::DictSpreadExpr>(cloneExpr(*p->operand));
        n->line = p->line; n->column = p->column;
        return n;
    }
    // Fallback — should not happen for well-formed trees.
    auto n = std::make_unique<ast::NullLiteral>();
    n->line = expr.line; n->column = expr.column;
    return n;
}

// ════════════════════════════════════════════════════════════════════════════
// Construction
// ════════════════════════════════════════════════════════════════════════════
Parser::Parser(const std::vector<Token>& tokens, const std::string& filename)
    : tokens_(tokens), filename_(filename), pos_(0) {}

// ════════════════════════════════════════════════════════════════════════════
// Token helpers
// ════════════════════════════════════════════════════════════════════════════
const Token& Parser::current() const {
    return tokens_[pos_];
}

const Token& Parser::previous() const {
    return tokens_[pos_ - 1];
}

const Token& Parser::advance() {
    if (!isAtEnd()) pos_++;
    return tokens_[pos_ - 1];
}

bool Parser::check(TokenType type) const {
    return !isAtEnd() && current().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) { advance(); return true; }
    return false;
}

const Token& Parser::expect(TokenType type, const std::string& msg) {
    if (check(type)) return advance();
    error(msg);
}

bool Parser::isAtEnd() const {
    return current().type == TokenType::END_OF_FILE;
}

void Parser::skipNewlines() {
    while (check(TokenType::NEWLINE)) advance();
}

[[noreturn]] void Parser::error(const std::string& msg) const {
    errorAt(current(), msg);
}

[[noreturn]] void Parser::errorAt(const Token& tok, const std::string& msg) const {
    throw ParseError(msg, filename_, tok.line, tok.column);
}

void Parser::synchronize() {
    while (!isAtEnd()) {
        if (current().type == TokenType::NEWLINE ||
            current().type == TokenType::DEDENT) {
            advance();
            return;
        }
        advance();
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Top-level parse — with error recovery via synchronize()
// ════════════════════════════════════════════════════════════════════════════
std::unique_ptr<ast::Program> Parser::parse() {
    ast::StmtList stmts;
    errors_.clear();

    skipNewlines();
    while (!isAtEnd()) {
        try {
            stmts.push_back(parseStatement());
        } catch (const ParseError& e) {
            errors_.push_back(e);
            synchronize();
        }
        skipNewlines();
    }

    // After recovery, re-throw the first error so callers see it.
    if (!errors_.empty()) throw errors_.front();

    return std::make_unique<ast::Program>(std::move(stmts));
}

// ════════════════════════════════════════════════════════════════════════════
// Block parsing:
//   block = INDENT statement+ DEDENT
//         | ELLIPSIS NEWLINE              ← inline no-op body
// ════════════════════════════════════════════════════════════════════════════
ast::StmtList Parser::parseBlock() {
    // Inline no-op:  define foo(): ...
    if (check(TokenType::ELLIPSIS)) {
        int ln = current().line, col = current().column;
        advance();
        match(TokenType::NEWLINE);
        ast::StmtList body;
        auto pass = std::make_unique<ast::PassStmt>();
        pass->line = ln; pass->column = col;
        body.push_back(std::move(pass));
        return body;
    }

    expect(TokenType::NEWLINE, "Expected newline before indented block");
    skipNewlines();
    expect(TokenType::INDENT, "Expected indented block");
    skipNewlines();

    ast::StmtList body;
    while (!check(TokenType::DEDENT) && !isAtEnd()) {
        body.push_back(parseStatement());
        skipNewlines();
    }

    expect(TokenType::DEDENT, "Expected end of indented block");
    return body;
}

// ════════════════════════════════════════════════════════════════════════════
// Statement dispatch
// ════════════════════════════════════════════════════════════════════════════
ast::StmtPtr Parser::parseStatement() {
    skipNewlines();
    TokenType t = current().type;

    switch (t) {
        case TokenType::KW_DEFINE:   return parseFuncDef();
        case TokenType::KW_INIT:     return parseInitDef();
        case TokenType::KW_CLASS:    return parseClassDef();
        case TokenType::KW_INTERFACE:return parseInterfaceDef();
        case TokenType::KW_IF:       return parseIfStmt();
        case TokenType::KW_FOR:      return parseForStmt();
        case TokenType::KW_WHILE:    return parseWhileStmt();
        case TokenType::KW_RETURN:   return parseReturnStmt();
        case TokenType::KW_TRY:      return parseTryStmt();
        case TokenType::KW_THROW:    return parseThrowStmt();
        case TokenType::KW_ASSERT:   return parseAssertStmt();
        case TokenType::KW_DEL:      return parseDelStmt();
        case TokenType::KW_WITH:     return parseWithStmt();
        case TokenType::KW_MATCH:    return parseMatchStmt();
        case TokenType::KW_IMPORT:   return parseImportStmt();
        case TokenType::KW_FROM:     return parseFromImportStmt();
        case TokenType::KW_BREAK:    return parseBreakStmt();
        case TokenType::KW_CONTINUE: return parseContinueStmt();
        case TokenType::AT:          return parseDecorated();
        case TokenType::ELLIPSIS: {
            int ln = current().line, col = current().column;
            advance();
            auto s = std::make_unique<ast::PassStmt>();
            s->line = ln; s->column = col;
            return s;
        }
        default:
            return parseExpressionStatement();
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Parameter list:  (name: type, name2: type2)
// ════════════════════════════════════════════════════════════════════════════
std::vector<ast::Param> Parser::parseParamList() {
    expect(TokenType::LPAREN, "Expected '(' for parameter list");
    std::vector<ast::Param> params;
    if (!check(TokenType::RPAREN)) {
        do {
            ast::Param p;
            // Variadic: *args
            if (match(TokenType::STAR)) {
                p.isVariadic = true;
                p.name = expect(TokenType::IDENTIFIER, "Expected parameter name after '*'").lexeme;
            } else {
                p.name = expect(TokenType::IDENTIFIER, "Expected parameter name").lexeme;
            }
            if (match(TokenType::COLON)) {
                p.typeAnnotation = parseTypeAnnotation();
            }
            // Default value: param = expr
            if (match(TokenType::ASSIGN)) {
                p.defaultValue = parseExpression();
            }
            params.push_back(std::move(p));
        } while (match(TokenType::COMMA));
    }
    expect(TokenType::RPAREN, "Expected ')' after parameters");
    return params;
}

// ════════════════════════════════════════════════════════════════════════════
// define name(params) -> returntype:
//     body
// ════════════════════════════════════════════════════════════════════════════
ast::StmtPtr Parser::parseFuncDef(std::vector<ast::ExprPtr> decorators) {
    int ln = current().line, col = current().column;
    advance(); // consume 'define'
    std::string name = expect(TokenType::IDENTIFIER, "Expected function name").lexeme;

    // Type params: define identity<T>(...)
    std::vector<std::string> typeParams;
    if (match(TokenType::LT)) {
        do {
            typeParams.push_back(expect(TokenType::IDENTIFIER, "Expected type parameter").lexeme);
        } while (match(TokenType::COMMA));
        expect(TokenType::GT, "Expected '>' after type parameters");
    }

    auto params = parseParamList();

    std::string retType;
    if (match(TokenType::ARROW)) {
        retType = parseTypeAnnotation();
    }
    expect(TokenType::COLON, "Expected ':' after function signature");

    auto body = parseBlock();
    auto node = std::make_unique<ast::FuncDef>(name, std::move(params), retType, std::move(body));
    node->decorators = std::move(decorators);
    node->typeParams = std::move(typeParams);
    node->line = ln; node->column = col;
    return node;
}

// ════════════════════════════════════════════════════════════════════════════
// init(params):
//     body
// ════════════════════════════════════════════════════════════════════════════
ast::StmtPtr Parser::parseInitDef() {
    int ln = current().line, col = current().column;
    advance(); // consume 'init'
    auto params = parseParamList();
    expect(TokenType::COLON, "Expected ':' after init signature");
    auto body = parseBlock();
    auto node = std::make_unique<ast::InitDef>(std::move(params), std::move(body));
    node->line = ln; node->column = col;
    return node;
}

// ════════════════════════════════════════════════════════════════════════════
// class Name:
//     body
// ════════════════════════════════════════════════════════════════════════════
ast::StmtPtr Parser::parseClassDef() {
    int ln = current().line, col = current().column;
    advance(); // consume 'class'
    std::string name = expect(TokenType::IDENTIFIER, "Expected class name").lexeme;

    // Type params: class Stack<T>
    std::vector<std::string> typeParams;
    if (match(TokenType::LT)) {
        do {
            typeParams.push_back(expect(TokenType::IDENTIFIER, "Expected type parameter").lexeme);
        } while (match(TokenType::COMMA));
        expect(TokenType::GT, "Expected '>' after type parameters");
    }

    // Inheritance: class Dog extends Animal  OR  class Dog(Animal, Mixin):
    std::optional<std::string> baseClass;
    std::vector<std::string> extraBases;
    if (match(TokenType::KW_EXTENDS)) {
        baseClass = expect(TokenType::IDENTIFIER, "Expected base class name").lexeme;
        // Also allow: class Foo extends A, B (multiple inheritance)
        while (match(TokenType::COMMA)) {
            extraBases.push_back(expect(TokenType::IDENTIFIER, "Expected base class name").lexeme);
        }
    } else if (current().type == TokenType::LPAREN) {
        // Python-style: class Foo(Base1, Base2):
        advance(); // consume '('
        if (current().type != TokenType::RPAREN) {
            baseClass = expect(TokenType::IDENTIFIER, "Expected base class name").lexeme;
            while (match(TokenType::COMMA)) {
                if (current().type == TokenType::RPAREN) break;
                extraBases.push_back(expect(TokenType::IDENTIFIER, "Expected base class name").lexeme);
            }
        }
        expect(TokenType::RPAREN, "Expected ')' after base classes");
    }

    // Interfaces: class Circle implements Drawable, Clickable
    std::vector<std::string> interfaces;
    if (match(TokenType::KW_IMPLEMENTS)) {
        do {
            interfaces.push_back(expect(TokenType::IDENTIFIER, "Expected interface name").lexeme);
        } while (match(TokenType::COMMA));
    }

    expect(TokenType::COLON, "Expected ':' after class header");
    auto body = parseBlock();
    auto node = std::make_unique<ast::ClassDef>(name, std::move(body));
    node->baseClass = baseClass;
    node->extraBases = std::move(extraBases);
    node->interfaces = std::move(interfaces);
    node->typeParams = std::move(typeParams);
    node->line = ln; node->column = col;
    return node;
}

// ════════════════════════════════════════════════════════════════════════════
// if condition:
//     ...
// elsif condition:
//     ...
// else:
//     ...
// ════════════════════════════════════════════════════════════════════════════
ast::StmtPtr Parser::parseIfStmt() {
    int ln = current().line, col = current().column;
    advance(); // consume 'if'
    auto cond = parseExpression();
    expect(TokenType::COLON, "Expected ':' after if condition");
    auto thenBody = parseBlock();

    std::vector<std::pair<ast::ExprPtr, ast::StmtList>> elsifs;
    ast::StmtList elseBody;

    skipNewlines();
    while (check(TokenType::KW_ELSIF)) {
        advance();
        auto ec = parseExpression();
        expect(TokenType::COLON, "Expected ':' after elsif condition");
        auto eb = parseBlock();
        elsifs.emplace_back(std::move(ec), std::move(eb));
        skipNewlines();
    }

    if (match(TokenType::KW_ELSE)) {
        expect(TokenType::COLON, "Expected ':' after else");
        elseBody = parseBlock();
    }

    auto node = std::make_unique<ast::IfStmt>(
        std::move(cond), std::move(thenBody), std::move(elsifs), std::move(elseBody));
    node->line = ln; node->column = col;
    return node;
}

// ════════════════════════════════════════════════════════════════════════════
// for x in iterable:
//     body
// ════════════════════════════════════════════════════════════════════════════
ast::StmtPtr Parser::parseForStmt() {
    int ln = current().line, col = current().column;
    advance(); // consume 'for'
    // Parse one or more comma-separated loop variables: for k, v in ...
    std::string firstVar = expect(TokenType::IDENTIFIER, "Expected loop variable").lexeme;
    std::vector<std::string> extraVars;
    while (match(TokenType::COMMA)) {
        extraVars.push_back(
            expect(TokenType::IDENTIFIER, "Expected identifier in for-loop variable list").lexeme);
    }
    expect(TokenType::KW_IN, "Expected 'in' in for loop");
    auto iter = parseExpression();
    expect(TokenType::COLON, "Expected ':' after for header");
    auto body = parseBlock();
    // Optional else: block
    ast::StmtList elseBranch;
    if (match(TokenType::KW_ELSE)) {
        expect(TokenType::COLON, "Expected ':' after 'else'");
        elseBranch = parseBlock();
    }
    auto node = std::make_unique<ast::ForStmt>(firstVar, std::move(iter), std::move(body));
    if (!extraVars.empty()) {
        node->variables.push_back(firstVar);
        for (auto& v : extraVars) node->variables.push_back(v);
    }
    node->elseBranch = std::move(elseBranch);
    node->line = ln; node->column = col;
    return node;
}

// ════════════════════════════════════════════════════════════════════════════
// while condition:
//     body
// ════════════════════════════════════════════════════════════════════════════
ast::StmtPtr Parser::parseWhileStmt() {
    int ln = current().line, col = current().column;
    advance(); // consume 'while'
    auto cond = parseExpression();
    expect(TokenType::COLON, "Expected ':' after while condition");
    auto body = parseBlock();
    // Optional else: block
    ast::StmtList elseBranch;
    if (match(TokenType::KW_ELSE)) {
        expect(TokenType::COLON, "Expected ':' after 'else'");
        elseBranch = parseBlock();
    }
    auto node = std::make_unique<ast::WhileStmt>(std::move(cond), std::move(body));
    node->elseBranch = std::move(elseBranch);
    node->line = ln; node->column = col;
    return node;
}

// ════════════════════════════════════════════════════════════════════════════
// return [expr]
// ════════════════════════════════════════════════════════════════════════════
ast::StmtPtr Parser::parseReturnStmt() {
    int ln = current().line, col = current().column;
    advance(); // consume 'return'
    ast::ExprPtr val = nullptr;
    if (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE) &&
        !check(TokenType::DEDENT)) {
        val = parseExpression();
    }
    auto node = std::make_unique<ast::ReturnStmt>(std::move(val));
    node->line = ln; node->column = col;
    return node;
}

// ════════════════════════════════════════════════════════════════════════════
// try:
//     body
// catch ExceptionType as e:
//     body
// finally:
//     body
// ════════════════════════════════════════════════════════════════════════════
ast::StmtPtr Parser::parseTryStmt() {
    int ln = current().line, col = current().column;
    advance(); // consume 'try'
    expect(TokenType::COLON, "Expected ':' after try");
    auto tryBody = parseBlock();

    std::vector<ast::CatchClause> catches;
    ast::StmtList finallyBody;

    skipNewlines();
    while (check(TokenType::KW_CATCH)) {
        advance();
        ast::CatchClause clause;
        if (check(TokenType::IDENTIFIER)) {
            clause.exceptionType = advance().lexeme;
            // Accept 'as' as either a keyword or an identifier
            if ((check(TokenType::KW_AS)) ||
                (check(TokenType::IDENTIFIER) && current().lexeme == "as")) {
                advance(); // consume 'as'
                clause.varName = expect(TokenType::IDENTIFIER,
                    "Expected variable name after 'as'").lexeme;
            }
        }
        expect(TokenType::COLON, "Expected ':' after catch");
        clause.body = parseBlock();
        catches.push_back(std::move(clause));
        skipNewlines();
    }

    if (match(TokenType::KW_FINALLY)) {
        expect(TokenType::COLON, "Expected ':' after finally");
        finallyBody = parseBlock();
    }

    auto node = std::make_unique<ast::TryStmt>(
        std::move(tryBody), std::move(catches), std::move(finallyBody));
    node->line = ln; node->column = col;
    return node;
}

// ════════════════════════════════════════════════════════════════════════════
// throw Expr
// ════════════════════════════════════════════════════════════════════════════
ast::StmtPtr Parser::parseThrowStmt() {
    int ln = current().line, col = current().column;
    advance(); // consume 'throw'
    // Bare 'throw' (re-raise) when followed by newline/dedent/EOF
    if (current().type == TokenType::NEWLINE ||
        current().type == TokenType::DEDENT  ||
        current().type == TokenType::END_OF_FILE) {
        auto node = std::make_unique<ast::ThrowStmt>();
        node->line = ln; node->column = col;
        return node;
    }
    auto expr = parseExpression();
    auto node = std::make_unique<ast::ThrowStmt>(std::move(expr));
    node->line = ln; node->column = col;
    return node;
}

// ════════════════════════════════════════════════════════════════════════════
// assert Expr [ , Expr ]
// ════════════════════════════════════════════════════════════════════════════
ast::StmtPtr Parser::parseAssertStmt() {
    int ln = current().line, col = current().column;
    advance(); // consume 'assert'
    auto cond = parseExpression();
    ast::ExprPtr msg;
    if (match(TokenType::COMMA)) {
        msg = parseExpression();
    }
    auto node = std::make_unique<ast::AssertStmt>(std::move(cond), std::move(msg));
    node->line = ln; node->column = col;
    return node;
}

// ════════════════════════════════════════════════════════════════════════════
// del target   (Identifier / IndexExpr / MemberExpr)
// ════════════════════════════════════════════════════════════════════════════
ast::StmtPtr Parser::parseDelStmt() {
    int ln = current().line, col = current().column;
    advance(); // consume 'del'
    auto target = parseExpression();
    auto node = std::make_unique<ast::DelStmt>(std::move(target));
    node->line = ln; node->column = col;
    return node;
}

// ════════════════════════════════════════════════════════════════════════════
// with expr [as name]: INDENT body DEDENT
// ════════════════════════════════════════════════════════════════════════════
ast::StmtPtr Parser::parseWithStmt() {
    int ln = current().line, col = current().column;
    advance(); // consume 'with'
    auto expr = parseExpression();

    std::string asName;
    if (check(TokenType::KW_AS)) {
        advance(); // consume 'as'
        asName = expect(TokenType::IDENTIFIER,
            "Expected variable name after 'as' in with statement").lexeme;
    }

    expect(TokenType::COLON, "Expected ':' after with expression");
    auto body = parseBlock();

    auto node = std::make_unique<ast::WithStmt>(
        std::move(expr), std::move(asName), std::move(body));
    node->line = ln; node->column = col;
    return node;
}

// ════════════════════════════════════════════════════════════════════════════
// match subject:
//     case pattern:
//         body
//     case _:
//         wildcard_body
// ════════════════════════════════════════════════════════════════════════════
ast::StmtPtr Parser::parseMatchStmt() {
    int ln = current().line, col = current().column;
    advance(); // consume 'match'
    auto subject = parseExpression();
    expect(TokenType::COLON, "Expected ':' after match expression");

    expect(TokenType::NEWLINE, "Expected newline after match:");
    skipNewlines();
    expect(TokenType::INDENT, "Expected indented block for match");
    skipNewlines();

    std::vector<ast::MatchCase> cases;
    while (!check(TokenType::DEDENT) && !isAtEnd()) {
        expect(TokenType::KW_CASE, "Expected 'case' in match body");

        // Wildcard: 'case _:' sets pattern to nullptr
        ast::ExprPtr pattern = nullptr;
        if (check(TokenType::IDENTIFIER) && current().lexeme == "_") {
            advance(); // consume '_'
        } else {
            pattern = parseExpression();
        }

        expect(TokenType::COLON, "Expected ':' after case pattern");
        auto body = parseBlock();
        skipNewlines();

        ast::MatchCase mc;
        mc.pattern = std::move(pattern);
        mc.body    = std::move(body);
        cases.push_back(std::move(mc));
    }

    expect(TokenType::DEDENT, "Expected end of match block");

    auto node = std::make_unique<ast::MatchStmt>(std::move(subject), std::move(cases));
    node->line = ln; node->column = col;
    return node;
}

// ════════════════════════════════════════════════════════════════════════════
// import module.submodule
// ════════════════════════════════════════════════════════════════════════════
ast::StmtPtr Parser::parseImportStmt() {
    int ln = current().line, col = current().column;
    advance(); // consume 'import'
    std::string mod = expect(TokenType::IDENTIFIER, "Expected module name").lexeme;
    while (match(TokenType::DOT)) {
        mod += ".";
        mod += expect(TokenType::IDENTIFIER, "Expected module name after '.'").lexeme;
    }
    auto node = std::make_unique<ast::ImportStmt>(mod);
    node->line = ln; node->column = col;
    return node;
}

// ════════════════════════════════════════════════════════════════════════════
// from module import name1, name2
// ════════════════════════════════════════════════════════════════════════════
ast::StmtPtr Parser::parseFromImportStmt() {
    int ln = current().line, col = current().column;
    advance(); // consume 'from'
    std::string mod = expect(TokenType::IDENTIFIER, "Expected module name").lexeme;
    expect(TokenType::KW_IMPORT, "Expected 'import' after module name");
    std::vector<std::string> names;
    do {
        names.push_back(expect(TokenType::IDENTIFIER, "Expected import name").lexeme);
    } while (match(TokenType::COMMA));
    auto node = std::make_unique<ast::FromImportStmt>(mod, std::move(names));
    node->line = ln; node->column = col;
    return node;
}

// ════════════════════════════════════════════════════════════════════════════
// break / continue
// ════════════════════════════════════════════════════════════════════════════
ast::StmtPtr Parser::parseBreakStmt() {
    int ln = current().line, col = current().column;
    advance();
    auto s = std::make_unique<ast::BreakStmt>();
    s->line = ln; s->column = col;
    return s;
}

ast::StmtPtr Parser::parseContinueStmt() {
    int ln = current().line, col = current().column;
    advance();
    auto s = std::make_unique<ast::ContinueStmt>();
    s->line = ln; s->column = col;
    return s;
}

// ════════════════════════════════════════════════════════════════════════════
// @decorator
// define ...
// ════════════════════════════════════════════════════════════════════════════
ast::StmtPtr Parser::parseDecorated() {
    std::vector<ast::ExprPtr> decorators;
    while (check(TokenType::AT)) {
        advance(); // consume '@'
        auto expr = parseExpression();
        decorators.push_back(std::move(expr));
        match(TokenType::NEWLINE);
        skipNewlines();
    }
    if (check(TokenType::KW_DEFINE)) {
        return parseFuncDef(std::move(decorators));
    }
    error("Expected 'define' after decorator(s)");
}

// ════════════════════════════════════════════════════════════════════════════
// Expression statement (or assignment)
// ════════════════════════════════════════════════════════════════════════════
ast::StmtPtr Parser::parseExpressionStatement() {
    int ln = current().line, col = current().column;
    auto expr = parseExpression();

    // ── Tuple unpacking: a, b, c = expr ────────────────────────────────
    if (match(TokenType::COMMA)) {
        // Collect identifier targets
        std::vector<std::string> targets;
        if (auto* id = dynamic_cast<const ast::Identifier*>(expr.get())) {
            targets.push_back(id->name);
        } else {
            error("Expected identifier in unpacking target");
        }
        do {
            targets.push_back(
                expect(TokenType::IDENTIFIER, "Expected identifier in unpacking target").lexeme);
        } while (match(TokenType::COMMA));

        expect(TokenType::ASSIGN, "Expected '=' after unpacking targets");
        auto value = parseExpression();
        auto node = std::make_unique<ast::TupleUnpackStmt>(std::move(targets), std::move(value));
        node->line = ln; node->column = col;
        return node;
    }

    // ── Augmented assignment desugaring ────────────────────────────────
    // x += 1  →  x = x + 1  (and similarly for all aug-assign operators)
    struct AugOp { TokenType tok; ast::BinOp op; };
    AugOp augOps[] = {
        {TokenType::PLUS_EQ,         ast::BinOp::Add},
        {TokenType::MINUS_EQ,        ast::BinOp::Sub},
        {TokenType::STAR_EQ,         ast::BinOp::Mul},
        {TokenType::SLASH_EQ,        ast::BinOp::Div},
        {TokenType::DOUBLE_SLASH_EQ, ast::BinOp::IntDiv},
        {TokenType::PERCENT_EQ,      ast::BinOp::Mod},
        {TokenType::DOUBLE_STAR_EQ,  ast::BinOp::Pow},
        {TokenType::AMP_EQ,          ast::BinOp::BitAnd},
        {TokenType::PIPE_EQ,         ast::BinOp::BitOr},
        {TokenType::CARET_EQ,        ast::BinOp::BitXor},
        {TokenType::LSHIFT_EQ,       ast::BinOp::Shl},
        {TokenType::RSHIFT_EQ,       ast::BinOp::Shr},
    };
    for (auto& aug : augOps) {
        if (match(aug.tok)) {
            auto rhs = parseExpression();
            auto lhsClone = cloneExpr(*expr);
            auto binExpr = std::make_unique<ast::BinaryExpr>(
                aug.op, std::move(lhsClone), std::move(rhs));
            binExpr->line = ln; binExpr->column = col;
            auto node = std::make_unique<ast::AssignStmt>(
                std::move(expr), std::move(binExpr));
            node->line = ln; node->column = col;
            return node;
        }
    }

    if (match(TokenType::ASSIGN)) {
        // Collect all LHS targets for chained assignment: a = b = 1
        std::vector<ast::ExprPtr> targets;
        targets.push_back(std::move(expr));
        auto value = parseExpression();
        while (match(TokenType::ASSIGN)) {
            targets.push_back(std::move(value));
            value = parseExpression();
        }
        // targets holds all but the final RHS; value holds the RHS
        ast::ExprPtr primaryTarget = std::move(targets[0]);
        std::vector<ast::ExprPtr> extras;
        for (size_t i = 1; i < targets.size(); ++i)
            extras.push_back(std::move(targets[i]));
        std::unique_ptr<ast::AssignStmt> node;
        if (extras.empty()) {
            node = std::make_unique<ast::AssignStmt>(std::move(primaryTarget), std::move(value));
        } else {
            node = std::make_unique<ast::AssignStmt>(std::move(primaryTarget), std::move(value), std::move(extras));
        }
        node->line = ln; node->column = col;
        return node;
    }

    auto node = std::make_unique<ast::ExprStmt>(std::move(expr));
    node->line = ln; node->column = col;
    return node;
}

// ════════════════════════════════════════════════════════════════════════════
// Expression parsing — precedence climbing
//
//   expr       → lambda
//   lambda     → IDENT ARROW expr | ternary
//   ternary    → or (IF or ELSE or)?
//   or         → and (OR and)*
//   and        → not (AND not)*
//   not        → NOT not | bitOr
//   bitOr      → bitXor (| bitXor)*
//   bitXor     → bitAnd (^ bitAnd)*
//   bitAnd     → shift (& shift)*
//   shift      → comparison ((<< | >>) comparison)*
//   comparison → addSub (compOp addSub)* [chained desugar]
//   addSub     → mulDiv ((+|-) mulDiv)*
//   mulDiv     → unary ((*|/|//|%) unary)*
//   unary      → (-|~) unary | power
//   power      → postfix (** unary)?        [right-associative]
//   postfix    → primary (.m | (args) | [idx])*
//   primary    → literals | IDENT | (expr) | [list] | {dict} | THIS
// ════════════════════════════════════════════════════════════════════════════

ast::ExprPtr Parser::parseExpression() {
    // Lambda detection:  ident -> expr
    if (check(TokenType::IDENTIFIER)) {
        size_t saved = pos_;
        std::vector<std::string> params;
        params.push_back(advance().lexeme);
        if (match(TokenType::ARROW)) {
            auto body = parseExpression();
            return std::make_unique<ast::LambdaExpr>(std::move(params), std::move(body));
        }
        pos_ = saved; // backtrack
    }
    return parseTernary();
}

// value IF condition ELSE alternative
ast::ExprPtr Parser::parseTernary() {
    auto expr = parseOr();
    if (match(TokenType::KW_IF)) {
        auto cond = parseOr();
        expect(TokenType::KW_ELSE, "Expected 'else' in ternary expression");
        auto alt = parseOr();
        auto node = std::make_unique<ast::TernaryExpr>(
            std::move(cond), std::move(expr), std::move(alt));
        node->line = cond->line; node->column = cond->column;
        return node;
    }
    return expr;
}

ast::ExprPtr Parser::parseOr() {
    auto left = parseAnd();
    while (match(TokenType::KW_OR)) {
        auto right = parseAnd();
        left = std::make_unique<ast::BinaryExpr>(
            ast::BinOp::Or, std::move(left), std::move(right));
    }
    return left;
}

ast::ExprPtr Parser::parseAnd() {
    auto left = parseNot();
    while (match(TokenType::KW_AND)) {
        auto right = parseNot();
        left = std::make_unique<ast::BinaryExpr>(
            ast::BinOp::And, std::move(left), std::move(right));
    }
    return left;
}

ast::ExprPtr Parser::parseNot() {
    if (match(TokenType::KW_NOT)) {
        auto operand = parseNot();
        return std::make_unique<ast::UnaryExpr>(ast::UnaryOp::Not, std::move(operand));
    }
    return parseBitOr();
}

// ── Bitwise OR: a | b ─────────────────────────────────────────────────────
ast::ExprPtr Parser::parseBitOr() {
    auto left = parseBitXor();
    while (match(TokenType::PIPE)) {
        auto right = parseBitXor();
        left = std::make_unique<ast::BinaryExpr>(
            ast::BinOp::BitOr, std::move(left), std::move(right));
    }
    return left;
}

// ── Bitwise XOR: a ^ b ────────────────────────────────────────────────────
ast::ExprPtr Parser::parseBitXor() {
    auto left = parseBitAnd();
    while (match(TokenType::CARET)) {
        auto right = parseBitAnd();
        left = std::make_unique<ast::BinaryExpr>(
            ast::BinOp::BitXor, std::move(left), std::move(right));
    }
    return left;
}

// ── Bitwise AND: a & b ────────────────────────────────────────────────────
ast::ExprPtr Parser::parseBitAnd() {
    auto left = parseShift();
    while (match(TokenType::AMP)) {
        auto right = parseShift();
        left = std::make_unique<ast::BinaryExpr>(
            ast::BinOp::BitAnd, std::move(left), std::move(right));
    }
    return left;
}

// ── Shift: a << b, a >> b ─────────────────────────────────────────────────
ast::ExprPtr Parser::parseShift() {
    auto left = parseComparison();
    while (true) {
        ast::BinOp op;
        if (match(TokenType::LSHIFT))      op = ast::BinOp::Shl;
        else if (match(TokenType::RSHIFT)) op = ast::BinOp::Shr;
        else break;
        auto right = parseComparison();
        left = std::make_unique<ast::BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

// ── Chained comparison desugaring ──────────────────────────────────────────
//   18 <= age <= 65  →  (18 <= age) AND (age <= 65)
//
// Collect all operands and operators, then build the AND chain.
// Middle operands are cloned so each ComparisonNode owns its own copy.
// ───────────────────────────────────────────────────────────────────────────
ast::ExprPtr Parser::parseComparison() {
    // Collect operands and comparison operators.
    std::vector<ast::ExprPtr> operands;
    std::vector<ast::BinOp>   ops;

    operands.push_back(parseAddSub());

    auto tryMatchCompOp = [&]() -> bool {
        if (match(TokenType::EQ))           { ops.push_back(ast::BinOp::Eq);    return true; }
        if (match(TokenType::NEQ))          { ops.push_back(ast::BinOp::Neq);   return true; }
        if (match(TokenType::LT))           { ops.push_back(ast::BinOp::Lt);    return true; }
        if (match(TokenType::GT))           { ops.push_back(ast::BinOp::Gt);    return true; }
        if (match(TokenType::LTE))          { ops.push_back(ast::BinOp::Lte);   return true; }
        if (match(TokenType::GTE))          { ops.push_back(ast::BinOp::Gte);   return true; }
        if (match(TokenType::KW_IN))        { ops.push_back(ast::BinOp::In);    return true; }
        // "not in" as a compound comparison operator
        if (check(TokenType::KW_NOT) && pos_ + 1 < tokens_.size() &&
            tokens_[pos_ + 1].type == TokenType::KW_IN) {
            advance(); advance(); // consume 'not' 'in'
            ops.push_back(ast::BinOp::NotIn);
            return true;
        }
        return false;
    };

    while (tryMatchCompOp()) {
        operands.push_back(parseAddSub());
    }

    // No comparison — just return the single operand.
    if (ops.empty()) return std::move(operands[0]);

    // Single comparison — no chaining needed.
    if (ops.size() == 1) {
        return std::make_unique<ast::BinaryExpr>(
            ops[0], std::move(operands[0]), std::move(operands[1]));
    }

    // Chained comparison: a op0 b op1 c op2 d ...
    // Desugar to: (a op0 b) AND (b op1 c) AND (c op2 d) ...
    // Each shared middle operand is cloned for the left comparison.
    ast::ExprPtr result = std::make_unique<ast::BinaryExpr>(
        ops[0], std::move(operands[0]), cloneExpr(*operands[1]));

    for (size_t i = 1; i < ops.size(); i++) {
        ast::ExprPtr rightOperand;
        if (i + 1 < ops.size()) {
            // More comparisons follow — clone the right operand.
            rightOperand = cloneExpr(*operands[i + 1]);
        } else {
            // Last comparison — move the right operand.
            rightOperand = std::move(operands[i + 1]);
        }
        auto comp = std::make_unique<ast::BinaryExpr>(
            ops[i], std::move(operands[i]), std::move(rightOperand));
        result = std::make_unique<ast::BinaryExpr>(
            ast::BinOp::And, std::move(result), std::move(comp));
    }
    return result;
}

ast::ExprPtr Parser::parseAddSub() {
    auto left = parseMulDiv();
    while (true) {
        ast::BinOp op;
        if (match(TokenType::PLUS))       op = ast::BinOp::Add;
        else if (match(TokenType::MINUS)) op = ast::BinOp::Sub;
        else break;
        auto right = parseMulDiv();
        left = std::make_unique<ast::BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

ast::ExprPtr Parser::parseMulDiv() {
    auto left = parseUnary();
    while (true) {
        ast::BinOp op;
        if (match(TokenType::STAR))             op = ast::BinOp::Mul;
        else if (match(TokenType::SLASH))       op = ast::BinOp::Div;
        else if (match(TokenType::DOUBLE_SLASH)) op = ast::BinOp::IntDiv;
        else if (match(TokenType::PERCENT))     op = ast::BinOp::Mod;
        else break;
        auto right = parseUnary();
        left = std::make_unique<ast::BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

ast::ExprPtr Parser::parseUnary() {
    if (match(TokenType::MINUS)) {
        auto operand = parseUnary();
        return std::make_unique<ast::UnaryExpr>(ast::UnaryOp::Neg, std::move(operand));
    }
    if (match(TokenType::TILDE)) {
        auto operand = parseUnary();
        return std::make_unique<ast::UnaryExpr>(ast::UnaryOp::BitNot, std::move(operand));
    }
    return parsePower();
}

ast::ExprPtr Parser::parsePower() {
    auto base = parsePostfix();
    if (match(TokenType::DOUBLE_STAR)) {
        auto exp = parseUnary(); // right-associative
        return std::make_unique<ast::BinaryExpr>(ast::BinOp::Pow, std::move(base), std::move(exp));
    }
    return base;
}

ast::ExprPtr Parser::parsePostfix() {
    auto expr = parsePrimary();

    while (true) {
        if (match(TokenType::LPAREN)) {
            // function call
            ast::ExprList args;
            if (!check(TokenType::RPAREN)) {
                do {
                    args.push_back(parseExpression());
                } while (match(TokenType::COMMA));
            }
            expect(TokenType::RPAREN, "Expected ')' after arguments");
            expr = std::make_unique<ast::CallExpr>(std::move(expr), std::move(args));
        } else if (match(TokenType::DOT)) {
            // Allow keywords like 'init' as member names after '.'
            std::string member;
            if (match(TokenType::KW_INIT)) {
                member = "init";
            } else {
                member = expect(TokenType::IDENTIFIER, "Expected member name").lexeme;
            }
            expr = std::make_unique<ast::MemberExpr>(std::move(expr), member);
        } else if (check(TokenType::LT)) {
            // Try to parse as generic type args: expr<T1, T2>(args)
            size_t saved = pos_;
            advance(); // consume '<'
            std::vector<std::string> typeArgs;
            bool isGenericCall = true;

            if (check(TokenType::IDENTIFIER)) {
                typeArgs.push_back(advance().lexeme);
                while (match(TokenType::COMMA)) {
                    if (!check(TokenType::IDENTIFIER)) {
                        isGenericCall = false;
                        break;
                    }
                    typeArgs.push_back(advance().lexeme);
                }
                if (isGenericCall && match(TokenType::GT) && check(TokenType::LPAREN)) {
                    // It's a generic call
                    advance(); // consume '('
                    ast::ExprList args;
                    if (!check(TokenType::RPAREN)) {
                        do {
                            args.push_back(parseExpression());
                        } while (match(TokenType::COMMA));
                    }
                    expect(TokenType::RPAREN, "Expected ')' after arguments");
                    auto call = std::make_unique<ast::CallExpr>(std::move(expr), std::move(args));
                    call->typeArgs = std::move(typeArgs);
                    expr = std::move(call);
                    continue;
                }
            }
            // Not a generic call, backtrack
            pos_ = saved;
            break;
        } else if (match(TokenType::LBRACKET)) {
            int sliceLn = previous().line, sliceCol = previous().column;
            // Check for slice notation: [start:stop:step]
            ast::ExprPtr startExpr = nullptr;
            if (!check(TokenType::COLON) && !check(TokenType::RBRACKET)) {
                startExpr = parseExpression();
            }
            if (check(TokenType::COLON)) {
                // This is a slice expression
                advance(); // consume first ':'
                ast::ExprPtr stopExpr = nullptr;
                ast::ExprPtr stepExpr = nullptr;
                if (!check(TokenType::COLON) && !check(TokenType::RBRACKET)) {
                    stopExpr = parseExpression();
                }
                if (match(TokenType::COLON)) {
                    if (!check(TokenType::RBRACKET)) {
                        stepExpr = parseExpression();
                    }
                }
                expect(TokenType::RBRACKET, "Expected ']' after slice");
                auto node = std::make_unique<ast::SliceExpr>(
                    std::move(expr), std::move(startExpr),
                    std::move(stopExpr), std::move(stepExpr));
                node->line = sliceLn; node->column = sliceCol;
                expr = std::move(node);
            } else {
                // Regular index
                if (!startExpr) error("Expected expression in index");
                expect(TokenType::RBRACKET, "Expected ']' after index");
                expr = std::make_unique<ast::IndexExpr>(std::move(expr), std::move(startExpr));
            }
        } else {
            break;
        }
    }

    return expr;
}

ast::ExprPtr Parser::parsePrimary() {
    int ln = current().line, col = current().column;

    // ── Literals ───────────────────────────────────────────────────────
    if (match(TokenType::INT_LITERAL)) {
        auto node = std::make_unique<ast::IntLiteral>(std::stoll(previous().lexeme));
        node->line = ln; node->column = col;
        return node;
    }
    if (match(TokenType::FLOAT_LITERAL)) {
        auto node = std::make_unique<ast::FloatLiteral>(std::stod(previous().lexeme));
        node->line = ln; node->column = col;
        return node;
    }
    if (match(TokenType::STRING_LITERAL)) {
        auto node = std::make_unique<ast::StringLiteral>(previous().lexeme);
        node->line = ln; node->column = col;
        return node;
    }
    if (match(TokenType::FSTRING_LITERAL)) {
        // Parse f-string into FStringExpr with alternating literal/expression parts.
        std::string raw = previous().lexeme;
        // raw is: f"content" or f'content' — strip f + quote + closing quote
        char quote = raw[1];
        std::string content = raw.substr(2, raw.size() - 3); // strip f, quote, closing quote
        (void)quote;

        std::vector<ast::FStringPart> parts;
        std::string currentLiteral;
        size_t i = 0;
        while (i < content.size()) {
            if (content[i] == '\\' && i + 1 < content.size()) {
                // Escape sequences
                char next = content[i + 1];
                switch (next) {
                    case 'n': currentLiteral += '\n'; break;
                    case 't': currentLiteral += '\t'; break;
                    case '\\': currentLiteral += '\\'; break;
                    case '\'': currentLiteral += '\''; break;
                    case '"': currentLiteral += '"'; break;
                    case '{': currentLiteral += '{'; break;
                    case '}': currentLiteral += '}'; break;
                    default: currentLiteral += '\\'; currentLiteral += next; break;
                }
                i += 2;
            } else if (content[i] == '{') {
                // Flush literal
                if (!currentLiteral.empty()) {
                    ast::FStringPart litPart;
                    litPart.isExpr = false;
                    litPart.literal = currentLiteral;
                    parts.push_back(std::move(litPart));
                    currentLiteral.clear();
                }
                // Extract expression text between { and }
                i++; // skip '{'
                std::string exprText;
                while (i < content.size() && content[i] != '}') {
                    exprText += content[i++];
                }
                if (i < content.size()) i++; // skip '}'

                // Parse the expression: lex + parse it
                Lexer exprLexer(exprText, filename_);
                auto exprTokens = exprLexer.tokenize();
                Parser exprParser(exprTokens, filename_);
                auto exprAst = exprParser.parseExpression();

                ast::FStringPart exprPart;
                exprPart.isExpr = true;
                exprPart.expr = std::move(exprAst);
                parts.push_back(std::move(exprPart));
            } else {
                currentLiteral += content[i++];
            }
        }
        // Flush remaining literal
        if (!currentLiteral.empty()) {
            ast::FStringPart litPart;
            litPart.isExpr = false;
            litPart.literal = currentLiteral;
            parts.push_back(std::move(litPart));
        }

        auto node = std::make_unique<ast::FStringExpr>(std::move(parts));
        node->line = ln; node->column = col;
        return node;
    }
    if (match(TokenType::BOOL_LITERAL)) {
        auto node = std::make_unique<ast::BoolLiteral>(previous().lexeme == "true");
        node->line = ln; node->column = col;
        return node;
    }
    if (match(TokenType::NULL_LITERAL)) {
        auto node = std::make_unique<ast::NullLiteral>();
        node->line = ln; node->column = col;
        return node;
    }

    // ── Spread/unpack: *expr ──────────────────────────────────────────
    if (match(TokenType::STAR)) {
        auto operand = parseUnary();
        auto node = std::make_unique<ast::SpreadExpr>(std::move(operand));
        node->line = ln; node->column = col;
        return node;
    }

    // ── Dict-spread: **expr ───────────────────────────────────────────
    if (match(TokenType::DOUBLE_STAR)) {
        auto operand = parseUnary();
        auto node = std::make_unique<ast::DictSpreadExpr>(std::move(operand));
        node->line = ln; node->column = col;
        return node;
    }

    // ── this ───────────────────────────────────────────────────────────
    if (match(TokenType::KW_THIS)) {
        auto node = std::make_unique<ast::ThisExpr>();
        node->line = ln; node->column = col;
        return node;
    }

    // ── super ──────────────────────────────────────────────────────────
    if (match(TokenType::KW_SUPER)) {
        auto node = std::make_unique<ast::SuperExpr>();
        node->line = ln; node->column = col;
        return node;
    }

    // ── Identifiers ────────────────────────────────────────────────────
    if (match(TokenType::IDENTIFIER)) {
        auto node = std::make_unique<ast::Identifier>(previous().lexeme);
        node->line = ln; node->column = col;
        return node;
    }

    // ── Parenthesized expression or tuple ──────────────────────────────
    if (match(TokenType::LPAREN)) {
        if (match(TokenType::RPAREN)) {
            // empty tuple
            auto node = std::make_unique<ast::TupleExpr>(ast::ExprList{});
            node->line = ln; node->column = col;
            return node;
        }
        auto first = parseExpression();
        if (match(TokenType::COMMA)) {
            // tuple
            ast::ExprList elements;
            elements.push_back(std::move(first));
            if (!check(TokenType::RPAREN)) {
                do {
                    elements.push_back(parseExpression());
                } while (match(TokenType::COMMA));
            }
            expect(TokenType::RPAREN, "Expected ')' to close tuple");
            auto node = std::make_unique<ast::TupleExpr>(std::move(elements));
            node->line = ln; node->column = col;
            return node;
        }
        expect(TokenType::RPAREN, "Expected ')'");
        return first;
    }

    // ── List literal or list comprehension ─────────────────────────────
    if (match(TokenType::LBRACKET)) {
        if (check(TokenType::RBRACKET)) {
            advance();
            auto node = std::make_unique<ast::ListExpr>(ast::ExprList{});
            node->line = ln; node->column = col;
            return node;
        }
        auto first = parseExpression();
        // Check for list comprehension: [expr for var in iter]
        if (check(TokenType::KW_FOR)) {
            advance(); // consume 'for'
            std::string var = expect(TokenType::IDENTIFIER, "Expected loop variable in comprehension").lexeme;
            expect(TokenType::KW_IN, "Expected 'in' in comprehension");
            auto iterable = parseOr();
            ast::ExprPtr condition = nullptr;
            if (match(TokenType::KW_IF)) {
                condition = parseOr();
            }
            expect(TokenType::RBRACKET, "Expected ']' after list comprehension");
            auto node = std::make_unique<ast::ListComprehension>(
                std::move(first), std::move(var), std::move(iterable), std::move(condition));
            node->line = ln; node->column = col;
            return node;
        }
        ast::ExprList elements;
        elements.push_back(std::move(first));
        while (match(TokenType::COMMA)) {
            if (check(TokenType::RBRACKET)) break; // trailing comma
            elements.push_back(parseExpression());
        }
        expect(TokenType::RBRACKET, "Expected ']'");
        auto node = std::make_unique<ast::ListExpr>(std::move(elements));
        node->line = ln; node->column = col;
        return node;
    }

    // ── Dict literal or dict comprehension ─────────────────────────────
    if (match(TokenType::LBRACE)) {
        if (check(TokenType::RBRACE)) {
            advance();
            auto node = std::make_unique<ast::DictExpr>(
                std::vector<std::pair<ast::ExprPtr, ast::ExprPtr>>{});
            node->line = ln; node->column = col;
            return node;
        }
        auto firstKey = parseExpression();
        expect(TokenType::COLON, "Expected ':' in dict entry");
        auto firstVal = parseExpression();
        // Check for dict comprehension: {k: v for var in iter}
        if (check(TokenType::KW_FOR)) {
            advance(); // consume 'for'
            std::string var = expect(TokenType::IDENTIFIER, "Expected loop variable in comprehension").lexeme;
            expect(TokenType::KW_IN, "Expected 'in' in comprehension");
            auto iterable = parseOr();
            ast::ExprPtr condition = nullptr;
            if (match(TokenType::KW_IF)) {
                condition = parseOr();
            }
            expect(TokenType::RBRACE, "Expected '}' after dict comprehension");
            auto node = std::make_unique<ast::DictComprehension>(
                std::move(firstKey), std::move(firstVal), std::move(var),
                std::move(iterable), std::move(condition));
            node->line = ln; node->column = col;
            return node;
        }
        std::vector<std::pair<ast::ExprPtr, ast::ExprPtr>> entries;
        entries.emplace_back(std::move(firstKey), std::move(firstVal));
        while (match(TokenType::COMMA)) {
            if (check(TokenType::RBRACE)) break; // trailing comma
            auto key = parseExpression();
            expect(TokenType::COLON, "Expected ':' in dict entry");
            auto val = parseExpression();
            entries.emplace_back(std::move(key), std::move(val));
        }
        expect(TokenType::RBRACE, "Expected '}'");
        auto node = std::make_unique<ast::DictExpr>(std::move(entries));
        node->line = ln; node->column = col;
        return node;
    }

    error("Expected expression, got '" + current().lexeme + "'");
}

// ════════════════════════════════════════════════════════════════════════════
// interface Name:
//     define method(params) -> rettype
// ════════════════════════════════════════════════════════════════════════════
ast::StmtPtr Parser::parseInterfaceDef() {
    int ln = current().line, col = current().column;
    advance(); // consume 'interface'
    std::string name = expect(TokenType::IDENTIFIER, "Expected interface name").lexeme;
    expect(TokenType::COLON, "Expected ':' after interface name");

    // Parse block with method signatures (no body)
    expect(TokenType::NEWLINE, "Expected newline before indented block");
    skipNewlines();
    expect(TokenType::INDENT, "Expected indented block");
    skipNewlines();

    std::vector<ast::MethodSignature> methods;
    while (!check(TokenType::DEDENT) && !isAtEnd()) {
        expect(TokenType::KW_DEFINE, "Expected method declaration in interface");
        std::string methodName = expect(TokenType::IDENTIFIER, "Expected method name").lexeme;
        auto params = parseParamList();
        std::string retType;
        if (match(TokenType::ARROW)) {
            retType = parseTypeAnnotation();
        }
        ast::MethodSignature sig;
        sig.name = methodName;
        sig.params = std::move(params);
        sig.returnType = retType;
        methods.push_back(std::move(sig));
        match(TokenType::NEWLINE);
        skipNewlines();
    }
    expect(TokenType::DEDENT, "Expected end of interface block");

    auto node = std::make_unique<ast::InterfaceDef>(name, std::move(methods));
    node->line = ln; node->column = col;
    return node;
}

// ════════════════════════════════════════════════════════════════════════════
// Type annotation: int, int | null, ?str, list[int], dict[str, int]
// ════════════════════════════════════════════════════════════════════════════
std::string Parser::parseTypeAnnotation() {
    std::string result;

    // Handle optional shorthand: ?T → "T|null"
    if (match(TokenType::QUESTION)) {
        result = parseTypeAnnotation();
        return result + "|null";
    }

    // Handle function type: (int, str) -> bool
    if (check(TokenType::LPAREN)) {
        // Check if this is a function type by looking for ')' followed by '->'
        // Save position and try to parse
        size_t savedPos = pos_;
        advance(); // consume '('
        result = "(";

        // Parse parameter types
        if (!check(TokenType::RPAREN)) {
            result += parseTypeAnnotation();
            while (match(TokenType::COMMA)) {
                result += ",";
                result += parseTypeAnnotation();
            }
        }

        if (match(TokenType::RPAREN) && match(TokenType::ARROW)) {
            result += ")->";
            result += parseTypeAnnotation();
            return result;
        }

        // Not a function type — restore position (shouldn't normally happen
        // since the grammar is unambiguous at this point)
        pos_ = savedPos;
        result.clear();
    }

    // Base type name (identifiers and 'null' keyword)
    if (match(TokenType::NULL_LITERAL)) {
        result = "null";
    } else {
        result = expect(TokenType::IDENTIFIER, "Expected type name").lexeme;
    }

    // Handle generic type args: list[int], dict[str, int]
    if (match(TokenType::LBRACKET)) {
        result += "[";
        result += parseTypeAnnotation();
        while (match(TokenType::COMMA)) {
            result += ",";
            result += parseTypeAnnotation();
        }
        expect(TokenType::RBRACKET, "Expected ']' in type annotation");
        result += "]";
    }

    // Handle union: int | null, int | str
    if (match(TokenType::PIPE)) {
        result += "|";
        result += parseTypeAnnotation();
    }

    return result;
}

} // namespace visuall
