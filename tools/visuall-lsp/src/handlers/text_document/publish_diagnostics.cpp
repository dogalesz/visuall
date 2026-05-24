#include "lsp_server.h"

namespace lsp {

// publishDiagnostics is implemented in lsp_server.cpp as it's a
// server→client notification, not a handler. This file exists for
// structural completeness per the project layout.
//
// The publishDiagnostics() method is called after every reanalyze()
// in didOpen, didChange, and didClose handlers.
//
// Diagnostic format:
//   LexErrors   → severity 1 (Error)
//   ParseErrors → severity 1 (Error)
//   TypeErrors  → severity 1 (Error)
//
// All diagnostics have source: "visuall"

} // namespace lsp
