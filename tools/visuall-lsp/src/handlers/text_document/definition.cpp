#include "lsp_server.h"

namespace lsp {

// Helper to build a Location JSON from a symbol and URI.
static json makeLocation(const std::string& uri, const SymbolInfo& sym) {
    int endCol = sym.defCol + std::max(1, static_cast<int>(sym.name.size()));
    return json{
        {"uri", uri},
        {"range", {
            {"start", {{"line", sym.defLine}, {"character", sym.defCol}}},
            {"end",   {{"line", sym.defLine}, {"character", endCol}}}
        }}
    };
}

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

    const std::string& name = tok->lexeme;

    // 1. Search current document.
    const SymbolInfo* sym = findSymbolByName(doc, name);
    if (sym) return makeLocation(uri, *sym);

    // 2. Search other open documents.
    for (const auto& [docUri, otherDoc] : store_.documents()) {
        if (docUri == uri) continue;
        sym = findSymbolByName(otherDoc, name);
        if (sym) return makeLocation(docUri, *sym);
    }

    // 3. Search workspace index (cross-file, non-open docs).
    auto defs = workspaceIndex_.findDefinitions(name);
    if (!defs.empty()) {
        // Find the file URI: look up the symbol's containing file.
        for (const auto& [fileUri, entry] : workspaceIndex_.files()) {
            for (const auto& s : entry.symbols) {
                if (&s == defs[0]) return makeLocation(fileUri, s);
                for (const auto& child : s.children) {
                    if (&child == defs[0]) return makeLocation(fileUri, child);
                }
            }
        }
        // Fallback: return first definition.
        return makeLocation("file:///unknown", *defs[0]);
    }

    return nullptr;
}

} // namespace lsp
