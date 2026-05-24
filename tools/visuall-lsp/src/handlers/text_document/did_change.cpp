#include "lsp_server.h"
#include <iostream>

namespace lsp {

void LspServer::handleDidChange(const json& params) {
    auto textDoc = params["textDocument"];
    std::string uri = textDoc["uri"];
    int version     = textDoc.value("version", 0);

    // We use Full sync mode — the entire document content is in each change.
    auto& changes = params["contentChanges"];
    if (!changes.empty()) {
        std::string content = changes[0]["text"];
        store_.change(uri, content, version);
    }

    store_.reanalyze(uri);
    publishDiagnostics(uri);
}

} // namespace lsp
