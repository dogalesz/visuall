#pragma once

#include "ast.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace visuall {

// ════════════════════════════════════════════════════════════════════════════
// FrontendResult — the complete output of compileFrontend().
//
// Contains everything the codegen pass needs: typed AST, class field map,
// and escape-analysis results (which allocations can stay on the stack).
// ════════════════════════════════════════════════════════════════════════════
struct FrontendResult {
    std::unique_ptr<ast::Program> program;
    std::unordered_map<std::string, std::vector<std::string>> classFields;
    std::shared_ptr<const std::unordered_map<const void*, bool>> escapeInfo;
};

// ════════════════════════════════════════════════════════════════════════════
// compileFrontend — lex → parse → analyzers → typecheck (NO LLVM dependency).
//
// Runs the full frontend pipeline on a source string.  Throws a Diagnostic
// subclass (LexError, ParseError, TypeError) on any compiler-detected error.
//
// This function is reusable by:
//   • Fuzzer harnesses (both libFuzzer and AFL++)
//   • Tests (avoids duplicating the pipeline in every test file)
//   • The LSP server (future: parse-and-check without codegen)
//   • The CLI (main.cpp can delegate to it)
// ════════════════════════════════════════════════════════════════════════════
FrontendResult compileFrontend(const std::string& source,
                                const std::string& filename = "<input>");

} // namespace visuall
