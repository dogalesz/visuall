#include "lsp_server.h"
#include <iostream>

int main() {
    // Disable buffering on stderr for logging.
    std::cerr << "[visuall-lsp] Starting Visuall Language Server..." << std::endl;

    try {
        lsp::LspServer server;
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "[visuall-lsp] Fatal error: " << e.what() << std::endl;
        return 1;
    }

    std::cerr << "[visuall-lsp] Server exited." << std::endl;
    return 0;
}
