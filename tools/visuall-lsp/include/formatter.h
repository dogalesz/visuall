#pragma once

#include <string>

namespace lsp {

// ════════════════════════════════════════════════════════════════════════════
// Formatter — formats Visuall source code.
//
// Rules:
//   - Normalize all indentation to tabs
//   - Remove trailing whitespace from every line
//   - Ensure single blank line between top-level definitions
//   - Ensure two blank lines before class definitions
//   - Normalize operator spacing: a+b → a + b
//   - Normalize comma spacing: f(a,b) → f(a, b)
//   - Remove blank lines at start/end of blocks
//   - Preserve ## comments exactly
// ════════════════════════════════════════════════════════════════════════════
class Formatter {
public:
    std::string format(const std::string& source) const;

private:
    // Normalize indentation: convert leading spaces to tabs.
    static std::string normalizeIndentation(const std::string& line);

    // Remove trailing whitespace.
    static std::string stripTrailingWhitespace(const std::string& line);

    // Normalize operator spacing in a line (outside strings and comments).
    static std::string normalizeOperators(const std::string& line);

    // Normalize comma spacing in a line (outside strings and comments).
    static std::string normalizeCommas(const std::string& line);

    // Check if a line starts a top-level definition.
    static bool isTopLevelDef(const std::string& line);

    // Check if a line starts a class definition.
    static bool isClassDef(const std::string& line);

    // Check if a line is a comment (## ...).
    static bool isComment(const std::string& line);

    // Check if a line is blank (empty or only whitespace).
    static bool isBlank(const std::string& line);
};

} // namespace lsp
