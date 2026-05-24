#include "lsp_server.h"

namespace lsp {

// Build a hierarchical DocumentSymbol JSON object from a SymbolInfo.
static json symbolInfoToDocumentSymbol(const SymbolInfo& sym) {
    json children = json::array();
    for (const auto& child : sym.children) {
        children.push_back(symbolInfoToDocumentSymbol(child));
    }

    json result = {
        {"name",           sym.name},
        {"kind",           sym.symbolKind},
        {"range", {
            {"start", {{"line", sym.defLine}, {"character", sym.defCol}}},
            {"end",   {{"line", sym.endLine > sym.defLine ? sym.endLine : sym.defLine},
                       {"character", sym.endCol}}}
        }},
        {"selectionRange", {
            {"start", {{"line", sym.defLine}, {"character", sym.defCol}}},
            {"end",   {{"line", sym.defLine},
                       {"character", sym.defCol + static_cast<int>(sym.name.size())}}}
        }}
    };

    if (!sym.detail.empty()) {
        result["detail"] = sym.detail;
    }

    if (!children.empty()) {
        result["children"] = children;
    }

    return result;
}

json LspServer::handleDocumentSymbol(const json& params) {
    std::string uri = params["textDocument"]["uri"];

    json symbols = json::array();

    if (!store_.has(uri)) {
        return symbols;
    }

    const Document& doc = store_.get(uri);

    for (const auto& sym : doc.symbols) {
        symbols.push_back(symbolInfoToDocumentSymbol(sym));
    }

    return symbols;
}

} // namespace lsp
