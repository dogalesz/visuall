#include "lsp_server.h"

namespace lsp {

json LspServer::handleReferences(const json& params) {
    std::string uri = params["textDocument"]["uri"];
    int line = params["position"]["line"];
    int col  = params["position"]["character"];

    json locations = json::array();

    if (!store_.has(uri)) {
        return locations;
    }

    const Document& doc = store_.get(uri);
    const visuall::Token* tok = findTokenAt(doc, line, col);

    if (!tok || tok->type != visuall::TokenType::IDENTIFIER) {
        return locations;
    }

    std::string name = tok->lexeme;

    // Find all references in the current document.
    auto refs = findAllReferences(doc, name);
    for (const auto* ref : refs) {
        locations.push_back({
            {"uri", uri},
            {"range", {
                {"start", {{"line", ref->line - 1}, {"character", ref->column - 1}}},
                {"end",   {{"line", ref->line - 1}, {"character", ref->column - 1 + static_cast<int>(ref->lexeme.size())}}}
            }}
        });
    }

    // Search across all other open documents.
    // NOTE: Full workspace indexing is v2 — only searching currently open documents.
    for (const auto& [docUri, otherDoc] : store_.documents()) {
        if (docUri == uri) continue;
        auto otherRefs = findAllReferences(otherDoc, name);
        for (const auto* ref : otherRefs) {
            locations.push_back({
                {"uri", docUri},
                {"range", {
                    {"start", {{"line", ref->line - 1}, {"character", ref->column - 1}}},
                    {"end",   {{"line", ref->line - 1}, {"character", ref->column - 1 + static_cast<int>(ref->lexeme.size())}}}
                }}
            });
        }
    }

    return locations;
}

} // namespace lsp
