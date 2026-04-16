// ════════════════════════════════════════════════════════════════════════════
// src/lexer.cpp — Single-pass lexer for the Visuall language.
//
// Responsibilities
// ────────────────
//   • Translate raw source text into a flat std::vector<Token>.
//   • Emit synthetic INDENT / DEDENT tokens based on tab depth.
//   • Reject spaces in indentation with IndentationError.
//   • Strip ## single-line and ### … ### multi-line (nestable) comments.
//   • Lex all Visuall literals, keywords, operators, and delimiters.
//   • Produce clear, file:line:col error messages on malformed input.
//
// Design notes
//   • One-character lookahead maximum (peek / peekNext).
//   • No regex — everything is hand-lexed.
//   • Compiles cleanly with -Wall -Wextra -std=c++17.
// ════════════════════════════════════════════════════════════════════════════

#include "lexer.h"
#include <cctype>
#include <unordered_map>

namespace visuall {

// ── Keyword table ──────────────────────────────────────────────────────────
// Maps every reserved word to its TokenType.  true/false/null are in the
// table so they are recognised as keywords; readIdentifierOrKeyword()
// remaps them to BOOL_LITERAL / NULL_LITERAL before returning.
static const std::unordered_map<std::string, TokenType> keywords = {
    {"define",   TokenType::KW_DEFINE},
    {"init",     TokenType::KW_INIT},
    {"class",    TokenType::KW_CLASS},
    {"if",       TokenType::KW_IF},
    {"elsif",    TokenType::KW_ELSIF},
    {"else",     TokenType::KW_ELSE},
    {"for",      TokenType::KW_FOR},
    {"while",    TokenType::KW_WHILE},
    {"in",       TokenType::KW_IN},
    {"return",   TokenType::KW_RETURN},
    {"break",    TokenType::KW_BREAK},
    {"continue", TokenType::KW_CONTINUE},
    {"pass",     TokenType::KW_PASS},
    {"and",      TokenType::KW_AND},
    {"or",       TokenType::KW_OR},
    {"not",      TokenType::KW_NOT},
    {"true",     TokenType::KW_TRUE},
    {"false",    TokenType::KW_FALSE},
    {"null",     TokenType::KW_NULL},
    {"import",   TokenType::KW_IMPORT},
    {"from",     TokenType::KW_FROM},
    {"try",      TokenType::KW_TRY},
    {"catch",    TokenType::KW_CATCH},
    {"finally",  TokenType::KW_FINALLY},
    {"throw",    TokenType::KW_THROW},
    {"this",     TokenType::KW_THIS},
    {"extends",    TokenType::KW_EXTENDS},
    {"implements", TokenType::KW_IMPLEMENTS},
    {"interface",  TokenType::KW_INTERFACE},
    {"super",      TokenType::KW_SUPER},
};

// ════════════════════════════════════════════════════════════════════════════
// Constructor
// ════════════════════════════════════════════════════════════════════════════
Lexer::Lexer(const std::string& source, const std::string& filename)
    : source_(source), filename_(filename), pos_(0), line_(1), column_(1),
      atLineStart_(true)
{
    indentStack_.push(0);   // base indent level is always zero
}

// ════════════════════════════════════════════════════════════════════════════
// Character-level helpers
// ════════════════════════════════════════════════════════════════════════════

// Return the character at the current position, or '\0' at end.
char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return source_[pos_];
}

// Return the character one position ahead, or '\0' at end.
char Lexer::peekNext() const {
    if (pos_ + 1 >= source_.size()) return '\0';
    return source_[pos_ + 1];
}

// Consume and return the current character, updating line/column tracking.
char Lexer::advance() {
    char c = source_[pos_++];
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

// True when the cursor has passed the last byte of source.
bool Lexer::isAtEnd() const {
    return pos_ >= source_.size();
}

// If the current character matches `expected`, consume it and return true.
bool Lexer::match(char expected) {
    if (isAtEnd() || source_[pos_] != expected) return false;
    advance();
    return true;
}

// Build a Token struct with the provided fields.
Token Lexer::makeToken(TokenType type, const std::string& lexeme,
                       int line, int col) const {
    return Token{type, lexeme, line, col};
}

// ════════════════════════════════════════════════════════════════════════════
// Error helpers
// ════════════════════════════════════════════════════════════════════════════

// Throw a LexError at the current cursor position.
[[noreturn]] void Lexer::error(const std::string& msg) const {
    throw LexError(msg, filename_, line_, column_);
}

// Throw a LexError at a specific line/column.
[[noreturn]] void Lexer::errorAt(const std::string& msg, int ln, int col) const {
    throw LexError(msg, filename_, ln, col);
}

// ════════════════════════════════════════════════════════════════════════════
// skipWhitespace — skip spaces and tabs that appear WITHIN a line.
//
// This is never called at line start (indentation is handled separately
// by handleIndentation).  Only horizontal whitespace is consumed; \n is
// left for the main loop.
// ════════════════════════════════════════════════════════════════════════════
void Lexer::skipWhitespace() {
    while (!isAtEnd() && (peek() == ' ' || peek() == '\t' || peek() == '\r')) {
        advance();
    }
}

// ════════════════════════════════════════════════════════════════════════════
// handleIndentation — called at the start of every logical line.
//
// Algorithm:
//   1. Count leading tab characters.
//   2. If a space is encountered in the leading whitespace, throw an
//      IndentationError — Visuall mandates tabs exclusively.
//   3. Compare the tab count to the top of indentStack_:
//        • Greater  → push new level, emit INDENT.
//        • Equal    → do nothing.
//        • Less     → pop levels, emit one DEDENT per pop.  If the stack
//                     never matches the new level, throw LexError
//                     (inconsistent indentation).
//   4. Blank lines (only whitespace + newline) and comment-only lines
//      do NOT produce INDENT/DEDENT.
// ════════════════════════════════════════════════════════════════════════════
void Lexer::handleIndentation(std::vector<Token>& tokens) {
    int tabCount = 0;
    int indentStartLine = line_;
    int indentStartCol  = column_;

    // Count leading tabs; reject spaces immediately.
    while (!isAtEnd() && (peek() == '\t' || peek() == ' ')) {
        if (peek() == ' ') {
            throw IndentationError(
                "Visuall uses tabs for indentation, not spaces",
                filename_, line_, column_);
        }
        // It's a tab.
        tabCount++;
        advance();
    }

    // ── Skip blank lines: whitespace followed by newline or EOF ────────
    if (isAtEnd() || peek() == '\n') {
        return;  // blank line — no INDENT/DEDENT changes
    }

    // ── Skip comment-only lines ────────────────────────────────────────
    if (peek() == '#') {
        return;  // comment-only line — indent logic handled next real line
    }

    // ── Compare to current indent level and emit tokens ────────────────
    int currentIndent = indentStack_.top();

    if (tabCount > currentIndent) {
        indentStack_.push(tabCount);
        tokens.push_back(makeToken(TokenType::INDENT, "<INDENT>",
                                   indentStartLine, indentStartCol));
    } else if (tabCount < currentIndent) {
        while (tabCount < indentStack_.top()) {
            indentStack_.pop();
            tokens.push_back(makeToken(TokenType::DEDENT, "<DEDENT>",
                                       indentStartLine, indentStartCol));
        }
        if (tabCount != indentStack_.top()) {
            errorAt("inconsistent indentation level",
                    indentStartLine, indentStartCol);
        }
    }
    // tabCount == currentIndent → do nothing
}

// ════════════════════════════════════════════════════════════════════════════
// skipSingleLineComment — consume all characters until EOL.
//
// The leading ## has already been consumed by the caller.
// The trailing \n is NOT consumed (the main loop handles it).
// ════════════════════════════════════════════════════════════════════════════
void Lexer::skipSingleLineComment() {
    while (!isAtEnd() && peek() != '\n') {
        advance();
    }
}

// ════════════════════════════════════════════════════════════════════════════
// skipMultiLineComment — consume a ### … ### block comment.
//
// The opening ### has already been consumed.  We scan for the matching
// closing ###.  Throws LexError on EOF before the close.
// ════════════════════════════════════════════════════════════════════════════
void Lexer::skipMultiLineComment() {
    int startLine = line_;
    int startCol  = column_ - 3;  // opening ### already consumed

    while (!isAtEnd()) {
        // Check for closing ###.
        if (peek() == '#' && peekNext() == '#' &&
            pos_ + 2 < source_.size() && source_[pos_ + 2] == '#') {
            advance(); advance(); advance();
            return;
        }
        advance();
    }

    throw LexError("Unterminated multi-line comment",
                   filename_, startLine, startCol);
}

// ════════════════════════════════════════════════════════════════════════════
// lexString — lex a single- or double-quoted string literal.
//
// The opening delimiter has already been consumed.  Handles the escape
// sequences \n  \t  \\  \'  \"  \r  \0.  Unknown escapes pass through
// literally (backslash + character).
// Throws LexError if the string is not terminated before EOL or EOF.
// ════════════════════════════════════════════════════════════════════════════
Token Lexer::lexString(char delimiter) {
    int startLine = line_;
    int startCol  = column_ - 1;  // opening quote was already consumed
    std::string value;

    while (!isAtEnd() && peek() != delimiter) {
        if (peek() == '\n') {
            errorAt("Unterminated string literal", startLine, startCol);
        }
        if (peek() == '\\') {
            advance();  // consume backslash
            if (isAtEnd()) {
                errorAt("Unterminated string literal", startLine, startCol);
            }
            char esc = advance();
            switch (esc) {
                case 'n':  value += '\n'; break;
                case 't':  value += '\t'; break;
                case 'r':  value += '\r'; break;
                case '\\': value += '\\'; break;
                case '\'': value += '\''; break;
                case '"':  value += '"';  break;
                case '0':  value += '\0'; break;
                default:
                    // Unknown escape — preserve literally.
                    value += '\\';
                    value += esc;
                    break;
            }
        } else {
            value += advance();
        }
    }

    if (isAtEnd()) {
        errorAt("Unterminated string literal", startLine, startCol);
    }
    advance();  // consume closing delimiter

    return makeToken(TokenType::STRING_LITERAL, value, startLine, startCol);
}

// ════════════════════════════════════════════════════════════════════════════
// lexFString — lex an f-string literal (f"…" or f'…').
//
// Both the 'f' prefix and the opening quote have been consumed already.
// The raw content (including {expr} blocks) is stored verbatim in the
// lexeme as:  f<quote><content><quote>   — downstream phases parse the
// interpolation expressions out of the raw text.
//
// Throws LexError on:
//   • Unterminated f-string (no closing delimiter before EOL/EOF).
//   • Unclosed brace ('{' without matching '}' before the delimiter).
// ════════════════════════════════════════════════════════════════════════════
Token Lexer::lexFString(char delimiter) {
    int startLine = line_;
    int startCol  = column_ - 2;  // 'f' + quote already consumed

    // Reconstruct the raw lexeme: f<quote>...<quote>
    std::string raw;
    raw += 'f';
    raw += delimiter;

    while (!isAtEnd() && peek() != delimiter) {
        if (peek() == '\n') {
            errorAt("Unterminated f-string literal", startLine, startCol);
        }
        if (peek() == '\\') {
            raw += advance();  // backslash
            if (isAtEnd()) {
                errorAt("Unterminated f-string literal", startLine, startCol);
            }
            raw += advance();  // escaped character
        } else if (peek() == '{') {
            raw += advance();  // consume '{'
            // Read until matching '}'.  No nesting of braces supported.
            int braceStartLine = line_;
            int braceStartCol  = column_ - 1;
            while (!isAtEnd() && peek() != '}') {
                if (peek() == '\n' || peek() == delimiter) {
                    errorAt("Unclosed '{' in f-string", braceStartLine, braceStartCol);
                }
                raw += advance();
            }
            if (isAtEnd()) {
                errorAt("Unclosed '{' in f-string", braceStartLine, braceStartCol);
            }
            raw += advance();  // consume '}'
        } else {
            raw += advance();
        }
    }

    if (isAtEnd()) {
        errorAt("Unterminated f-string literal", startLine, startCol);
    }
    raw += advance();  // closing delimiter

    return makeToken(TokenType::FSTRING_LITERAL, raw, startLine, startCol);
}

// ════════════════════════════════════════════════════════════════════════════
// lexNumber — lex an integer or floating-point literal.
//
// The first digit has already been consumed.
//
// Handles:
//   • Digit separators: 1_000_000  (underscores stripped from lexeme)
//   • Decimal floats:   3.14
//   • Scientific:       1e10, 2.5e-3
//
// Rejects malformed numbers like 3.14.15 with a LexError.
// ════════════════════════════════════════════════════════════════════════════
Token Lexer::lexNumber() {
    int startLine = line_;
    int startCol  = column_ - 1;  // first digit already consumed
    std::string value;
    value += source_[pos_ - 1];
    bool isFloat = false;

    // Integer part (remaining digits + underscores).
    while (!isAtEnd() && (std::isdigit(static_cast<unsigned char>(peek())) || peek() == '_')) {
        if (peek() == '_') { advance(); continue; }  // skip separator
        value += advance();
    }

    // Decimal part: only if '.' is followed by a digit (so "x.method" works).
    if (!isAtEnd() && peek() == '.' && std::isdigit(static_cast<unsigned char>(peekNext()))) {
        isFloat = true;
        value += advance();  // '.'
        while (!isAtEnd() && (std::isdigit(static_cast<unsigned char>(peek())) || peek() == '_')) {
            if (peek() == '_') { advance(); continue; }
            value += advance();
        }
        // Reject double-dot floats like 3.14.15.
        if (!isAtEnd() && peek() == '.') {
            errorAt("Malformed numeric literal", startLine, startCol);
        }
    }

    // Scientific notation: e / E optionally followed by +/-.
    if (!isAtEnd() && (peek() == 'e' || peek() == 'E')) {
        isFloat = true;
        value += advance();  // 'e'
        if (!isAtEnd() && (peek() == '+' || peek() == '-')) {
            value += advance();
        }
        if (isAtEnd() || !std::isdigit(static_cast<unsigned char>(peek()))) {
            errorAt("Invalid numeric literal: expected digit after exponent",
                    startLine, startCol);
        }
        while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
            value += advance();
        }
    }

    return makeToken(isFloat ? TokenType::FLOAT_LITERAL : TokenType::INT_LITERAL,
                     value, startLine, startCol);
}

// ════════════════════════════════════════════════════════════════════════════
// lexIdentifierOrKeyword — lex a word and check the keyword table.
//
// The first character (alpha or '_') has already been consumed.
// After collecting [a-zA-Z0-9_]*, the word is looked up in `keywords`.
//
// Special-case mappings:
//   true / false  →  BOOL_LITERAL
//   null          →  NULL_LITERAL
// Everything else that matches a keyword → its KW_* type.
// Non-keywords → IDENTIFIER.
// ════════════════════════════════════════════════════════════════════════════
Token Lexer::lexIdentifierOrKeyword() {
    int startLine = line_;
    int startCol  = column_ - 1;
    std::string value;
    value += source_[pos_ - 1];

    while (!isAtEnd() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) {
        value += advance();
    }

    auto it = keywords.find(value);
    if (it != keywords.end()) {
        TokenType kwType = it->second;
        // true / false → BOOL_LITERAL
        if (kwType == TokenType::KW_TRUE || kwType == TokenType::KW_FALSE) {
            return makeToken(TokenType::BOOL_LITERAL, value, startLine, startCol);
        }
        // null → NULL_LITERAL
        if (kwType == TokenType::KW_NULL) {
            return makeToken(TokenType::NULL_LITERAL, value, startLine, startCol);
        }
        return makeToken(kwType, value, startLine, startCol);
    }

    return makeToken(TokenType::IDENTIFIER, value, startLine, startCol);
}

// ════════════════════════════════════════════════════════════════════════════
// lexOperator — lex a single- or multi-character operator / delimiter.
//
// The first character `c` has already been consumed.  We use one-char
// lookahead (peek / peekNext) to disambiguate multi-character tokens:
//
//   **  before  *           //  before  /
//   ->  before  -  and >    ...  as single ELLIPSIS
//   ==  vs  =               !=
//   <=  vs  <               >=  vs  >
// ════════════════════════════════════════════════════════════════════════════
Token Lexer::lexOperator(char c, int tokLine, int tokCol) {
    switch (c) {
        case '+':
            if (match('='))
                return makeToken(TokenType::PLUS_EQ, "+=", tokLine, tokCol);
            return makeToken(TokenType::PLUS, "+", tokLine, tokCol);

        case '-':
            if (match('>'))
                return makeToken(TokenType::ARROW, "->", tokLine, tokCol);
            if (match('='))
                return makeToken(TokenType::MINUS_EQ, "-=", tokLine, tokCol);
            return makeToken(TokenType::MINUS, "-", tokLine, tokCol);

        case '*':
            if (match('*')) {
                if (match('='))
                    return makeToken(TokenType::DOUBLE_STAR_EQ, "**=", tokLine, tokCol);
                return makeToken(TokenType::DOUBLE_STAR, "**", tokLine, tokCol);
            }
            if (match('='))
                return makeToken(TokenType::STAR_EQ, "*=", tokLine, tokCol);
            return makeToken(TokenType::STAR, "*", tokLine, tokCol);

        case '/':
            if (match('/')) {
                if (match('='))
                    return makeToken(TokenType::DOUBLE_SLASH_EQ, "//=", tokLine, tokCol);
                return makeToken(TokenType::DOUBLE_SLASH, "//", tokLine, tokCol);
            }
            if (match('='))
                return makeToken(TokenType::SLASH_EQ, "/=", tokLine, tokCol);
            return makeToken(TokenType::SLASH, "/", tokLine, tokCol);

        case '%':
            if (match('='))
                return makeToken(TokenType::PERCENT_EQ, "%=", tokLine, tokCol);
            return makeToken(TokenType::PERCENT, "%", tokLine, tokCol);

        case '=':
            if (match('='))
                return makeToken(TokenType::EQ, "==", tokLine, tokCol);
            return makeToken(TokenType::ASSIGN, "=", tokLine, tokCol);

        case '!':
            if (match('='))
                return makeToken(TokenType::NEQ, "!=", tokLine, tokCol);
            throw LexError(std::string("Unexpected character '!'"),
                           filename_, tokLine, tokCol);

        case '<':
            if (match('<')) {
                if (match('='))
                    return makeToken(TokenType::LSHIFT_EQ, "<<=", tokLine, tokCol);
                return makeToken(TokenType::LSHIFT, "<<", tokLine, tokCol);
            }
            if (match('='))
                return makeToken(TokenType::LTE, "<=", tokLine, tokCol);
            return makeToken(TokenType::LT, "<", tokLine, tokCol);

        case '>':
            if (match('>')) {
                if (match('='))
                    return makeToken(TokenType::RSHIFT_EQ, ">>=", tokLine, tokCol);
                return makeToken(TokenType::RSHIFT, ">>", tokLine, tokCol);
            }
            if (match('='))
                return makeToken(TokenType::GTE, ">=", tokLine, tokCol);
            return makeToken(TokenType::GT, ">", tokLine, tokCol);

        case '(':  return makeToken(TokenType::LPAREN,   "(", tokLine, tokCol);
        case ')':  return makeToken(TokenType::RPAREN,   ")", tokLine, tokCol);
        case '[':  return makeToken(TokenType::LBRACKET, "[", tokLine, tokCol);
        case ']':  return makeToken(TokenType::RBRACKET, "]", tokLine, tokCol);
        case '{':  return makeToken(TokenType::LBRACE,   "{", tokLine, tokCol);
        case '}':  return makeToken(TokenType::RBRACE,   "}", tokLine, tokCol);
        case ',':  return makeToken(TokenType::COMMA,    ",", tokLine, tokCol);
        case ':':  return makeToken(TokenType::COLON,    ":", tokLine, tokCol);
        case '~':  return makeToken(TokenType::TILDE,    "~", tokLine, tokCol);

        case '&':
            if (match('='))
                return makeToken(TokenType::AMP_EQ, "&=", tokLine, tokCol);
            return makeToken(TokenType::AMP, "&", tokLine, tokCol);

        case '|':
            if (match('='))
                return makeToken(TokenType::PIPE_EQ, "|=", tokLine, tokCol);
            return makeToken(TokenType::PIPE, "|", tokLine, tokCol);

        case '^':
            if (match('='))
                return makeToken(TokenType::CARET_EQ, "^=", tokLine, tokCol);
            return makeToken(TokenType::CARET, "^", tokLine, tokCol);

        case '.':
            if (!isAtEnd() && peek() == '.' && peekNext() == '.') {
                advance(); advance();  // consume ".."
                return makeToken(TokenType::ELLIPSIS, "...", tokLine, tokCol);
            }
            return makeToken(TokenType::DOT, ".", tokLine, tokCol);

        case '@':
            return makeToken(TokenType::AT, "@", tokLine, tokCol);

        case '?':
            return makeToken(TokenType::QUESTION, "?", tokLine, tokCol);

        default:
            throw LexError(
                std::string("Unexpected character '") + c + "'",
                filename_, tokLine, tokCol);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// tokenize — the main lexer loop.
//
// Processes entire source_ in a single pass and returns a flat token
// vector.  The stream always ends with END_OF_FILE.
//
// Top-level control flow:
//   1. At line start → handleIndentation() for INDENT/DEDENT.
//   2. Whitespace (mid-line) → skip.
//   3. Newline → emit NEWLINE (unless redundant), set atLineStart_.
//   4. Comments → skip.
//   5. f-strings → lexFString.
//   6. Strings → lexString.
//   7. Numbers → lexNumber.
//   8. Identifiers/keywords → lexIdentifierOrKeyword.
//   9. Everything else → lexOperator.
//  10. At EOF → flush remaining DEDENTs, emit END_OF_FILE.
// ════════════════════════════════════════════════════════════════════════════
std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (!isAtEnd()) {
        // ── 1. Line-start: handle indentation ─────────────────────────
        if (atLineStart_) {
            atLineStart_ = false;
            handleIndentation(tokens);
            if (isAtEnd()) break;
            // If after eating indentation we're on a blank line, loop back.
            if (peek() == '\n') continue;
        }

        char c = peek();

        // ── 2. Mid-line whitespace ────────────────────────────────────
        if (c == ' ' || c == '\t' || c == '\r') {
            skipWhitespace();
            continue;
        }

        // ── 3. Newlines ───────────────────────────────────────────────
        if (c == '\n') {
            int nl = line_;
            int nc = column_;
            advance();

            // Only emit NEWLINE if the previous meaningful token isn't
            // already a NEWLINE, INDENT, or DEDENT (avoids duplicates
            // on blank/comment lines).
            if (!tokens.empty()) {
                TokenType last = tokens.back().type;
                if (last != TokenType::NEWLINE &&
                    last != TokenType::INDENT &&
                    last != TokenType::DEDENT) {
                    tokens.push_back(makeToken(TokenType::NEWLINE, "\\n", nl, nc));
                }
            }
            atLineStart_ = true;
            continue;
        }

        int tokLine = line_;
        int tokCol  = column_;

        // ── 4. Comments ───────────────────────────────────────────────
        if (c == '#') {
            if (peekNext() == '#') {
                advance(); advance();  // consume first two '#'
                if (!isAtEnd() && peek() == '#') {
                    advance();  // consume third '#'
                    skipMultiLineComment();
                } else {
                    skipSingleLineComment();
                }
                continue;
            }
            // A lone '#' — treat as single-line comment for robustness.
            advance();
            skipSingleLineComment();
            continue;
        }

        // ── 5. F-strings ─────────────────────────────────────────────
        if (c == 'f' && (peekNext() == '"' || peekNext() == '\'')) {
            advance();            // consume 'f'
            char q = advance();   // consume opening quote
            tokens.push_back(lexFString(q));
            continue;
        }

        // ── 6. Strings ───────────────────────────────────────────────
        if (c == '"' || c == '\'') {
            advance();  // consume opening quote
            tokens.push_back(lexString(c));
            continue;
        }

        // ── 7. Numbers ───────────────────────────────────────────────
        if (std::isdigit(static_cast<unsigned char>(c))) {
            advance();  // consume first digit
            tokens.push_back(lexNumber());
            continue;
        }

        // ── 8. Identifiers / keywords ─────────────────────────────────
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            advance();  // consume first character
            tokens.push_back(lexIdentifierOrKeyword());
            continue;
        }

        // ── 9. Operators and delimiters ───────────────────────────────
        advance();  // consume first character of the operator
        tokens.push_back(lexOperator(c, tokLine, tokCol));
    }

    // ── 10. EOF: flush remaining DEDENTs ──────────────────────────────
    while (indentStack_.top() > 0) {
        indentStack_.pop();
        tokens.push_back(makeToken(TokenType::DEDENT, "<DEDENT>", line_, column_));
    }

    tokens.push_back(makeToken(TokenType::END_OF_FILE, "", line_, column_));
    return tokens;
}

} // namespace visuall
