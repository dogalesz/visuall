#include "lsp_server.h"
#include <token.h>

namespace lsp {

json LspServer::handleCodeAction(const json& params) {
    std::string uri = params["textDocument"]["uri"];
    json result = json::array();

    if (!store_.has(uri)) return result;

    const Document& doc = store_.get(uri);
    auto diags = params.value("context", json::object()).value("diagnostics", json::array());

    for (const auto& diag : diags) {
        std::string msg = diag.value("message", "");

        // "missing return statement" → suggest adding return
        if (msg.find("missing return statement") != std::string::npos) {
            int line = diag["range"]["start"]["line"];
            result.push_back({
                {"title", "Add 'return' statement"},
                {"kind", "quickfix"},
                {"edit", {
                    {"changes", {
                        {uri, json::array({
                            {{"range", {
                                {"start", {{"line", line + 1}, {"character", 0}}},
                                {"end",   {{"line", line + 1}, {"character", 0}}}
                            }},
                            {"newText", "\treturn null\n"}
                            }
                        })}
                    }}
                }}
            });
        }

        // "undefined" → suggest importing or fixing name
        if (msg.find("undefined") != std::string::npos) {
            // Extract the undefined name.
            auto pos = msg.find("'");
            if (pos != std::string::npos) {
                auto end = msg.find("'", pos + 1);
                if (end != std::string::npos) {
                    std::string name = msg.substr(pos + 1, end - pos - 1);
                    // Suggest a typo fix if a similar name exists.
                    for (const auto& sym : doc.symbols) {
                        if (sym.name != name &&
                            std::abs(static_cast<int>(sym.name.size()) -
                                     static_cast<int>(name.size())) <= 2) {
                            // Simple edit-distance check.
                            int diffs = 0;
                            size_t maxLen = std::max(sym.name.size(), name.size());
                            size_t minLen = std::min(sym.name.size(), name.size());
                            for (size_t i = 0; i < minLen && diffs <= 2; ++i)
                                if (sym.name[i] != name[i]) diffs++;
                            diffs += static_cast<int>(maxLen - minLen);
                            if (diffs <= 2) {
                                result.push_back({
                                    {"title", "Change '" + name + "' to '" + sym.name + "'"},
                                    {"kind", "quickfix"}
                                    // Full edit would need the diagnostic range.
                                });
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    return result;
}

} // namespace lsp
