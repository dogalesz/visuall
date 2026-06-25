#include "visuall_compile.h"
#include "lexer.h"
#include "parser.h"
#include "capture_analyzer.h"
#include "class_analyzer.h"
#include "escape_analyzer.h"
#include "typechecker.h"

namespace visuall {

FrontendResult compileFrontend(const std::string& source,
                                const std::string& filename) {
    // ── Lexer ──────────────────────────────────────────────────────────
    Lexer lexer(source, filename);
    auto tokens = lexer.tokenize();

    // ── Parser ─────────────────────────────────────────────────────────
    Parser parser(tokens, filename);
    auto program = parser.parse();

    // ── Capture analysis ───────────────────────────────────────────────
    CaptureAnalyzer captureAnalyzer;
    captureAnalyzer.analyze(*program);

    // ── Class field analysis ───────────────────────────────────────────
    ClassAnalyzer classAnalyzer;
    classAnalyzer.analyze(*program);

    // ── Escape analysis ────────────────────────────────────────────────
    EscapeAnalyzer escapeAnalyzer;
    escapeAnalyzer.analyze(*program);

    // ── Type checking ──────────────────────────────────────────────────
    TypeChecker typeChecker(filename);
    typeChecker.check(*program);

    // ── Package results ────────────────────────────────────────────────
    FrontendResult result;
    result.program = std::move(program);
    result.classFields = classAnalyzer.classFields();
    result.escapeInfo = std::make_shared<
        const std::unordered_map<const void*, bool>>(
            escapeAnalyzer.stackAllocatable());

    return result;
}

} // namespace visuall
