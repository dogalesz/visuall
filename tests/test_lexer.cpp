// ════════════════════════════════════════════════════════════════════════════
// tests/test_lexer.cpp — Comprehensive test suite for the Visuall lexer.
//
// 17 test cases covering every aspect of the specification:
//
//   1. Basic keywords are lexed correctly
//   2. Integer and float literals
//   3. Single and double quoted strings
//   4. F-strings with expressions inside
//   5. Single-line ## comments are stripped
//   6. Multi-line ### comments are stripped
//   7. INDENT emitted on tab increase
//   8. DEDENT emitted on tab decrease
//   9. Multiple DEDENTs emitted on large dedent
//  10. Space indentation throws IndentationError
//  11. Unterminated string throws LexError
//  12. Unterminated ### comment throws LexError
//  13. Chained comparison tokens: 18 <= age <= 65
//  14. Arrow token -> is distinct from - and >
//  15. Ellipsis ... is a single token
//  16. Blank lines do not emit INDENT/DEDENT/NEWLINE
//  17. EOF flushes remaining DEDENTs
// ════════════════════════════════════════════════════════════════════════════

#include "lexer.h"
#include <iostream>
#include <string>
#include <vector>

using namespace visuall;

static int failures = 0;

// Helper: assert a condition and print PASS / FAIL with the test name.
static void expect(bool condition, const std::string& testName) {
    if (!condition) {
        std::cerr << "  FAIL: " << testName << "\n";
        failures++;
    } else {
        std::cout << "  PASS: " << testName << "\n";
    }
}

// ── 1. Basic keywords are lexed correctly ──────────────────────────────────
static void testKeywords() {
    Lexer lexer("define class init return if elsif else for while "
                "break continue import from try catch finally "
                "throw in not and or this pass\n", "test.vsl");
    auto tokens = lexer.tokenize();

    expect(tokens[0].type  == TokenType::KW_DEFINE,   "kw: define");
    expect(tokens[1].type  == TokenType::KW_CLASS,    "kw: class");
    expect(tokens[2].type  == TokenType::KW_INIT,     "kw: init");
    expect(tokens[3].type  == TokenType::KW_RETURN,   "kw: return");
    expect(tokens[4].type  == TokenType::KW_IF,       "kw: if");
    expect(tokens[5].type  == TokenType::KW_ELSIF,    "kw: elsif");
    expect(tokens[6].type  == TokenType::KW_ELSE,     "kw: else");
    expect(tokens[7].type  == TokenType::KW_FOR,      "kw: for");
    expect(tokens[8].type  == TokenType::KW_WHILE,    "kw: while");
    expect(tokens[9].type  == TokenType::KW_BREAK,    "kw: break");
    expect(tokens[10].type == TokenType::KW_CONTINUE, "kw: continue");
    expect(tokens[11].type == TokenType::KW_IMPORT,   "kw: import");
    expect(tokens[12].type == TokenType::KW_FROM,     "kw: from");
    expect(tokens[13].type == TokenType::KW_TRY,      "kw: try");
    expect(tokens[14].type == TokenType::KW_CATCH,    "kw: catch");
    expect(tokens[15].type == TokenType::KW_FINALLY,  "kw: finally");
    expect(tokens[16].type == TokenType::KW_THROW,    "kw: throw");
    expect(tokens[17].type == TokenType::KW_IN,       "kw: in");
    expect(tokens[18].type == TokenType::KW_NOT,      "kw: not");
    expect(tokens[19].type == TokenType::KW_AND,      "kw: and");
    expect(tokens[20].type == TokenType::KW_OR,       "kw: or");
    expect(tokens[21].type == TokenType::KW_THIS,     "kw: this");
    expect(tokens[22].type == TokenType::KW_PASS,     "kw: pass");

    // true / false / null map to literal types, not keyword types.
    Lexer lexer2("true false null\n", "test.vsl");
    auto tokens2 = lexer2.tokenize();
    expect(tokens2[0].type == TokenType::BOOL_LITERAL, "kw: true → BOOL_LITERAL");
    expect(tokens2[0].lexeme == "true",                "kw: true lexeme");
    expect(tokens2[1].type == TokenType::BOOL_LITERAL, "kw: false → BOOL_LITERAL");
    expect(tokens2[2].type == TokenType::NULL_LITERAL, "kw: null → NULL_LITERAL");
}

// ── 2. Integer and float literals ──────────────────────────────────────────
static void testNumberLiterals() {
    Lexer lexer("42 1_000 3.14 1e10 2.5e-3\n", "test.vsl");
    auto tokens = lexer.tokenize();

    expect(tokens[0].type == TokenType::INT_LITERAL,   "num: 42 is INT");
    expect(tokens[0].lexeme == "42",                   "num: 42 lexeme");
    expect(tokens[1].type == TokenType::INT_LITERAL,   "num: 1_000 is INT");
    expect(tokens[1].lexeme == "1000",                 "num: underscores stripped");
    expect(tokens[2].type == TokenType::FLOAT_LITERAL, "num: 3.14 is FLOAT");
    expect(tokens[2].lexeme == "3.14",                 "num: 3.14 lexeme");
    expect(tokens[3].type == TokenType::FLOAT_LITERAL, "num: 1e10 is FLOAT");
    expect(tokens[4].type == TokenType::FLOAT_LITERAL, "num: 2.5e-3 is FLOAT");
}

// ── 3. Single and double quoted strings ────────────────────────────────────
static void testStrings() {
    Lexer lexer("\"hello\" 'world' \"esc\\n\\t\"\n", "test.vsl");
    auto tokens = lexer.tokenize();

    expect(tokens[0].type == TokenType::STRING_LITERAL, "str: double-quoted type");
    expect(tokens[0].lexeme == "hello",                 "str: double-quoted value");
    expect(tokens[1].type == TokenType::STRING_LITERAL, "str: single-quoted type");
    expect(tokens[1].lexeme == "world",                 "str: single-quoted value");
    expect(tokens[2].type == TokenType::STRING_LITERAL, "str: with escapes type");
    expect(tokens[2].lexeme == "esc\n\t",               "str: \\n and \\t expanded");
}

// ── 4. F-strings with expressions inside ───────────────────────────────────
static void testFStrings() {
    Lexer lexer("f\"hello {name}\" f'value is {x + 1}'\n", "test.vsl");
    auto tokens = lexer.tokenize();

    expect(tokens[0].type == TokenType::FSTRING_LITERAL, "fstr: double-quoted type");
    expect(tokens[0].lexeme.find("{name}") != std::string::npos,
           "fstr: contains {name}");
    expect(tokens[1].type == TokenType::FSTRING_LITERAL, "fstr: single-quoted type");
    expect(tokens[1].lexeme.find("{x + 1}") != std::string::npos,
           "fstr: contains {x + 1}");
}

// ── 5. Single-line ## comments are stripped ────────────────────────────────
static void testSingleLineComment() {
    Lexer lexer("x = 1 ## this is a comment\ny = 2\n", "test.vsl");
    auto tokens = lexer.tokenize();

    // No token should contain the word "comment".
    bool hasComment = false;
    for (const auto& t : tokens) {
        if (t.lexeme.find("comment") != std::string::npos) hasComment = true;
    }
    expect(!hasComment, "comment ##: stripped from tokens");

    // We should still get x, =, 1, NEWLINE, y, =, 2, …
    expect(tokens[0].type == TokenType::IDENTIFIER,  "comment ##: x present");
    expect(tokens[2].type == TokenType::INT_LITERAL,  "comment ##: 1 present");
}

// ── 6. Multi-line ### comments are stripped ─────────────────────────────────
static void testMultiLineComment() {
    Lexer lexer("x = 1\n### multi\nline\ncomment ###\ny = 2\n", "test.vsl");
    auto tokens = lexer.tokenize();

    bool hasMulti = false;
    for (const auto& t : tokens) {
        if (t.lexeme.find("multi") != std::string::npos) hasMulti = true;
    }
    expect(!hasMulti, "comment ###: multi-line stripped");
}

// ── 7. INDENT emitted on tab increase ──────────────────────────────────────
static void testIndentEmitted() {
    Lexer lexer("if true:\n\tx = 1\n", "test.vsl");
    auto tokens = lexer.tokenize();

    bool hasIndent = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::INDENT) { hasIndent = true; break; }
    }
    expect(hasIndent, "indent: INDENT emitted on tab increase");
}

// ── 8. DEDENT emitted on tab decrease ──────────────────────────────────────
static void testDedentEmitted() {
    Lexer lexer("if true:\n\tx = 1\ny = 2\n", "test.vsl");
    auto tokens = lexer.tokenize();

    bool hasDedent = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::DEDENT) { hasDedent = true; break; }
    }
    expect(hasDedent, "dedent: DEDENT emitted on tab decrease");
}

// ── 9. Multiple DEDENTs emitted on large dedent ────────────────────────────
static void testMultipleDedents() {
    // Two levels of indent, then back to zero — should get 2 DEDENTs.
    Lexer lexer("a:\n\tb:\n\t\tc = 1\nd = 2\n", "test.vsl");
    auto tokens = lexer.tokenize();

    int dedentCount = 0;
    for (const auto& t : tokens) {
        if (t.type == TokenType::DEDENT) dedentCount++;
    }
    expect(dedentCount == 2, "multi-dedent: 2 DEDENTs emitted");
}

// ── 10. Space indentation throws IndentationError ──────────────────────────
static void testSpaceIndentationError() {
    Lexer lexer("if true:\n    x = 1\n", "test.vsl");
    bool caught = false;
    try {
        lexer.tokenize();
    } catch (const IndentationError& e) {
        caught = true;
        std::string msg = e.what();
        expect(msg.find("tabs") != std::string::npos ||
               msg.find("Visuall uses tabs") != std::string::npos,
               "indent-err: message mentions tabs");
    } catch (...) {
        // Might be caught as LexerError alias.
        caught = true;
    }
    expect(caught, "indent-err: space indentation throws");
}

// ── 11. Unterminated string throws LexError ────────────────────────────────
static void testUnterminatedString() {
    Lexer lexer("x = \"hello\n", "test.vsl");
    bool caught = false;
    try {
        lexer.tokenize();
    } catch (const LexError& e) {
        caught = true;
        std::string msg = e.what();
        expect(msg.find("Unterminated string") != std::string::npos,
               "unterm-str: error message correct");
    }
    expect(caught, "unterm-str: throws LexError");
}

// ── 12. Unterminated ### comment throws LexError ───────────────────────────
static void testUnterminatedMultiLineComment() {
    Lexer lexer("### this never ends\nstill going\n", "test.vsl");
    bool caught = false;
    try {
        lexer.tokenize();
    } catch (const LexError& e) {
        caught = true;
        std::string msg = e.what();
        expect(msg.find("Unterminated multi-line comment") != std::string::npos,
               "unterm-comment: error message correct");
    }
    expect(caught, "unterm-comment: throws LexError");
}

// ── 13. Chained comparison tokens: 18 <= age <= 65 ─────────────────────────
static void testChainedComparison() {
    Lexer lexer("18 <= age <= 65\n", "test.vsl");
    auto tokens = lexer.tokenize();

    expect(tokens[0].type == TokenType::INT_LITERAL, "chain-cmp: 18");
    expect(tokens[1].type == TokenType::LTE,         "chain-cmp: first <=");
    expect(tokens[2].type == TokenType::IDENTIFIER,  "chain-cmp: age");
    expect(tokens[3].type == TokenType::LTE,         "chain-cmp: second <=");
    expect(tokens[4].type == TokenType::INT_LITERAL, "chain-cmp: 65");
    expect(tokens[4].lexeme == "65",                 "chain-cmp: 65 lexeme");
}

// ── 14. Arrow token -> is distinct from - and > ────────────────────────────
static void testArrowToken() {
    Lexer lexer("a -> b - c > d\n", "test.vsl");
    auto tokens = lexer.tokenize();

    expect(tokens[0].type == TokenType::IDENTIFIER, "arrow: a");
    expect(tokens[1].type == TokenType::ARROW,      "arrow: -> is ARROW");
    expect(tokens[1].lexeme == "->",                "arrow: lexeme is ->");
    expect(tokens[2].type == TokenType::IDENTIFIER, "arrow: b");
    expect(tokens[3].type == TokenType::MINUS,      "arrow: standalone - is MINUS");
    expect(tokens[4].type == TokenType::IDENTIFIER, "arrow: c");
    expect(tokens[5].type == TokenType::GT,         "arrow: standalone > is GT");
    expect(tokens[6].type == TokenType::IDENTIFIER, "arrow: d");
}

// ── 15. Ellipsis ... is a single token ─────────────────────────────────────
static void testEllipsis() {
    Lexer lexer("...\n", "test.vsl");
    auto tokens = lexer.tokenize();

    expect(tokens[0].type == TokenType::ELLIPSIS, "ellipsis: type");
    expect(tokens[0].lexeme == "...",             "ellipsis: lexeme");
}

// ── 16. Blank lines do not emit INDENT/DEDENT/NEWLINE ──────────────────────
static void testBlankLinesIgnored() {
    // Blank line between two statements at column 0.
    Lexer lexer("x = 1\n\n\ny = 2\n", "test.vsl");
    auto tokens = lexer.tokenize();

    int indentCount = 0;
    int dedentCount = 0;
    for (const auto& t : tokens) {
        if (t.type == TokenType::INDENT) indentCount++;
        if (t.type == TokenType::DEDENT) dedentCount++;
    }
    expect(indentCount == 0, "blank-lines: no spurious INDENT");
    expect(dedentCount == 0, "blank-lines: no spurious DEDENT");

    // Count consecutive NEWLINEs — should only ever be 1 between statements.
    int maxConsecutiveNewlines = 0;
    int consecutive = 0;
    for (const auto& t : tokens) {
        if (t.type == TokenType::NEWLINE) {
            consecutive++;
            if (consecutive > maxConsecutiveNewlines)
                maxConsecutiveNewlines = consecutive;
        } else {
            consecutive = 0;
        }
    }
    expect(maxConsecutiveNewlines <= 1, "blank-lines: no consecutive NEWLINEs");
}

// ── 17. EOF flushes remaining DEDENTs ──────────────────────────────────────
static void testEofFlushesDedents() {
    // Source ends while still indented — no trailing newline.
    Lexer lexer("if true:\n\tx = 1", "test.vsl");
    auto tokens = lexer.tokenize();

    // Last token must be END_OF_FILE.
    expect(tokens.back().type == TokenType::END_OF_FILE,
           "eof-flush: last token is EOF");

    // There must be at least one DEDENT before EOF.
    bool hasDedent = false;
    for (size_t i = 0; i < tokens.size() - 1; i++) {
        if (tokens[i].type == TokenType::DEDENT) { hasDedent = true; break; }
    }
    expect(hasDedent, "eof-flush: DEDENT emitted before EOF");
}

// ════════════════════════════════════════════════════════════════════════════
// Test-suite entry point (called from tests/test_main.cpp).
// ════════════════════════════════════════════════════════════════════════════
int runLexerTests() {
    failures = 0;

    testKeywords();                     //  1
    testNumberLiterals();               //  2
    testStrings();                      //  3
    testFStrings();                     //  4
    testSingleLineComment();            //  5
    testMultiLineComment();             //  6
    testIndentEmitted();                //  7
    testDedentEmitted();                //  8
    testMultipleDedents();              //  9
    testSpaceIndentationError();        // 10
    testUnterminatedString();           // 11
    testUnterminatedMultiLineComment(); // 12
    testChainedComparison();            // 13
    testArrowToken();                   // 14
    testEllipsis();                     // 15
    testBlankLinesIgnored();            // 16
    testEofFlushesDedents();            // 17

    return failures;
}
