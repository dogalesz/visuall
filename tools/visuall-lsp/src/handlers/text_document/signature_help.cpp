#include "lsp_server.h"
#include <token.h>

namespace lsp {

json LspServer::handleSignatureHelp(const json& params) {
    std::string uri = params["textDocument"]["uri"];
    int line = params["position"]["line"];
    int col  = params["position"]["character"];

    json result;
    result["signatures"] = json::array();
    result["activeSignature"] = 0;
    result["activeParameter"] = -1;

    if (!store_.has(uri)) return result;

    const Document& doc = store_.get(uri);
    int tokenLine = line + 1;
    int tokenCol  = col + 1;

    // Find the token at or before cursor.
    const visuall::Token* cursorTok = nullptr;
    int cursorIdx = -1;
    for (int i = static_cast<int>(doc.tokens.size()) - 1; i >= 0; --i) {
        const auto& tok = doc.tokens[i];
        if (tok.line < tokenLine ||
            (tok.line == tokenLine && tok.column + static_cast<int>(tok.lexeme.size()) <= tokenCol)) {
            cursorTok = &tok;
            cursorIdx = i;
            break;
        }
    }
    if (!cursorTok) return result;

    // Walk left to find matching '(' and the callee name.
    int parenDepth = 0;
    int openParenIdx = -1;
    for (int i = cursorIdx; i >= 0; --i) {
        const auto& tok = doc.tokens[i];
        if (tok.type == visuall::TokenType::RPAREN) {
            parenDepth++;
        } else if (tok.type == visuall::TokenType::LPAREN) {
            if (parenDepth == 0) {
                openParenIdx = i;
                break;
            }
            parenDepth--;
        }
    }
    if (openParenIdx < 0) return result;

    // Find the identifier before the '(' that names the callee.
    std::string calleeName;
    for (int i = openParenIdx - 1; i >= 0; --i) {
        const auto& tok = doc.tokens[i];
        if (tok.type == visuall::TokenType::IDENTIFIER) {
            calleeName = tok.lexeme;
            break;
        }
        // Skip whitespace/newlines but stop at other tokens.
        if (tok.type != visuall::TokenType::NEWLINE &&
            tok.type != visuall::TokenType::DOT) {
            break;
        }
        // For member calls like obj.method( — get the method name.
        if (tok.type == visuall::TokenType::DOT && i > 0) {
            const auto& prev = doc.tokens[i - 1];
            if (prev.type == visuall::TokenType::IDENTIFIER) {
                calleeName = prev.lexeme;
            }
            break;
        }
    }
    if (calleeName.empty()) return result;

    // Count commas between '(' and cursor to find active parameter index.
    int paramIndex = 0;
    for (int i = openParenIdx + 1; i < cursorIdx; ++i) {
        if (doc.tokens[i].type == visuall::TokenType::COMMA) {
            paramIndex++;
        }
    }

    // Look up the callee's signature from symbols.
    for (const auto& sym : doc.symbols) {
        if (sym.name == calleeName && sym.symbolKind == 12) { // Function
            json sig;
            sig["label"] = sym.detail; // e.g., "(x: int, y: int) -> int"
            sig["activeParameter"] = paramIndex;

            // Parse parameters from detail string: "(name: type, ...)"
            json paramsArr = json::array();
            std::string detail = sym.detail;
            // Extract text between '(' and the closing ')'
            auto lp = detail.find('(');
            auto rp = detail.find(')');
            if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
                std::string paramStr = detail.substr(lp + 1, rp - lp - 1);
                size_t start = 0;
                while (start < paramStr.size()) {
                    auto comma = paramStr.find(',', start);
                    std::string param = paramStr.substr(start, comma - start);
                    // Trim leading/trailing whitespace.
                    auto first = param.find_first_not_of(" \t");
                    auto last  = param.find_last_not_of(" \t");
                    if (first != std::string::npos) {
                        param = param.substr(first, last - first + 1);
                        paramsArr.push_back({{"label", param}});
                    }
                    if (comma == std::string::npos) break;
                    start = comma + 1;
                }
            }
            if (!paramsArr.empty()) {
                sig["parameters"] = paramsArr;
            }
            result["signatures"].push_back(sig);

            if (paramIndex < static_cast<int>(paramsArr.size())) {
                result["activeParameter"] = paramIndex;
            }
            return result;
        }
        // Also check children (methods)
        for (const auto& child : sym.children) {
            if (child.name == calleeName) {
                json sig;
                sig["label"] = child.detail;
                sig["activeParameter"] = paramIndex;

                json paramsArr = json::array();
                std::string detail = child.detail;
                auto lp = detail.find('(');
                auto rp = detail.find(')');
                if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
                    std::string paramStr = detail.substr(lp + 1, rp - lp - 1);
                    size_t start = 0;
                    while (start < paramStr.size()) {
                        auto comma = paramStr.find(',', start);
                        std::string param = paramStr.substr(start, comma - start);
                        auto first = param.find_first_not_of(" \t");
                        auto last  = param.find_last_not_of(" \t");
                        if (first != std::string::npos) {
                            param = param.substr(first, last - first + 1);
                            paramsArr.push_back({{"label", param}});
                        }
                        if (comma == std::string::npos) break;
                        start = comma + 1;
                    }
                }
                if (!paramsArr.empty()) {
                    sig["parameters"] = paramsArr;
                }
                result["signatures"].push_back(sig);
                if (paramIndex < static_cast<int>(paramsArr.size())) {
                    result["activeParameter"] = paramIndex;
                }
                return result;
            }
        }
    }

    return result;
}

} // namespace lsp
