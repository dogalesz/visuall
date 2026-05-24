#include "lsp_server.h"
#include <iostream>

namespace lsp {

void LspServer::handleDidClose(const json& params) {
    std::string uri = params["textDocument"]["uri"];

    store_.close(uri);

    // Send empty diagnostics to clear squiggles.
    rpc_.sendNotification("textDocument/publishDiagnostics", {
        {"uri",         uri},
        {"diagnostics", json::array()}
    });

    std::cerr << "[visuall-lsp] Closed: " << uri << std::endl;
}

} // namespace lsp
