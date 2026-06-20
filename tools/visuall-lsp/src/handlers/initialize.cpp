#include "lsp_server.h"
#include "capabilities.h"
#include <iostream>

namespace lsp {

json LspServer::handleInitialize(const json& /*params*/) {
    initialized_ = true;

    json result = {
        {"capabilities", buildServerCapabilities()},
        {"serverInfo", {
            {"name",    "visuall-lsp"},
            {"version", "1.3.4"}
        }}
    };

    std::cerr << "[visuall-lsp] Initialized." << std::endl;
    return result;
}

void LspServer::handleInitialized(const json& /*params*/) {
    // Client acknowledged initialization — nothing to do.
    std::cerr << "[visuall-lsp] Client initialized notification received." << std::endl;
}

json LspServer::handleShutdown(const json& /*params*/) {
    shutdown_ = true;
    std::cerr << "[visuall-lsp] Shutdown requested." << std::endl;
    return nullptr; // Respond with null.
}

void LspServer::handleExit(const json& /*params*/) {
    std::cerr << "[visuall-lsp] Exit." << std::endl;
    rpc_.stop();
    std::exit(shutdown_ ? 0 : 1);
}

} // namespace lsp
