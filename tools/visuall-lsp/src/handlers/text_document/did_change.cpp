#include "lsp_server.h"
#include <iostream>

namespace lsp {

void LspServer::handleDidChange(const json& params) {
    auto textDoc = params["textDocument"];
    std::string uri = textDoc["uri"];
    int version     = textDoc.value("version", 0);

    auto& changes = params["contentChanges"];
    if (changes.empty()) return;

    // Apply changes to the stored document content.
    if (!store_.has(uri)) return;
    Document& doc = store_.get(uri);
    doc.version = version;

    for (const auto& change : changes) {
        if (change.contains("range")) {
            // Incremental sync: apply range-based edit.
            int startLine = change["range"]["start"]["line"];
            int startCol  = change["range"]["start"]["character"];
            int endLine   = change["range"]["end"]["line"];
            int endCol    = change["range"]["end"]["character"];
            std::string newText = change.value("text", "");

            // Convert LSP 0-based positions to offsets in the content string.
            auto offsetOf = [&](int line, int col) -> size_t {
                size_t off = 0;
                int ln = 0;
                while (ln < line && off < doc.content.size()) {
                    if (doc.content[off] == '\n') ln++;
                    off++;
                }
                return off + col;
            };

            size_t startOff = offsetOf(startLine, startCol);
            size_t endOff   = offsetOf(endLine, endCol);
            if (startOff <= doc.content.size() && endOff <= doc.content.size()) {
                doc.content.replace(startOff, endOff - startOff, newText);
            }
        } else {
            // Full sync fallback: entire document content provided.
            doc.content = change["text"];
        }
    }

    store_.reanalyze(uri);
    workspaceIndex_.indexFile(uri, doc.content);
    publishDiagnostics(uri);
}

} // namespace lsp
