#include "lsp_server.h"
#include <iostream>

namespace lsp {

void LspServer::handleDidOpen(const json& params) {
    auto textDoc = params["textDocument"];
    std::string uri     = textDoc["uri"];
    std::string content = textDoc["text"];
    int version         = textDoc.value("version", 0);

    store_.open(uri, content, version);
    store_.reanalyze(uri);
    workspaceIndex_.indexFile(uri, content);
    publishDiagnostics(uri);

    std::cerr << "[visuall-lsp] Opened: " << uri << std::endl;
}

} // namespace lsp
