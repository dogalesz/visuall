#include "lsp_server.h"
#include <token.h>

namespace lsp {

json LspServer::handleRename(const json& params) {
    std::string uri = params["textDocument"]["uri"];
    int line = params["position"]["line"];
    int col  = params["position"]["character"];
    std::string newName = params["newName"];

    json result;
    result["changes"] = json::object();

    if (!store_.has(uri)) return nullptr;

    const Document& doc = store_.get(uri);
    const visuall::Token* tok = findTokenAt(doc, line, col);

    if (!tok || tok->type != visuall::TokenType::IDENTIFIER) {
        return nullptr;
    }

    std::string oldName = tok->lexeme;
    json edits = json::array();

    // 1. Find all references to the name in the current document's tokens
    // and replace them.
    for (const auto& t : doc.tokens) {
        if (t.type == visuall::TokenType::IDENTIFIER && t.lexeme == oldName) {
            int startLine = t.line - 1;
            int startCol  = t.column - 1;
            int endCol    = startCol + static_cast<int>(t.lexeme.size());

            edits.push_back({
                {"range", {
                    {"start", {{"line", startLine}, {"character", startCol}}},
                    {"end",   {{"line", startLine}, {"character", endCol}}}
                }},
                {"newText", newName}
            });
        }
    }

    result["changes"][uri] = edits;

    // 2. Search workspace index for the same symbol in other files.
    auto defs = workspaceIndex_.findDefinitions(oldName);
    for (const auto* sym : defs) {
        // Find the file URI for this symbol.
        for (const auto& [fileUri, entry] : workspaceIndex_.files()) {
            bool found = false;
            for (const auto& s : entry.symbols) {
                if (&s == sym) { found = true; break; }
                for (const auto& child : s.children) {
                    if (&child == sym) { found = true; break; }
                }
                if (found) break;
            }
            if (!found) continue;
            if (fileUri == uri) continue; // already handled above

            // For other files, add a single edit for the symbol definition.
            // Full token-stream replacement requires opening the file.
            json otherEdits = json::array();
            otherEdits.push_back({
                {"range", {
                    {"start", {{"line", sym->defLine}, {"character", sym->defCol}}},
                    {"end",   {{"line", sym->defLine}, {"character", sym->defCol + static_cast<int>(sym->name.size())}}}
                }},
                {"newText", newName}
            });
            result["changes"][fileUri] = otherEdits;
            break;
        }
    }

    return result;
}

} // namespace lsp
