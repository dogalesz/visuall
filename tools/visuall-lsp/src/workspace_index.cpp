#include "workspace_index.h"
#include <lexer.h>
#include <parser.h>
#include <algorithm>
#include <cctype>

namespace lsp {

void WorkspaceIndex::indexFile(const std::string& uri, const std::string& content) {
    FileEntry entry;
    entry.uri = uri;

    std::string filename = DocumentStore::uriToFilename(uri);

    try {
        visuall::Lexer lexer(content, filename);
        auto tokens = lexer.tokenize();

        visuall::Parser parser(tokens, filename);
        auto ast = parser.parse();

        if (ast) {
            entry.symbols = DocumentStore::collectSymbols(*ast, content);
        }
    } catch (...) {
        // Indexing is best-effort; skip files with parse errors.
    }

    files_[uri] = std::move(entry);
}

void WorkspaceIndex::removeFile(const std::string& uri) {
    files_.erase(uri);
}

std::vector<const SymbolInfo*> WorkspaceIndex::findDefinitions(
    const std::string& name) const
{
    std::vector<const SymbolInfo*> results;
    for (const auto& [uri, entry] : files_) {
        for (const auto& sym : entry.symbols) {
            if (sym.name == name) {
                results.push_back(&sym);
                // Also check children (e.g., class methods).
                for (const auto& child : sym.children) {
                    if (child.name == name) {
                        results.push_back(&child);
                    }
                }
            }
            // Top-level only: check children even if parent name doesn't match.
            for (const auto& child : sym.children) {
                if (child.name == name) {
                    results.push_back(&child);
                }
            }
        }
    }
    return results;
}

std::vector<std::pair<std::string, SymbolInfo>> WorkspaceIndex::findReferences(
    const std::string& name) const
{
    std::vector<std::pair<std::string, SymbolInfo>> results;
    // For now, return all definitions as references.
    // Full semantic reference finding would require scanning token streams
    // and checking scope, which is a Phase 3+ feature.
    for (const auto& [uri, entry] : files_) {
        for (const auto& sym : entry.symbols) {
            if (sym.name == name) {
                results.emplace_back(uri, sym);
            }
            for (const auto& child : sym.children) {
                if (child.name == name) {
                    results.emplace_back(uri, child);
                }
            }
        }
    }
    return results;
}

std::vector<const SymbolInfo*> WorkspaceIndex::searchByPrefix(
    const std::string& prefix) const
{
    std::string lower = prefix;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    std::vector<const SymbolInfo*> results;
    for (const auto& [uri, entry] : files_) {
        for (const auto& sym : entry.symbols) {
            std::string symLower = sym.name;
            std::transform(symLower.begin(), symLower.end(), symLower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (symLower.find(lower) == 0) {
                results.push_back(&sym);
            }
            for (const auto& child : sym.children) {
                std::string childLower = child.name;
                std::transform(childLower.begin(), childLower.end(), childLower.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (childLower.find(lower) == 0) {
                    results.push_back(&child);
                }
            }
        }
    }
    return results;
}

} // namespace lsp
