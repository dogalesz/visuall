#include "lsp_server.h"
#include <token.h>
#include <algorithm>

namespace lsp {

// Visuall keywords for completion.
static const std::vector<std::string> KEYWORDS = {
    "define", "class", "init", "if", "elsif", "else", "for", "while",
    "in", "return", "break", "continue", "pass", "and", "or", "not",
    "true", "false", "null", "import", "from", "try", "catch", "finally",
    "throw", "this", "extends", "implements", "interface", "super"
};

// Known stdlib modules.
static const std::vector<std::string> STDLIB_MODULES = {
    "math", "io", "string", "list", "dict", "os", "json", "http", "random"
};

json LspServer::handleCompletion(const json& params) {
    std::string uri = params["textDocument"]["uri"];
    int line = params["position"]["line"];
    int col  = params["position"]["character"];

    json items = json::array();

    if (!store_.has(uri)) {
        return items;
    }

    const Document& doc = store_.get(uri);

    // Determine completion context by looking at the token before cursor.
    const visuall::Token* tokenBefore = nullptr;
    int tokenLine = line + 1;  // tokens are 1-based
    int tokenCol  = col + 1;

    // Find the token immediately before or at the cursor.
    for (int i = static_cast<int>(doc.tokens.size()) - 1; i >= 0; --i) {
        const auto& tok = doc.tokens[i];
        if (tok.line < tokenLine ||
            (tok.line == tokenLine && tok.column + static_cast<int>(tok.lexeme.size()) <= tokenCol)) {
            tokenBefore = &tok;
            break;
        }
    }

    // Context A — after a dot: member completion.
    if (tokenBefore && tokenBefore->type == visuall::TokenType::DOT) {
        // Find the object token before the dot.
        const visuall::Token* objToken = nullptr;
        for (int i = static_cast<int>(doc.tokens.size()) - 1; i >= 0; --i) {
            const auto& tok = doc.tokens[i];
            if (&tok == tokenBefore) {
                if (i > 0) objToken = &doc.tokens[i - 1];
                break;
            }
        }

        if (objToken && objToken->type == visuall::TokenType::IDENTIFIER) {
            // Look up the type of the object.
            std::string objName = objToken->lexeme;

            // If it's "this", return current class members.
            // Otherwise, find the class with this name or the type of the variable.
            for (const auto& sym : doc.symbols) {
                if (sym.symbolKind == 5 /* Class */) {
                    // Check if this is the class of the object.
                    bool isMatch = (sym.name == objName);
                    // Also match if objName is "this" and we're inside this class body.
                    if (objToken->type == visuall::TokenType::KW_THIS || isMatch) {
                        for (const auto& child : sym.children) {
                            json item = {
                                {"label",  child.name},
                                {"detail", child.detail},
                                {"kind",   child.symbolKind == 6 ? 2 :  // Method → Method
                                           child.symbolKind == 8 ? 5 :  // Field → Field
                                           child.symbolKind == 9 ? 4 :  // Constructor → Constructor
                                           6}                           // Variable
                            };
                            items.push_back(item);
                        }
                    }
                }
            }
            return items;
        }
    }

    // Context B — after import/from keyword: module completion.
    if (tokenBefore &&
        (tokenBefore->type == visuall::TokenType::KW_IMPORT ||
         tokenBefore->type == visuall::TokenType::KW_FROM)) {
        for (const auto& mod : STDLIB_MODULES) {
            items.push_back({
                {"label", mod},
                {"kind",  9}, // Module
                {"detail", "stdlib module"}
            });
        }
        return items;
    }

    // Context C — general scope: return all visible symbols + keywords.
    auto visible = getVisibleSymbols(doc, line);
    for (const auto& sym : visible) {
        int kind = 6; // Variable
        switch (sym.symbolKind) {
            case 12: kind = 3;  break; // Function
            case  6: kind = 2;  break; // Method
            case  5: kind = 7;  break; // Class
            case 11: kind = 8;  break; // Interface
            case  2: kind = 9;  break; // Module
            case 13: kind = 6;  break; // Variable
            default: kind = 6;
        }

        json item = {
            {"label",  sym.name},
            {"kind",   kind},
            {"detail", sym.detail}
        };
        if (!sym.documentation.empty()) {
            item["documentation"] = {
                {"kind",  "markdown"},
                {"value", sym.documentation}
            };
        }
        items.push_back(item);
    }

    // Add keywords.
    for (const auto& kw : KEYWORDS) {
        items.push_back({
            {"label", kw},
            {"kind",  14}, // Keyword
            {"detail", "keyword"}
        });
    }

    return items;
}

} // namespace lsp
