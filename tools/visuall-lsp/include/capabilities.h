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
            {"change", 2}   // 2 = Incremental — client sends per-change range diffs
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
        {"signatureHelpProvider", {
            {"triggerCharacters", {"(", ","}},
            {"retriggerCharacters", {}}
        }},
        {"codeActionProvider", true},
        {"renameProvider", true},
        {"workspaceSymbolProvider", true},
        {"semanticTokensProvider", {
            {"legend", {
                {"tokenTypes", {"keyword", "function", "class", "parameter",
                    "variable", "string", "number", "comment", "type"}},
                {"tokenModifiers", {"declaration", "defaultLibrary", "readonly"}}
            }},
            {"full", true},
            {"range", false}
        }}
    };
}

} // namespace lsp
