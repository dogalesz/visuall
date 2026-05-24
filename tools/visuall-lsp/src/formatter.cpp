#include "formatter.h"
#include <sstream>
#include <vector>
#include <algorithm>

namespace lsp {

std::string Formatter::format(const std::string& source) const {
    // Split into lines.
    std::vector<std::string> lines;
    std::istringstream stream(source);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }

    // Phase 1: per-line normalization.
    for (auto& l : lines) {
        l = stripTrailingWhitespace(l);
        l = normalizeIndentation(l);

        // Only normalize operators/commas on non-comment, non-blank lines.
        if (!isComment(l) && !isBlank(l)) {
            l = normalizeOperators(l);
            l = normalizeCommas(l);
        }
    }

    // Phase 2: blank line normalization between top-level definitions.
    std::vector<std::string> result;
    for (size_t i = 0; i < lines.size(); ++i) {
        // Remove blank lines at very start of file.
        if (result.empty() && isBlank(lines[i])) {
            continue;
        }

        // Before class definitions: ensure two blank lines.
        if (isClassDef(lines[i]) && !result.empty()) {
            // Remove existing trailing blanks.
            while (!result.empty() && isBlank(result.back())) {
                result.pop_back();
            }
            result.push_back("");
            result.push_back("");
        }
        // Before other top-level definitions: ensure one blank line.
        else if (isTopLevelDef(lines[i]) && !result.empty() && !isBlank(result.back())) {
            // Check if previous non-blank line is also a top-level thing.
            // Don't add blank line after a comment that precedes this def.
            if (!isComment(result.back())) {
                result.push_back("");
            }
        }

        result.push_back(lines[i]);
    }

    // Remove blank lines at end of file.
    while (!result.empty() && isBlank(result.back())) {
        result.pop_back();
    }

    // Phase 3: remove blank lines at start/end of indented blocks.
    // A block starts after a line ending with ":" and is indented.
    std::vector<std::string> cleaned;
    for (size_t i = 0; i < result.size(); ++i) {
        // Skip blank lines immediately after a block opener (line ending with :).
        if (isBlank(result[i]) && i > 0) {
            std::string prev = stripTrailingWhitespace(result[i - 1]);
            if (!prev.empty() && prev.back() == ':') {
                continue; // Skip blank line at start of block.
            }
        }

        // Skip blank lines immediately before a dedent (less indented non-blank line).
        if (isBlank(result[i]) && i + 1 < result.size() && !isBlank(result[i + 1])) {
            // Check if next line is less indented than previous non-blank.
            // Simple heuristic: skip double blanks inside blocks.
            if (i > 0 && !cleaned.empty() && isBlank(cleaned.back())) {
                continue; // Collapse multiple blank lines to one.
            }
        }

        cleaned.push_back(result[i]);
    }

    // Rebuild source.
    std::string output;
    for (size_t i = 0; i < cleaned.size(); ++i) {
        output += cleaned[i];
        if (i + 1 < cleaned.size()) {
            output += '\n';
        }
    }

    // Ensure file ends with a newline.
    if (!output.empty() && output.back() != '\n') {
        output += '\n';
    }

    return output;
}

std::string Formatter::normalizeIndentation(const std::string& line) {
    if (line.empty()) return line;

    // Count leading spaces and tabs.
    size_t i = 0;
    int spaces = 0;
    int tabs = 0;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
        if (line[i] == ' ') ++spaces;
        else ++tabs;
        ++i;
    }

    if (i == line.size()) return ""; // All whitespace → blank line.

    // Convert spaces to tabs (assuming 4 spaces = 1 tab).
    int totalTabs = tabs + (spaces / 4);
    int remainingSpaces = spaces % 4;
    // If there are remaining spaces, round up to a tab.
    if (remainingSpaces > 0) {
        totalTabs += 1;
    }

    return std::string(totalTabs, '\t') + line.substr(i);
}

std::string Formatter::stripTrailingWhitespace(const std::string& line) {
    size_t end = line.find_last_not_of(" \t\r");
    if (end == std::string::npos) return "";
    return line.substr(0, end + 1);
}

std::string Formatter::normalizeOperators(const std::string& line) {
    std::string result;
    bool inString = false;
    char stringDelim = 0;
    size_t i = 0;

    // Find the start of indentation.
    while (i < line.size() && (line[i] == '\t' || line[i] == ' ')) {
        result += line[i];
        ++i;
    }

    while (i < line.size()) {
        char c = line[i];

        // Track string state.
        if (!inString && (c == '"' || c == '\'')) {
            inString = true;
            stringDelim = c;
            result += c;
            ++i;
            continue;
        }
        if (inString) {
            if (c == '\\' && i + 1 < line.size()) {
                result += c;
                result += line[i + 1];
                i += 2;
                continue;
            }
            if (c == stringDelim) {
                inString = false;
            }
            result += c;
            ++i;
            continue;
        }

        // Skip comments.
        if (c == '#' && i + 1 < line.size() && line[i + 1] == '#') {
            result += line.substr(i);
            break;
        }

        // Binary operators: +, -, *, /, %, but not unary or in-compound.
        // Handle: ==, !=, <=, >=, ->, //, **, +=, -=, etc.
        if ((c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
             c == '=' || c == '!' || c == '<' || c == '>' || c == '|' ||
             c == '&' || c == '^') && !inString) {

            // Check for compound operators (2-char).
            std::string twoChar;
            if (i + 1 < line.size()) {
                twoChar = std::string(1, c) + line[i + 1];
            }

            // Skip arrow -> (special syntax).
            if (twoChar == "->") {
                // Ensure spaces around ->.
                if (!result.empty() && result.back() != ' ') result += ' ';
                result += "->";
                i += 2;
                if (i < line.size() && line[i] != ' ') result += ' ';
                continue;
            }

            // Compound operators: ==, !=, <=, >=, **, //, +=, -=, etc.
            bool isCompound = (twoChar == "==" || twoChar == "!=" ||
                               twoChar == "<=" || twoChar == ">=" ||
                               twoChar == "**" || twoChar == "//" ||
                               twoChar == "+=" || twoChar == "-=" ||
                               twoChar == "*=" || twoChar == "/=" ||
                               twoChar == "%=" || twoChar == "&=" ||
                               twoChar == "|=" || twoChar == "^=" ||
                               twoChar == "<<" || twoChar == ">>");

            if (isCompound) {
                if (!result.empty() && result.back() != ' ') result += ' ';
                result += twoChar;
                i += 2;
                if (i < line.size() && line[i] != ' ' && line[i] != '\n') result += ' ';
                continue;
            }

            // Single character = (assignment).
            if (c == '=') {
                if (!result.empty() && result.back() != ' ') result += ' ';
                result += '=';
                ++i;
                if (i < line.size() && line[i] != ' ' && line[i] != '=') result += ' ';
                continue;
            }

            // For +, -, *, /, %: add spaces around them if they're binary.
            // Simple heuristic: if preceded by an alphanumeric/closing paren/bracket, treat as binary.
            bool isBinary = false;
            if (!result.empty()) {
                char prev = result.back();
                isBinary = (std::isalnum(prev) || prev == ')' || prev == ']' ||
                            prev == '\'' || prev == '"' || prev == '_');
            }

            if (isBinary) {
                if (result.back() != ' ') result += ' ';
                result += c;
                ++i;
                if (i < line.size() && line[i] != ' ') result += ' ';
            } else {
                result += c;
                ++i;
            }
            continue;
        }

        result += c;
        ++i;
    }

    return result;
}

std::string Formatter::normalizeCommas(const std::string& line) {
    std::string result;
    bool inString = false;
    char stringDelim = 0;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];

        // Track string state.
        if (!inString && (c == '"' || c == '\'')) {
            inString = true;
            stringDelim = c;
            result += c;
            continue;
        }
        if (inString) {
            if (c == '\\' && i + 1 < line.size()) {
                result += c;
                result += line[i + 1];
                ++i;
                continue;
            }
            if (c == stringDelim) {
                inString = false;
            }
            result += c;
            continue;
        }

        // Comment — preserve rest of line.
        if (c == '#' && i + 1 < line.size() && line[i + 1] == '#') {
            result += line.substr(i);
            return result;
        }

        if (c == ',') {
            // Remove preceding spaces before comma.
            while (!result.empty() && result.back() == ' ') {
                result.pop_back();
            }
            result += ',';
            // Ensure exactly one space after comma (if not at end of line).
            if (i + 1 < line.size() && line[i + 1] != ' ' && line[i + 1] != '\n') {
                result += ' ';
            } else if (i + 1 < line.size() && line[i + 1] == ' ') {
                result += ' ';
                // Skip extra spaces after comma.
                while (i + 1 < line.size() && line[i + 1] == ' ') {
                    ++i;
                }
            }
            continue;
        }

        result += c;
    }

    return result;
}

bool Formatter::isTopLevelDef(const std::string& line) {
    // A top-level definition starts at column 0 (no indentation).
    if (line.empty() || line[0] == '\t' || line[0] == ' ') return false;

    return line.find("define ") == 0 ||
           line.find("class ")  == 0 ||
           line.find("interface ") == 0;
}

bool Formatter::isClassDef(const std::string& line) {
    if (line.empty() || line[0] == '\t' || line[0] == ' ') return false;
    return line.find("class ") == 0;
}

bool Formatter::isComment(const std::string& line) {
    size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos) return false;
    return line.size() > start + 1 && line[start] == '#' && line[start + 1] == '#';
}

bool Formatter::isBlank(const std::string& line) {
    return line.find_first_not_of(" \t\r") == std::string::npos;
}

} // namespace lsp
