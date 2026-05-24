#include "lsp_server.h"
#include <algorithm>

namespace lsp {

json LspServer::handleWorkspaceSymbol(const json& params) {
    std::string query = params.value("query", "");

    json symbols = json::array();

    // Search all open documents for matching symbols.
    for (const auto& [uri, doc] : store_.documents()) {
        for (const auto& sym : doc.symbols) {
            // Case-insensitive prefix match.
            bool matches = query.empty();
            if (!matches) {
                std::string symLower = sym.name;
                std::string queryLower = query;
                std::transform(symLower.begin(), symLower.end(), symLower.begin(), ::tolower);
                std::transform(queryLower.begin(), queryLower.end(), queryLower.begin(), ::tolower);
                matches = symLower.find(queryLower) == 0;
            }

            if (matches) {
                // Return as flat SymbolInformation (required by workspace/symbol).
                json symInfo = {
                    {"name", sym.name},
                    {"kind", sym.symbolKind},
                    {"location", {
                        {"uri", uri},
                        {"range", {
                            {"start", {{"line", sym.defLine}, {"character", sym.defCol}}},
                            {"end",   {{"line", sym.endLine > sym.defLine ? sym.endLine : sym.defLine},
                                       {"character", sym.endCol}}}
                        }}
                    }}
                };

                if (!sym.detail.empty()) {
                    symInfo["containerName"] = sym.detail;
                }

                symbols.push_back(symInfo);
            }

            // Also search children (class members, etc.).
            for (const auto& child : sym.children) {
                bool childMatches = query.empty();
                if (!childMatches) {
                    std::string childLower = child.name;
                    std::string queryLower = query;
                    std::transform(childLower.begin(), childLower.end(), childLower.begin(), ::tolower);
                    std::transform(queryLower.begin(), queryLower.end(), queryLower.begin(), ::tolower);
                    childMatches = childLower.find(queryLower) == 0;
                }

                if (childMatches) {
                    symbols.push_back({
                        {"name", child.name},
                        {"kind", child.symbolKind},
                        {"containerName", sym.name},
                        {"location", {
                            {"uri", uri},
                            {"range", {
                                {"start", {{"line", child.defLine}, {"character", child.defCol}}},
                                {"end",   {{"line", child.defLine}, {"character", child.defCol + static_cast<int>(child.name.size())}}}
                            }}
                        }}
                    });
                }
            }
        }
    }

    // Also search the workspace index for files not currently open.
    auto indexed = workspaceIndex_.searchByPrefix(query);
    for (const auto* sym : indexed) {
        // Find which file this symbol belongs to.
        std::string fileUri;
        for (const auto& [fUri, entry] : workspaceIndex_.files()) {
            for (const auto& s : entry.symbols) {
                if (&s == sym) { fileUri = fUri; break; }
                for (const auto& child : s.children) {
                    if (&child == sym) { fileUri = fUri; break; }
                }
                if (!fileUri.empty()) break;
            }
            if (!fileUri.empty()) break;
        }
        if (fileUri.empty()) continue;

        json symInfo = {
            {"name", sym->name},
            {"kind", sym->symbolKind},
            {"location", {
                {"uri", fileUri},
                {"range", {
                    {"start", {{"line", sym->defLine}, {"character", sym->defCol}}},
                    {"end",   {{"line", sym->defLine}, {"character", sym->defCol + static_cast<int>(sym->name.size())}}}
                }}
            }}
        };
        if (!sym->detail.empty()) {
            symInfo["containerName"] = sym->detail;
        }
        symbols.push_back(symInfo);
    }

    return symbols;
}

} // namespace lsp
