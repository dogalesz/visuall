#include "lsp_server.h"

namespace lsp {

json LspServer::handleHover(const json& params) {
    std::string uri = params["textDocument"]["uri"];
    int line = params["position"]["line"];
    int col  = params["position"]["character"];

    if (!store_.has(uri)) {
        return nullptr;
    }

    const Document& doc = store_.get(uri);
    const visuall::Token* tok = findTokenAt(doc, line, col);

    if (!tok || tok->type != visuall::TokenType::IDENTIFIER) {
        return nullptr;
    }

    const SymbolInfo* sym = findSymbolByName(doc, tok->lexeme);
    if (!sym) {
        return nullptr;
    }

    // Build the hover content.
    std::string hoverText = "```visuall\n";

    switch (sym->symbolKind) {
        case 12: // Function
            hoverText += "define " + sym->name + sym->detail;
            break;
        case 5: // Class
            hoverText += sym->detail;
            break;
        case 11: // Interface
            hoverText += sym->detail;
            break;
        case 6: // Method
            hoverText += "define " + sym->name + sym->detail;
            break;
        case 9: // Constructor
            hoverText += "init" + sym->detail;
            break;
        case 8: // Field
            hoverText += sym->name;
            if (!sym->typeName.empty()) {
                hoverText += ": " + sym->typeName;
            }
            break;
        case 13: // Variable
            hoverText += sym->name;
            if (!sym->typeName.empty()) {
                hoverText += ": " + sym->typeName;
            }
            break;
        case 2: // Module
            hoverText += sym->detail;
            break;
        default:
            hoverText += sym->name;
            break;
    }

    hoverText += "\n```";

    if (!sym->documentation.empty()) {
        hoverText += "\n\n---\n\n" + sym->documentation;
    }

    json range = {
        {"start", {{"line", tok->line - 1}, {"character", tok->column - 1}}},
        {"end",   {{"line", tok->line - 1}, {"character", tok->column - 1 + static_cast<int>(tok->lexeme.size())}}}
    };

    return {
        {"contents", {
            {"kind",  "markdown"},
            {"value", hoverText}
        }},
        {"range", range}
    };
}

} // namespace lsp
