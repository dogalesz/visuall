#pragma once

#include <string>
#include <ostream>

namespace visuall {

// ════════════════════════════════════════════════════════════════════════════
// TokenType — every token the Visuall lexer can produce.
// ════════════════════════════════════════════════════════════════════════════
enum class TokenType {
    // ── Literals ───────────────────────────────────────────────────────────
    INT_LITERAL,        // 42
    FLOAT_LITERAL,      // 3.14
    STRING_LITERAL,     // "hello" or 'hello'
    FSTRING_LITERAL,    // f"hello {name}"
    BOOL_LITERAL,       // true / false
    NULL_LITERAL,       // null

    // ── Identifiers ────────────────────────────────────────────────────────
    IDENTIFIER,         // user-defined names

    // ── Keywords ───────────────────────────────────────────────────────────
    KW_DEFINE,          // define
    KW_INIT,            // init
    KW_CLASS,           // class
    KW_IF,              // if
    KW_ELSIF,           // elsif
    KW_ELSE,            // else
    KW_FOR,             // for
    KW_WHILE,           // while
    KW_IN,              // in
    KW_RETURN,          // return
    KW_BREAK,           // break
    KW_CONTINUE,        // continue
    KW_PASS,            // pass
    KW_AND,             // and
    KW_OR,              // or
    KW_NOT,             // not
    KW_TRUE,            // true  (also produces BOOL_LITERAL)
    KW_FALSE,           // false (also produces BOOL_LITERAL)
    KW_NULL,            // null  (also produces NULL_LITERAL)
    KW_IMPORT,          // import
    KW_FROM,            // from
    KW_TRY,             // try
    KW_CATCH,           // catch
    KW_FINALLY,         // finally
    KW_THROW,           // throw
    KW_THIS,            // this
    KW_LAMBDA,          // reserved
    KW_EXTENDS,         // extends
    KW_IMPLEMENTS,      // implements
    KW_INTERFACE,       // interface
    KW_SUPER,           // super

    // ── Operators ──────────────────────────────────────────────────────────
    PLUS,               // +
    MINUS,              // -
    STAR,               // *
    SLASH,              // /
    DOUBLE_SLASH,       // //
    PERCENT,            // %
    DOUBLE_STAR,        // **
    ASSIGN,             // =
    EQ,                 // ==
    NEQ,                // !=
    LT,                 // <
    GT,                 // >
    LTE,                // <=
    GTE,                // >=
    ARROW,              // ->
    DOT,                // .

    // ── Augmented assignment ────────────────────────────────────────────────
    PLUS_EQ,            // +=
    MINUS_EQ,           // -=
    STAR_EQ,            // *=
    SLASH_EQ,           // /=
    DOUBLE_SLASH_EQ,    // //=
    PERCENT_EQ,         // %=
    DOUBLE_STAR_EQ,     // **=
    AMP_EQ,             // &=
    PIPE_EQ,            // |=
    CARET_EQ,           // ^=
    LSHIFT_EQ,          // <<=
    RSHIFT_EQ,          // >>=

    // ── Bitwise operators ──────────────────────────────────────────────────
    AMP,                // &
    PIPE,               // |
    CARET,              // ^
    TILDE,              // ~
    LSHIFT,             // <<
    RSHIFT,             // >>

    // ── Delimiters ─────────────────────────────────────────────────────────
    LPAREN,             // (
    RPAREN,             // )
    LBRACKET,           // [
    RBRACKET,           // ]
    LBRACE,             // {
    RBRACE,             // }
    COMMA,              // ,
    COLON,              // :
    ELLIPSIS,           // ...
    AT,                 // @
    QUESTION,           // ?

    // ── Structural ─────────────────────────────────────────────────────────
    NEWLINE,            // logical end-of-line
    INDENT,             // indentation level increased
    DEDENT,             // indentation level decreased
    END_OF_FILE,        // end of input
};

// ════════════════════════════════════════════════════════════════════════════
// Token — a single lexeme produced by the lexer.
// ════════════════════════════════════════════════════════════════════════════
struct Token {
    TokenType   type;
    std::string lexeme;
    int         line;
    int         column;

    // Human-readable representation used by the --tokens flag.
    std::string toString() const;
};

// ════════════════════════════════════════════════════════════════════════════
// tokenTypeToString — map a TokenType to its printable name.
// Also aliased as tokenTypeName for backward-compatibility with the parser.
// ════════════════════════════════════════════════════════════════════════════
inline const char* tokenTypeToString(TokenType t) {
    switch (t) {
        case TokenType::INT_LITERAL:    return "INT_LITERAL";
        case TokenType::FLOAT_LITERAL:  return "FLOAT_LITERAL";
        case TokenType::STRING_LITERAL: return "STRING_LITERAL";
        case TokenType::FSTRING_LITERAL:return "FSTRING_LITERAL";
        case TokenType::BOOL_LITERAL:   return "BOOL_LITERAL";
        case TokenType::NULL_LITERAL:   return "NULL_LITERAL";
        case TokenType::IDENTIFIER:     return "IDENTIFIER";
        case TokenType::KW_DEFINE:      return "KW_DEFINE";
        case TokenType::KW_INIT:        return "KW_INIT";
        case TokenType::KW_CLASS:       return "KW_CLASS";
        case TokenType::KW_IF:          return "KW_IF";
        case TokenType::KW_ELSIF:       return "KW_ELSIF";
        case TokenType::KW_ELSE:        return "KW_ELSE";
        case TokenType::KW_FOR:         return "KW_FOR";
        case TokenType::KW_WHILE:       return "KW_WHILE";
        case TokenType::KW_IN:          return "KW_IN";
        case TokenType::KW_RETURN:      return "KW_RETURN";
        case TokenType::KW_BREAK:       return "KW_BREAK";
        case TokenType::KW_CONTINUE:    return "KW_CONTINUE";
        case TokenType::KW_PASS:        return "KW_PASS";
        case TokenType::KW_AND:         return "KW_AND";
        case TokenType::KW_OR:          return "KW_OR";
        case TokenType::KW_NOT:         return "KW_NOT";
        case TokenType::KW_TRUE:        return "KW_TRUE";
        case TokenType::KW_FALSE:       return "KW_FALSE";
        case TokenType::KW_NULL:        return "KW_NULL";
        case TokenType::KW_IMPORT:      return "KW_IMPORT";
        case TokenType::KW_FROM:        return "KW_FROM";
        case TokenType::KW_TRY:         return "KW_TRY";
        case TokenType::KW_CATCH:       return "KW_CATCH";
        case TokenType::KW_FINALLY:     return "KW_FINALLY";
        case TokenType::KW_THROW:       return "KW_THROW";
        case TokenType::KW_THIS:        return "KW_THIS";
        case TokenType::KW_LAMBDA:      return "KW_LAMBDA";
        case TokenType::KW_EXTENDS:     return "KW_EXTENDS";
        case TokenType::KW_IMPLEMENTS:  return "KW_IMPLEMENTS";
        case TokenType::KW_INTERFACE:   return "KW_INTERFACE";
        case TokenType::KW_SUPER:       return "KW_SUPER";
        case TokenType::PLUS:           return "PLUS";
        case TokenType::MINUS:          return "MINUS";
        case TokenType::STAR:           return "STAR";
        case TokenType::SLASH:          return "SLASH";
        case TokenType::DOUBLE_SLASH:   return "DOUBLE_SLASH";
        case TokenType::PERCENT:        return "PERCENT";
        case TokenType::DOUBLE_STAR:    return "DOUBLE_STAR";
        case TokenType::ASSIGN:         return "ASSIGN";
        case TokenType::EQ:             return "EQ";
        case TokenType::NEQ:            return "NEQ";
        case TokenType::LT:             return "LT";
        case TokenType::GT:             return "GT";
        case TokenType::LTE:            return "LTE";
        case TokenType::GTE:            return "GTE";
        case TokenType::ARROW:          return "ARROW";
        case TokenType::DOT:            return "DOT";
        case TokenType::PLUS_EQ:        return "PLUS_EQ";
        case TokenType::MINUS_EQ:       return "MINUS_EQ";
        case TokenType::STAR_EQ:        return "STAR_EQ";
        case TokenType::SLASH_EQ:       return "SLASH_EQ";
        case TokenType::DOUBLE_SLASH_EQ:return "DOUBLE_SLASH_EQ";
        case TokenType::PERCENT_EQ:     return "PERCENT_EQ";
        case TokenType::DOUBLE_STAR_EQ: return "DOUBLE_STAR_EQ";
        case TokenType::AMP_EQ:         return "AMP_EQ";
        case TokenType::PIPE_EQ:        return "PIPE_EQ";
        case TokenType::CARET_EQ:       return "CARET_EQ";
        case TokenType::LSHIFT_EQ:      return "LSHIFT_EQ";
        case TokenType::RSHIFT_EQ:      return "RSHIFT_EQ";
        case TokenType::AMP:            return "AMP";
        case TokenType::PIPE:           return "PIPE";
        case TokenType::CARET:          return "CARET";
        case TokenType::TILDE:          return "TILDE";
        case TokenType::LSHIFT:         return "LSHIFT";
        case TokenType::RSHIFT:         return "RSHIFT";
        case TokenType::LPAREN:         return "LPAREN";
        case TokenType::RPAREN:         return "RPAREN";
        case TokenType::LBRACKET:       return "LBRACKET";
        case TokenType::RBRACKET:       return "RBRACKET";
        case TokenType::LBRACE:         return "LBRACE";
        case TokenType::RBRACE:         return "RBRACE";
        case TokenType::COMMA:          return "COMMA";
        case TokenType::COLON:          return "COLON";
        case TokenType::ELLIPSIS:       return "ELLIPSIS";
        case TokenType::AT:             return "AT";
        case TokenType::QUESTION:       return "QUESTION";
        case TokenType::NEWLINE:        return "NEWLINE";
        case TokenType::INDENT:         return "INDENT";
        case TokenType::DEDENT:         return "DEDENT";
        case TokenType::END_OF_FILE:    return "END_OF_FILE";
    }
    return "UNKNOWN";
}

// Backward-compatible alias used by the parser.
inline const char* tokenTypeName(TokenType t) {
    return tokenTypeToString(t);
}

// Human-readable token output for --tokens and debug printing.
inline std::string Token::toString() const {
    return std::string(tokenTypeToString(type)) + " '" + lexeme + "' " +
           std::to_string(line) + ":" + std::to_string(column);
}

inline std::ostream& operator<<(std::ostream& os, const Token& tok) {
    os << tok.toString();
    return os;
}

} // namespace visuall
