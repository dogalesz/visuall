#include "lsp_server.h"
#include <token.h>
#include <ast.h>
#include <vector>
#include <cstdint>

namespace lsp {

// LSP semantic token type indices (order must match the legend).
enum SemanticTokenType : uint32_t {
    SEM_KEYWORD   = 0,
    SEM_FUNCTION  = 1,
    SEM_CLASS     = 2,
    SEM_PARAMETER = 3,
    SEM_VARIABLE  = 4,
    SEM_STRING    = 5,
    SEM_NUMBER    = 6,
    SEM_COMMENT   = 7,
    SEM_TYPE      = 8,
};

// LSP semantic token modifier flags (bitmask, order must match legend).
enum SemanticTokenMod : uint32_t {
    MOD_DECLARATION = 1 << 0,
    MOD_DEFAULT_LIB = 1 << 1,
    MOD_READONLY    = 1 << 2,
};

// Map Visuall token types to semantic token types.
static uint32_t classifyToken(visuall::TokenType tt) {
    switch (tt) {
        // Keywords
        case visuall::TokenType::KW_DEFINE:
        case visuall::TokenType::KW_CLASS:
        case visuall::TokenType::KW_INIT:
        case visuall::TokenType::KW_IF:
        case visuall::TokenType::KW_ELSIF:
        case visuall::TokenType::KW_ELSE:
        case visuall::TokenType::KW_FOR:
        case visuall::TokenType::KW_WHILE:
        case visuall::TokenType::KW_IN:
        case visuall::TokenType::KW_RETURN:
        case visuall::TokenType::KW_BREAK:
        case visuall::TokenType::KW_CONTINUE:
        case visuall::TokenType::KW_PASS:
        case visuall::TokenType::KW_AND:
        case visuall::TokenType::KW_OR:
        case visuall::TokenType::KW_NOT:
        case visuall::TokenType::KW_TRUE:
        case visuall::TokenType::KW_FALSE:
        case visuall::TokenType::KW_NULL:
        case visuall::TokenType::KW_IMPORT:
        case visuall::TokenType::KW_FROM:
        case visuall::TokenType::KW_TRY:
        case visuall::TokenType::KW_CATCH:
        case visuall::TokenType::KW_FINALLY:
        case visuall::TokenType::KW_THROW:
        case visuall::TokenType::KW_THIS:
        case visuall::TokenType::KW_EXTENDS:
        case visuall::TokenType::KW_IMPLEMENTS:
        case visuall::TokenType::KW_INTERFACE:
        case visuall::TokenType::KW_SUPER:
        case visuall::TokenType::KW_YIELD:
        case visuall::TokenType::KW_GO:
        case visuall::TokenType::KW_CHAN:
        case visuall::TokenType::KW_SEND:
        case visuall::TokenType::KW_MATCH:
        case visuall::TokenType::KW_CASE:
        case visuall::TokenType::KW_ENUM:
        case visuall::TokenType::KW_DEL:
        case visuall::TokenType::KW_WITH:
        case visuall::TokenType::KW_ASSERT:
        case visuall::TokenType::KW_LAMBDA:
        case visuall::TokenType::KW_AS:
            return SEM_KEYWORD;

        // Strings
        case visuall::TokenType::STRING_LITERAL:
        case visuall::TokenType::FSTRING_LITERAL:
            return SEM_STRING;

        // Numbers
        case visuall::TokenType::INT_LITERAL:
        case visuall::TokenType::FLOAT_LITERAL:
            return SEM_NUMBER;

        // Boolean and null literals
        case visuall::TokenType::BOOL_LITERAL:
        case visuall::TokenType::NULL_LITERAL:
            return SEM_KEYWORD;

        default:
            return UINT32_MAX; // no semantic highlighting
    }
}

json LspServer::handleSemanticTokens(const json& params) {
    std::string uri = params["textDocument"]["uri"];

    json result;
    result["data"] = json::array();

    if (!store_.has(uri)) return result;

    const Document& doc = store_.get(uri);
    if (doc.tokens.empty()) return result;

    // Scan tokens and build delta-encoded semantic token array.
    // Format: [deltaLine, deltaStart, length, tokenType, tokenModifiers]
    std::vector<uint32_t> data;
    int prevLine = 0;
    int prevCol  = 0;

    for (size_t i = 0; i < doc.tokens.size(); ++i) {
        const auto& tok = doc.tokens[i];

        // Skip whitespace, newlines, indentation, and synthetic tokens.
        if (tok.type == visuall::TokenType::NEWLINE ||
            tok.type == visuall::TokenType::INDENT ||
            tok.type == visuall::TokenType::DEDENT ||
            tok.type == visuall::TokenType::END_OF_FILE ||
            tok.lexeme.empty()) {
            continue;
        }

        uint32_t semType = classifyToken(tok.type);
        if (semType == UINT32_MAX) {
            // Check AST context for identifiers: determine if function, class, parameter, or variable.
            if (tok.type == visuall::TokenType::IDENTIFIER && doc.ast) {
                // Walk symbols to classify this identifier.
                const auto& syms = doc.symbols;
                for (const auto& s : syms) {
                    int sLine = s.defLine + 1; // back to 1-based
                    int sCol  = s.defCol + 1;
                    if (sLine == tok.line && sCol == tok.column && s.name == tok.lexeme) {
                        switch (s.symbolKind) {
                            case 12: semType = SEM_FUNCTION; break; // Function
                            case 5:  semType = SEM_CLASS;    break; // Class
                            case 6:  semType = SEM_FUNCTION; break; // Method
                            case 9:  semType = SEM_FUNCTION; break; // Constructor
                            case 13: semType = SEM_VARIABLE; break; // Variable
                            case 8:  semType = SEM_VARIABLE; break; // Field
                            default: semType = SEM_VARIABLE; break;
                        }
                        break;
                    }
                }
                // If still not classified, check if it's a parameter.
                if (semType == UINT32_MAX) {
                    // Heuristic: identifier followed by ':' in a parameter list is a parameter.
                    if (i + 1 < doc.tokens.size() &&
                        doc.tokens[i + 1].type == visuall::TokenType::COLON) {
                        semType = SEM_PARAMETER;
                    } else {
                        semType = SEM_VARIABLE;
                    }
                }
            } else {
                continue; // not a token we highlight
            }
        }

        // LSP positions are 0-based; tokens use 1-based.
        int lspLine = tok.line - 1;
        int lspCol  = tok.column - 1;
        int length  = static_cast<int>(tok.lexeme.size());

        int deltaLine = lspLine - prevLine;
        int deltaCol  = (deltaLine == 0) ? (lspCol - prevCol) : lspCol;

        uint32_t modifiers = 0;
        // Mark keywords as defaultLibrary.
        if (semType == SEM_KEYWORD) modifiers |= MOD_DEFAULT_LIB;

        data.push_back(static_cast<uint32_t>(deltaLine));
        data.push_back(static_cast<uint32_t>(deltaCol));
        data.push_back(static_cast<uint32_t>(length));
        data.push_back(semType);
        data.push_back(modifiers);

        prevLine = lspLine;
        prevCol  = lspCol;
    }

    result["data"] = data;
    return result;
}

} // namespace lsp
