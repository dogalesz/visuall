#pragma once

#include <nlohmann/json.hpp>
#include <token.h>
#include <ast.h>
#include <typechecker.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace lsp {

using json = nlohmann::json;

// ════════════════════════════════════════════════════════════════════════════
// Diagnostic — an error/warning at a specific location.
// ════════════════════════════════════════════════════════════════════════════
struct Diagnostic {
    int startLine   = 0;
    int startCol    = 0;
    int endLine     = 0;
    int endCol      = 0;
    int severity    = 1; // 1=Error, 2=Warning, 3=Info, 4=Hint
    std::string source  = "visuall";
    std::string message;

    json toLsp() const {
        return {
            {"range", {
                {"start", {{"line", startLine}, {"character", startCol}}},
                {"end",   {{"line", endLine},   {"character", endCol}}}
            }},
            {"severity", severity},
            {"source",   source},
            {"message",  message}
        };
    }
};

// ════════════════════════════════════════════════════════════════════════════
// SymbolInfo — a collected symbol from the AST for LSP features.
// ════════════════════════════════════════════════════════════════════════════
struct SymbolInfo {
    std::string name;
    std::string detail;         // type signature
    std::string documentation;  // ## comment above definition
    int symbolKind = 0;         // LSP SymbolKind
    int defLine    = 0;
    int defCol     = 0;
    int endLine    = 0;
    int endCol     = 0;
    std::string typeName;       // resolved type name
    std::vector<SymbolInfo> children; // for class members
};

// ════════════════════════════════════════════════════════════════════════════
// Document — content and analysis results for a single open .vsl file.
// ════════════════════════════════════════════════════════════════════════════
struct Document {
    std::string uri;
    std::string content;
    int version = 0;

    // Last successful lex results.
    std::vector<visuall::Token> tokens;

    // Last successful parse result (may be nullptr on parse failure).
    std::unique_ptr<visuall::ast::Program> ast;

    // Collected symbols from the AST.
    std::vector<SymbolInfo> symbols;

    // All diagnostics from the last pipeline run.
    std::vector<Diagnostic> diagnostics;
};

// ════════════════════════════════════════════════════════════════════════════
// DocumentStore — manages the content of all open .vsl files.
// ════════════════════════════════════════════════════════════════════════════
class DocumentStore {
public:
    void open(const std::string& uri, const std::string& content, int version);
    void change(const std::string& uri, const std::string& content, int version);
    void close(const std::string& uri);

    bool has(const std::string& uri) const;
    Document& get(const std::string& uri);
    const Document& get(const std::string& uri) const;

    // Returns all open documents.
    std::unordered_map<std::string, Document>& documents();
    const std::unordered_map<std::string, Document>& documents() const;

    // Re-runs lex → parse → typecheck on the document.
    // NEVER crashes — collects all errors as Diagnostic objects.
    // Stores partial results even if later stages fail.
    void reanalyze(const std::string& uri);

private:
    std::unordered_map<std::string, Document> docs_;

    // Extract filename from URI for compiler error messages.
    static std::string uriToFilename(const std::string& uri);

    // Collect symbols from a parsed AST.
    static std::vector<SymbolInfo> collectSymbols(
        const visuall::ast::Program& program,
        const std::string& source);

    // Extract ## doc comment above a given line from source text.
    static std::string extractDocComment(const std::string& source, int defLine);

    // Strip "TypeName: message at file:line:col" prefix from error messages.
    static std::string stripErrorPrefix(const std::string& msg);
};

} // namespace lsp
