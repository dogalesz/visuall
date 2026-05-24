#pragma once

#include <nlohmann/json.hpp>

namespace lsp {

using json = nlohmann::json;

// ════════════════════════════════════════════════════════════════════════════
// buildServerCapabilities — returns the ServerCapabilities JSON object
// declared during initialize.
// ════════════════════════════════════════════════════════════════════════════
inline json buildServerCapabilities() {
    return {
        {"textDocumentSync", {
            {"openClose", true},
            {"change", 1}   // 1 = Full — send entire document on every change
        }},
        {"completionProvider", {
            {"triggerCharacters", {".", "(", "\"", "'"}},
            {"resolveProvider", false}
        }},
        {"hoverProvider", true},
        {"definitionProvider", true},
        {"referencesProvider", true},
        {"documentSymbolProvider", true},
        {"documentFormattingProvider", true},
        {"inlayHintProvider", true},
        {"workspaceSymbolProvider", true}
    };
}

} // namespace lsp
