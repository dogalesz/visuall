#include "lsp_server.h"
#include "formatter.h"

namespace lsp {

json LspServer::handleFormatting(const json& params) {
    std::string uri = params["textDocument"]["uri"];

    if (!store_.has(uri)) {
        return json::array();
    }

    const Document& doc = store_.get(uri);

    // Format the document.
    Formatter formatter;
    std::string formatted = formatter.format(doc.content);

    if (formatted == doc.content) {
        return json::array(); // No changes needed.
    }

    // Return a single TextEdit covering the entire document.
    // Count lines in the original document.
    int lineCount = 0;
    int lastLineLength = 0;
    for (size_t i = 0; i < doc.content.size(); ++i) {
        if (doc.content[i] == '\n') {
            ++lineCount;
            lastLineLength = 0;
        } else {
            ++lastLineLength;
        }
    }

    json edits = json::array();
    edits.push_back({
        {"range", {
            {"start", {{"line", 0}, {"character", 0}}},
            {"end",   {{"line", lineCount}, {"character", lastLineLength}}}
        }},
        {"newText", formatted}
    });

    return edits;
}

} // namespace lsp
