#include "lsp_server.h"
#include "capabilities.h"
#include <iostream>
#include <unordered_set>

namespace lsp {

LspServer::LspServer() {}

void LspServer::run() {
    registerHandlers();
    rpc_.run();
}

void LspServer::registerHandlers() {
    // ── Lifecycle ──────────────────────────────────────────────────────
    rpc_.onRequest("initialize", [this](const json& params) {
        return handleInitialize(params);
    });
    rpc_.onNotification("initialized", [this](const json& params) {
        handleInitialized(params);
    });
    rpc_.onRequest("shutdown", [this](const json& params) {
        return handleShutdown(params);
    });
    rpc_.onNotification("exit", [this](const json& params) {
        handleExit(params);
    });

    // ── Text document notifications ────────────────────────────────────
    rpc_.onNotification("textDocument/didOpen", [this](const json& params) {
        handleDidOpen(params);
    });
    rpc_.onNotification("textDocument/didChange", [this](const json& params) {
        handleDidChange(params);
    });
    rpc_.onNotification("textDocument/didClose", [this](const json& params) {
        handleDidClose(params);
    });

    // ── Text document requests ─────────────────────────────────────────
    rpc_.onRequest("textDocument/completion", [this](const json& params) {
        return handleCompletion(params);
    });
    rpc_.onRequest("textDocument/hover", [this](const json& params) {
        return handleHover(params);
    });
    rpc_.onRequest("textDocument/definition", [this](const json& params) {
        return handleDefinition(params);
    });
    rpc_.onRequest("textDocument/references", [this](const json& params) {
        return handleReferences(params);
    });
    rpc_.onRequest("textDocument/documentSymbol", [this](const json& params) {
        return handleDocumentSymbol(params);
    });
    rpc_.onRequest("textDocument/formatting", [this](const json& params) {
        return handleFormatting(params);
    });
    rpc_.onRequest("textDocument/inlayHint", [this](const json& params) {
        return handleInlayHints(params);
    });

    // ── Workspace requests ─────────────────────────────────────────────
    rpc_.onRequest("workspace/symbol", [this](const json& params) {
        return handleWorkspaceSymbol(params);
    });

    // ── Semantic tokens ────────────────────────────────────────────────
    rpc_.onRequest("textDocument/semanticTokens/full", [this](const json& params) {
        return handleSemanticTokens(params);
    });

    // ── Signature help ──────────────────────────────────────────────────
    rpc_.onRequest("textDocument/signatureHelp", [this](const json& params) {
        return handleSignatureHelp(params);
    });

    // ── Code actions ────────────────────────────────────────────────────
    rpc_.onRequest("textDocument/codeAction", [this](const json& params) {
        return handleCodeAction(params);
    });

    // ── Rename ──────────────────────────────────────────────────────────
    rpc_.onRequest("textDocument/rename", [this](const json& params) {
        return handleRename(params);
    });
}

void LspServer::publishDiagnostics(const std::string& uri) {
    json diagnosticsArray = json::array();

    if (store_.has(uri)) {
        const Document& doc = store_.get(uri);
        for (const auto& d : doc.diagnostics) {
            diagnosticsArray.push_back(d.toLsp());
        }
    }

    rpc_.sendNotification("textDocument/publishDiagnostics", {
        {"uri",         uri},
        {"diagnostics", diagnosticsArray}
    });
}

// ── Utility methods ────────────────────────────────────────────────────────

const visuall::Token* LspServer::findTokenAt(const Document& doc, int line, int col) const {
    // LSP positions are 0-based; tokens use 1-based line/column.
    int tokenLine = line + 1;
    int tokenCol  = col + 1;

    const visuall::Token* best = nullptr;
    for (const auto& tok : doc.tokens) {
        if (tok.line == tokenLine) {
            int tokEnd = tok.column + static_cast<int>(tok.lexeme.size());
            if (tokenCol >= tok.column && tokenCol <= tokEnd) {
                best = &tok;
                break;
            }
            // Pick the closest token on the same line.
            if (!best || std::abs(tok.column - tokenCol) < std::abs(best->column - tokenCol)) {
                best = &tok;
            }
        }
    }
    return best;
}

const SymbolInfo* LspServer::findSymbolByName(const Document& doc, const std::string& name) const {
    for (const auto& sym : doc.symbols) {
        if (sym.name == name) return &sym;
        for (const auto& child : sym.children) {
            if (child.name == name) return &child;
        }
    }
    return nullptr;
}

const SymbolInfo* LspServer::findSymbolAtPosition(const Document& doc, int line, int col) const {
    // Find a symbol whose definition line matches the token at this position.
    const visuall::Token* tok = findTokenAt(doc, line, col);
    if (!tok) return nullptr;
    if (tok->type != visuall::TokenType::IDENTIFIER) return nullptr;
    return findSymbolByName(doc, tok->lexeme);
}

std::vector<const visuall::Token*> LspServer::findAllReferences(
    const Document& doc, const std::string& name) const
{
    std::vector<const visuall::Token*> refs;
    for (const auto& tok : doc.tokens) {
        if (tok.type == visuall::TokenType::IDENTIFIER && tok.lexeme == name) {
            refs.push_back(&tok);
        }
    }
    return refs;
}

std::vector<SymbolInfo> LspServer::getVisibleSymbols(const Document& doc, int line) const {
    // Geometric cursor walk: use the TypeChecker's scope registry to find
    // symbols visible at the cursor position, respecting lexical scoping
    // and variable shadowing.

    // If no scope data (e.g., parse failure), fall back to flat list.
    if (doc.scopes.empty()) {
        std::vector<SymbolInfo> flat;
        for (const auto& sym : doc.symbols) {
            flat.push_back(sym);
            for (const auto& child : sym.children)
                flat.push_back(child);
        }
        return flat;
    }

    int cursorLine = line + 1; // LSP 0-based to AST 1-based

    // Find the innermost scope that contains the cursor.
    int scopeIdx = -1;
    for (int i = static_cast<int>(doc.scopes.size()) - 1; i >= 0; --i) {
        const auto& s = doc.scopes[i];
        if (s.startLine == 0 && s.endLine == 0) continue;
        if (cursorLine >= s.startLine && (s.endLine == 0 || cursorLine <= s.endLine)) {
            scopeIdx = i;
            break;
        }
    }

    // If no scope contains the cursor, default to the global scope (index 0).
    if (scopeIdx < 0) scopeIdx = 0;

    // Walk from the found scope up to the root via parentIndex,
    // collecting symbols while respecting shadowing.
    std::vector<SymbolInfo> visible;
    std::unordered_set<std::string> seen;

    int current = scopeIdx;
    while (current >= 0 && current < static_cast<int>(doc.scopes.size())) {
        const auto& scope = doc.scopes[current];

        for (const auto& [name, typeRef] : scope.symbols) {
            if (seen.count(name)) continue;
            seen.insert(name);

            bool found = false;
            for (const auto& sym : doc.symbols) {
                if (sym.name == name) {
                    SymbolInfo enriched = sym;
                    if (enriched.typeName.empty() && typeRef)
                        enriched.typeName = typeRef->toUserString();
                    visible.push_back(std::move(enriched));
                    found = true;
                    break;
                }
                for (const auto& child : sym.children) {
                    if (child.name == name) {
                        SymbolInfo enriched = child;
                        if (enriched.typeName.empty() && typeRef)
                            enriched.typeName = typeRef->toUserString();
                        visible.push_back(std::move(enriched));
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }
            if (!found && typeRef) {
                SymbolInfo sym;
                sym.name = name;
                sym.typeName = typeRef->toUserString();
                sym.symbolKind = 13;
                sym.defLine = scope.startLine > 0 ? scope.startLine - 1 : 0;
                sym.detail = typeRef->toUserString();
                visible.push_back(std::move(sym));
            }
        }

        current = scope.parentIndex;
    }

    return visible;
}

} // namespace lsp
