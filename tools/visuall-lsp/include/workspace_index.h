#pragma once

#include "document_store.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace lsp {

// ════════════════════════════════════════════════════════════════════════════
// WorkspaceIndex — persistent symbol index across all workspace .vsl files.
//
// Enables go-to-definition and find-references across files that are not
// currently open. Populated on initialize from the workspace root and kept
// up-to-date as files are opened, changed, and closed.
// ════════════════════════════════════════════════════════════════════════════
class WorkspaceIndex {
public:
    // FileEntry stores symbols from a single .vsl file.
    struct FileEntry {
        std::string uri;
        std::vector<SymbolInfo> symbols;
    };

    // Index (or re-index) a single file from its source content.
    void indexFile(const std::string& uri, const std::string& content);

    // Remove a file from the index (e.g., on didClose).
    void removeFile(const std::string& uri);

    // Look up a symbol by name across all indexed files.
    // Returns all matching definitions (may be multiple across files).
    std::vector<const SymbolInfo*> findDefinitions(const std::string& name) const;

    // Find all references to a given name across all indexed files.
    // For now this does a simple name match; full semantic references
    // would require the TypeChecker.
    std::vector<std::pair<std::string, SymbolInfo>> findReferences(
        const std::string& name) const;

    // Search symbols by prefix (case-insensitive) across all indexed files.
    std::vector<const SymbolInfo*> searchByPrefix(const std::string& prefix) const;

    // Access underlying data.
    const std::unordered_map<std::string, FileEntry>& files() const { return files_; }
    std::unordered_map<std::string, FileEntry>& files() { return files_; }

private:
    std::unordered_map<std::string, FileEntry> files_;
};

} // namespace lsp
