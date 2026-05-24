#pragma once

#include "json_rpc.h"
#include "document_store.h"
#include <nlohmann/json.hpp>

namespace lsp {

using json = nlohmann::json;

// ════════════════════════════════════════════════════════════════════════════
// LspServer — wires together the JSON-RPC transport, document store,
// and all LSP handlers.
// ════════════════════════════════════════════════════════════════════════════
class LspServer {
public:
    LspServer();

    // Start the server — registers all handlers, then runs the JSON-RPC loop.
    void run();

private:
    JsonRpc       rpc_;
    DocumentStore store_;
    bool          initialized_ = false;
    bool          shutdown_    = false;

    // Register all LSP request and notification handlers.
    void registerHandlers();

    // Publish diagnostics for a document to the client.
    void publishDiagnostics(const std::string& uri);

    // ── Handler functions ──────────────────────────────────────────────

    // Lifecycle
    json handleInitialize(const json& params);
    void handleInitialized(const json& params);
    json handleShutdown(const json& params);
    void handleExit(const json& params);

    // Text document notifications
    void handleDidOpen(const json& params);
    void handleDidChange(const json& params);
    void handleDidClose(const json& params);

    // Text document requests
    json handleCompletion(const json& params);
    json handleHover(const json& params);
    json handleDefinition(const json& params);
    json handleReferences(const json& params);
    json handleDocumentSymbol(const json& params);
    json handleFormatting(const json& params);
    json handleInlayHints(const json& params);

    // Workspace requests
    json handleWorkspaceSymbol(const json& params);

    // ── Utility ────────────────────────────────────────────────────────

    // Find the token at a given (0-based) line and column in a document.
    const visuall::Token* findTokenAt(const Document& doc, int line, int col) const;

    // Find a symbol by name in a document.
    const SymbolInfo* findSymbolByName(const Document& doc, const std::string& name) const;

    // Find a symbol at a given position.
    const SymbolInfo* findSymbolAtPosition(const Document& doc, int line, int col) const;

    // Collect all identifier tokens that match a given name.
    std::vector<const visuall::Token*> findAllReferences(
        const Document& doc, const std::string& name) const;

    // Get all symbols visible at a position (for completion).
    std::vector<SymbolInfo> getVisibleSymbols(const Document& doc, int line) const;
};

} // namespace lsp
