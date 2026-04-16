#pragma once

#include "token.h"
#include <string>
#include <vector>
#include <stack>
#include <stdexcept>

namespace visuall {

// ════════════════════════════════════════════════════════════════════════════
// LexError — thrown on all lexer-time errors.
// Format: "LexError: [message] at [filename]:[line]:[col]"
// ════════════════════════════════════════════════════════════════════════════
class LexError : public std::runtime_error {
public:
    std::string filename;
    int line;
    int column;

    LexError(const std::string& msg, const std::string& file, int ln, int col)
        : std::runtime_error("LexError: " + msg + " at " +
                             file + ":" + std::to_string(ln) + ":" +
                             std::to_string(col)),
          filename(file), line(ln), column(col) {}
};

// ════════════════════════════════════════════════════════════════════════════
// IndentationError — thrown when spaces are used for indentation.
// ════════════════════════════════════════════════════════════════════════════
class IndentationError : public std::runtime_error {
public:
    std::string filename;
    int line;
    int column;

    IndentationError(const std::string& msg, const std::string& file, int ln, int col)
        : std::runtime_error("IndentationError: " + msg + " at " +
                             file + ":" + std::to_string(ln) + ":" +
                             std::to_string(col)),
          filename(file), line(ln), column(col) {}
};

// Backward-compatible alias so existing catch(LexerError) sites still compile.
using LexerError = LexError;

// ════════════════════════════════════════════════════════════════════════════
// Lexer — single-pass tokeniser for the Visuall language.
//
// Usage:
//   Lexer lex(source, "file.vsl");
//   std::vector<Token> tokens = lex.tokenize();
// ════════════════════════════════════════════════════════════════════════════
class Lexer {
public:
    // Construct a lexer over the given source text, with filename for errors.
    Lexer(const std::string& source, const std::string& filename);

    // Run the lexer and return the complete, flat token stream.
    // The stream always ends with an END_OF_FILE token.
    std::vector<Token> tokenize();

private:
    // ── Source state ───────────────────────────────────────────────────
    std::string source_;        // full source text
    std::string filename_;      // used in error messages
    size_t      pos_;           // current byte offset into source_
    int         line_;          // current line (1-based)
    int         column_;        // current column (1-based)
    bool        atLineStart_;   // true when we haven't consumed content yet

    // ── Indent tracking ────────────────────────────────────────────────
    std::stack<int> indentStack_;   // monotonic stack of tab-depths

    // ── Character-level helpers ────────────────────────────────────────

    // Return the character at the current position, or '\0' at end.
    char peek() const;

    // Return the character one position ahead, or '\0' at end.
    char peekNext() const;

    // Consume and return the current character, updating line/column.
    char advance();

    // True when pos_ >= source_.size().
    bool isAtEnd() const;

    // If the current character matches `expected`, consume it and
    // return true. Otherwise leave the cursor unchanged.
    bool match(char expected);

    // ── Whitespace ─────────────────────────────────────────────────────

    // Skip spaces and tabs that appear mid-line (NOT at line start,
    // because leading whitespace is significant for indentation).
    void skipWhitespace();

    // ── Indentation ────────────────────────────────────────────────────

    // Called at the start of every new line.  Counts leading tabs,
    // rejects spaces, and emits INDENT / DEDENT tokens as needed.
    void handleIndentation(std::vector<Token>& tokens);

    // ── Comments ───────────────────────────────────────────────────────

    // Consume a single-line ## comment up to (but not including) \n.
    void skipSingleLineComment();

    // Consume a multi-line ### … ### comment (nestable).
    // Throws LexError on unterminated comment.
    void skipMultiLineComment();

    // ── Literal lexers ─────────────────────────────────────────────────

    // Lex a single- or double-quoted string literal.
    // Handles escape sequences: \n \t \\ \' \"
    // Throws LexError on unterminated string.
    Token lexString(char delimiter);

    // Lex an f-string (f"..." or f'...').
    // The 'f' and opening quote have already been consumed.
    // Preserves {expr} blocks in the raw lexeme.
    // Throws LexError on unterminated f-string or unclosed brace.
    Token lexFString(char delimiter);

    // Lex an integer or floating-point literal.
    // Rejects malformed numbers like 3.14.15.
    Token lexNumber();

    // ── Identifier / keyword lexer ─────────────────────────────────────

    // Lex an alphanumeric+underscore sequence, then check the keyword
    // table.  Returns the appropriate keyword TokenType or IDENTIFIER.
    Token lexIdentifierOrKeyword();

    // ── Operator / delimiter lexer ─────────────────────────────────────

    // Lex all single- and multi-character operators and delimiters.
    // Handles: ** before *, // before /, -> vs -, ... as ELLIPSIS, etc.
    Token lexOperator(char c, int tokLine, int tokCol);

    // ── Token factory ──────────────────────────────────────────────────

    // Convenience: build a Token with the given fields.
    Token makeToken(TokenType type, const std::string& lexeme,
                    int line, int col) const;

    // ── Error reporting ────────────────────────────────────────────────

    // Throw a LexError at the current position.
    [[noreturn]] void error(const std::string& msg) const;

    // Throw a LexError at a specific position.
    [[noreturn]] void errorAt(const std::string& msg, int ln, int col) const;
};

} // namespace visuall
