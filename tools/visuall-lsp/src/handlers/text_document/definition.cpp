#include "lsp_server.h"

namespace lsp {

json LspServer::handleDefinition(const json& params) {
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

    // Search for the symbol definition in this document.
    const SymbolInfo* sym = findSymbolByName(doc, tok->lexeme);
    if (!sym) {
        // Search across all open documents.
        for (const auto& [docUri, otherDoc] : store_.documents()) {
            if (docUri == uri) continue;
            sym = findSymbolByName(otherDoc, tok->lexeme);
            if (sym) {
                return json{
                    {"uri", docUri},
                    {"range", {
                        {"start", {{"line", sym->defLine}, {"character", sym->defCol}}},
                        {"end",   {{"line", sym->defLine}, {"character", sym->defCol + static_cast<int>(sym->name.size())}}}
                    }}
                };
            }
        }
        return nullptr;
    }

    return json{
        {"uri", uri},
        {"range", {
            {"start", {{"line", sym->defLine}, {"character", sym->defCol}}},
            {"end",   {{"line", sym->defLine}, {"character", sym->defCol + static_cast<int>(sym->name.size())}}}
        }}
    };
}

} // namespace lsp
