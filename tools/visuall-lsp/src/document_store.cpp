#include "document_store.h"
#include <lexer.h>
#include <parser.h>
#include <typechecker.h>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <regex>

namespace lsp {

// ── CRUD operations ────────────────────────────────────────────────────────

void DocumentStore::open(const std::string& uri, const std::string& content, int version) {
    Document doc;
    doc.uri = uri;
    doc.content = content;
    doc.version = version;
    docs_[uri] = std::move(doc);
}

void DocumentStore::change(const std::string& uri, const std::string& content, int version) {
    auto it = docs_.find(uri);
    if (it != docs_.end()) {
        it->second.content = content;
        it->second.version = version;
    }
}

void DocumentStore::close(const std::string& uri) {
    docs_.erase(uri);
}

bool DocumentStore::has(const std::string& uri) const {
    return docs_.find(uri) != docs_.end();
}

Document& DocumentStore::get(const std::string& uri) {
    return docs_.at(uri);
}

const Document& DocumentStore::get(const std::string& uri) const {
    return docs_.at(uri);
}

std::unordered_map<std::string, Document>& DocumentStore::documents() {
    return docs_;
}

const std::unordered_map<std::string, Document>& DocumentStore::documents() const {
    return docs_;
}

// ── reanalyze — the core method ────────────────────────────────────────────

void DocumentStore::reanalyze(const std::string& uri) {
    auto it = docs_.find(uri);
    if (it == docs_.end()) return;

    Document& doc = it->second;
    doc.diagnostics.clear();
    doc.tokens.clear();
    doc.ast.reset();
    doc.symbols.clear();
    doc.scopes.clear();

    std::string filename = uriToFilename(uri);

    // Stage 1: Lex (error-tolerant)
    try {
        visuall::Lexer lexer(doc.content, filename);
        doc.tokens = lexer.tokenize();

        for (const auto& e : lexer.errors()) {
            Diagnostic d;
            d.startLine = e.line > 0 ? e.line - 1 : 0;  // LSP lines are 0-based
            d.startCol  = e.col > 0 ? e.col - 1 : 0;
            if (e.endLine > 0) {
                d.endLine = e.endLine - 1;
                d.endCol  = e.endCol - 1;
            } else {
                d.endLine = d.startLine;
                d.endCol  = d.startCol + 1;
            }
            d.severity  = 1;
            d.message   = stripErrorPrefix(e.what());
            doc.diagnostics.push_back(std::move(d));
        }
    } catch (const std::exception& e) {
        Diagnostic d;
        d.severity = 1;
        d.message  = std::string("Lexer error: ") + e.what();
        doc.diagnostics.push_back(std::move(d));
    }

    // Stage 2: Parse (error-tolerant with synchronize)
    try {
        visuall::Parser parser(doc.tokens, filename);
        doc.ast = parser.parse();

        for (const auto& e : parser.errors()) {
            Diagnostic d;
            d.startLine = e.line > 0 ? e.line - 1 : 0;
            d.startCol  = e.col > 0 ? e.col - 1 : 0;
            if (e.endLine > 0) {
                d.endLine = e.endLine - 1;
                d.endCol  = e.endCol - 1;
            } else {
                d.endLine = d.startLine;
                d.endCol  = d.startCol + 1;
            }
            d.severity  = 1;
            d.message   = stripErrorPrefix(e.what());
            doc.diagnostics.push_back(std::move(d));
        }
    } catch (const std::exception& e) {
        Diagnostic d;
        d.severity = 1;
        d.message  = std::string("Parser error: ") + e.what();
        doc.diagnostics.push_back(std::move(d));
    }

    // Collect symbols from AST (even partial).
    if (doc.ast) {
        doc.symbols = collectSymbols(*doc.ast, doc.content);
    }

    // Stage 3: Type-check (error-tolerant, collect-all mode for LSP)
    if (doc.ast) {
        try {
            visuall::TypeChecker checker(filename, false); // false = don't throw on first error
            checker.check(*doc.ast);

            // Enrich symbols with actual resolved types from the TypeChecker.
            // Also save scopes for scope-aware completion cursor walks.
            const auto& checkerScopes = checker.getScopes();
            doc.scopes.assign(checkerScopes.begin(), checkerScopes.end());
            for (auto& sym : doc.symbols) {
                // Walk scopes from innermost to outermost to find this symbol's resolved type.
                for (auto it = checkerScopes.rbegin(); it != checkerScopes.rend(); ++it) {
                    auto found = it->symbols.find(sym.name);
                    if (found != it->symbols.end()) {
                        sym.typeName = found->second->toUserString();
                        break;
                    }
                }
            }

            for (const auto& e : checker.errors()) {
                Diagnostic d;
                d.startLine = e.line > 0 ? e.line - 1 : 0;
                d.startCol  = e.col > 0 ? e.col - 1 : 0;
                if (e.endLine > 0) {
                    d.endLine = e.endLine - 1;
                    d.endCol  = e.endCol - 1;
                } else {
                    d.endLine = d.startLine;
                    d.endCol  = d.startCol + 1;
                }
                d.severity  = 1;
                d.message   = stripErrorPrefix(e.what());
                doc.diagnostics.push_back(std::move(d));
            }
        } catch (const std::exception& e) {
            Diagnostic d;
            d.severity = 1;
            d.message  = std::string("Type error: ") + e.what();
            doc.diagnostics.push_back(std::move(d));
        }
    }
}

// ── Symbol collection ──────────────────────────────────────────────────────

std::vector<SymbolInfo> DocumentStore::collectSymbols(
    const visuall::ast::Program& program,
    const std::string& source)
{
    std::vector<SymbolInfo> symbols;

    for (const auto& stmtPtr : program.statements) {
        if (!stmtPtr) continue;

        if (auto* func = dynamic_cast<const visuall::ast::FuncDef*>(stmtPtr.get())) {
            SymbolInfo sym;
            sym.name = func->name;
            sym.symbolKind = 12; // Function
            sym.defLine = func->line > 0 ? func->line - 1 : 0;
            sym.defCol  = func->column > 0 ? func->column - 1 : 0;
            sym.documentation = extractDocComment(source, func->line);

            // Build detail string: (param: type, ...) -> returnType
            std::string detail = "(";
            for (size_t i = 0; i < func->params.size(); ++i) {
                if (i > 0) detail += ", ";
                detail += func->params[i].name;
                if (!func->params[i].typeAnnotation.empty()) {
                    detail += ": " + func->params[i].typeAnnotation;
                }
            }
            detail += ")";
            if (!func->returnType.empty()) {
                detail += " -> " + func->returnType;
            }
            sym.detail = detail;
            sym.typeName = func->returnType;

            // Use end position from AST if available, else fall back to body.
            if (func->endLine > 0) {
                sym.endLine = func->endLine - 1;
                sym.endCol  = func->endCol - 1;
            } else if (!func->body.empty()) {
                auto& last = func->body.back();
                if (last) { sym.endLine = last->line > 0 ? last->line - 1 : sym.defLine; sym.endCol = 0; }
            } else {
                sym.endLine = sym.defLine; sym.endCol = 0;
            }

            symbols.push_back(std::move(sym));
        }
        else if (auto* cls = dynamic_cast<const visuall::ast::ClassDef*>(stmtPtr.get())) {
            SymbolInfo sym;
            sym.name = cls->name;
            sym.symbolKind = 5; // Class
            sym.defLine = cls->line > 0 ? cls->line - 1 : 0;
            sym.defCol  = cls->column > 0 ? cls->column - 1 : 0;
            sym.documentation = extractDocComment(source, cls->line);

            std::string detail = "class " + cls->name;
            if (cls->baseClass) {
                detail += " extends " + *cls->baseClass;
            }
            sym.detail = detail;

            // Collect class children.
            for (const auto& memberPtr : cls->body) {
                if (!memberPtr) continue;

                if (auto* init = dynamic_cast<const visuall::ast::InitDef*>(memberPtr.get())) {
                    SymbolInfo child;
                    child.name = "init";
                    child.symbolKind = 9; // Constructor
                    child.defLine = init->line > 0 ? init->line - 1 : 0;
                    child.defCol  = init->column > 0 ? init->column - 1 : 0;

                    std::string childDetail = "(";
                    for (size_t i = 0; i < init->params.size(); ++i) {
                        if (i > 0) childDetail += ", ";
                        childDetail += init->params[i].name;
                        if (!init->params[i].typeAnnotation.empty()) {
                            childDetail += ": " + init->params[i].typeAnnotation;
                        }
                    }
                    childDetail += ")";
                    child.detail = childDetail;
                    sym.children.push_back(std::move(child));

                    // Collect fields from init body: this.field = ...
                    for (const auto& bodyStmt : init->body) {
                        if (auto* assign = dynamic_cast<const visuall::ast::AssignStmt*>(bodyStmt.get())) {
                            if (auto* member = dynamic_cast<const visuall::ast::MemberExpr*>(assign->target.get())) {
                                if (dynamic_cast<const visuall::ast::ThisExpr*>(member->object.get())) {
                                    SymbolInfo field;
                                    field.name = member->member;
                                    field.symbolKind = 8; // Field
                                    field.defLine = assign->line > 0 ? assign->line - 1 : 0;
                                    field.defCol  = assign->column > 0 ? assign->column - 1 : 0;
                                    sym.children.push_back(std::move(field));
                                }
                            }
                        }
                    }
                }
                else if (auto* method = dynamic_cast<const visuall::ast::FuncDef*>(memberPtr.get())) {
                    SymbolInfo child;
                    child.name = method->name;
                    child.symbolKind = 6; // Method
                    child.defLine = method->line > 0 ? method->line - 1 : 0;
                    child.defCol  = method->column > 0 ? method->column - 1 : 0;

                    std::string childDetail = "(";
                    for (size_t i = 0; i < method->params.size(); ++i) {
                        if (i > 0) childDetail += ", ";
                        childDetail += method->params[i].name;
                        if (!method->params[i].typeAnnotation.empty()) {
                            childDetail += ": " + method->params[i].typeAnnotation;
                        }
                    }
                    childDetail += ")";
                    if (!method->returnType.empty()) {
                        childDetail += " -> " + method->returnType;
                    }
                    child.detail = childDetail;
                    sym.children.push_back(std::move(child));
                }
            }

            if (cls->endLine > 0) {
                sym.endLine = cls->endLine - 1;
                sym.endCol  = cls->endCol - 1;
            } else if (!cls->body.empty()) {
                auto& last = cls->body.back();
                if (last) { sym.endLine = last->line > 0 ? last->line - 1 : sym.defLine; sym.endCol = 0; }
            } else {
                sym.endLine = sym.defLine; sym.endCol = 0;
            }

            symbols.push_back(std::move(sym));
        }
        else if (auto* iface = dynamic_cast<const visuall::ast::InterfaceDef*>(stmtPtr.get())) {
            SymbolInfo sym;
            sym.name = iface->name;
            sym.symbolKind = 11; // Interface
            sym.defLine = iface->line > 0 ? iface->line - 1 : 0;
            sym.defCol  = iface->column > 0 ? iface->column - 1 : 0;
            sym.detail = "interface " + iface->name;
            sym.documentation = extractDocComment(source, iface->line);

            for (const auto& msig : iface->methods) {
                SymbolInfo child;
                child.name = msig.name;
                child.symbolKind = 6; // Method
                std::string childDetail = "(";
                for (size_t i = 0; i < msig.params.size(); ++i) {
                    if (i > 0) childDetail += ", ";
                    childDetail += msig.params[i].name;
                    if (!msig.params[i].typeAnnotation.empty()) {
                        childDetail += ": " + msig.params[i].typeAnnotation;
                    }
                }
                childDetail += ")";
                if (!msig.returnType.empty()) {
                    childDetail += " -> " + msig.returnType;
                }
                child.detail = childDetail;
                sym.children.push_back(std::move(child));
            }

            symbols.push_back(std::move(sym));
        }
        else if (auto* imp = dynamic_cast<const visuall::ast::ImportStmt*>(stmtPtr.get())) {
            SymbolInfo sym;
            sym.name = imp->module;
            sym.symbolKind = 2; // Module
            sym.defLine = imp->line > 0 ? imp->line - 1 : 0;
            sym.defCol  = imp->column > 0 ? imp->column - 1 : 0;
            sym.detail = "import " + imp->module;
            symbols.push_back(std::move(sym));
        }
        else if (auto* fimp = dynamic_cast<const visuall::ast::FromImportStmt*>(stmtPtr.get())) {
            SymbolInfo sym;
            sym.name = fimp->module;
            sym.symbolKind = 2; // Module
            sym.defLine = fimp->line > 0 ? fimp->line - 1 : 0;
            sym.defCol  = fimp->column > 0 ? fimp->column - 1 : 0;
            std::string names;
            for (size_t i = 0; i < fimp->names.size(); ++i) {
                if (i > 0) names += ", ";
                names += fimp->names[i];
            }
            sym.detail = "from " + fimp->module + " import " + names;
            symbols.push_back(std::move(sym));
        }
        else if (auto* assign = dynamic_cast<const visuall::ast::AssignStmt*>(stmtPtr.get())) {
            // Top-level variable assignments.
            if (auto* ident = dynamic_cast<const visuall::ast::Identifier*>(assign->target.get())) {
                SymbolInfo sym;
                sym.name = ident->name;
                sym.symbolKind = 13; // Variable
                sym.defLine = assign->line > 0 ? assign->line - 1 : 0;
                sym.defCol  = assign->column > 0 ? assign->column - 1 : 0;
                sym.endLine = sym.defLine;
                sym.endCol  = sym.defCol;
                symbols.push_back(std::move(sym));
            }
        }
    }

    return symbols;
}

std::string DocumentStore::extractDocComment(const std::string& source, int defLine) {
    // defLine is 1-based. Look at lines immediately above it for ## comments.
    std::istringstream stream(source);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }

    std::vector<std::string> commentLines;
    // defLine-1 is 0-based index of the definition line.
    // Look upward from defLine-2 for consecutive ## comment lines.
    for (int i = defLine - 2; i >= 0; --i) {
        std::string trimmed = lines[i];
        // Trim leading whitespace.
        size_t start = trimmed.find_first_not_of(" \t");
        if (start != std::string::npos) {
            trimmed = trimmed.substr(start);
        }
        if (trimmed.size() >= 2 && trimmed[0] == '#' && trimmed[1] == '#' &&
            (trimmed.size() < 3 || trimmed[2] != '#')) {
            // It's a ## comment (not ###).
            std::string text = trimmed.substr(2);
            if (!text.empty() && text[0] == ' ') text = text.substr(1);
            commentLines.push_back(text);
        } else {
            break; // Not a comment — stop looking.
        }
    }

    // Reverse because we collected bottom-to-top.
    std::reverse(commentLines.begin(), commentLines.end());

    std::string result;
    for (size_t i = 0; i < commentLines.size(); ++i) {
        if (i > 0) result += "\n";
        result += commentLines[i];
    }
    return result;
}

std::string DocumentStore::uriToFilename(const std::string& uri) {
    // Convert file:///path/to/file.vsl → path/to/file.vsl
    const std::string filePrefix = "file:///";
    if (uri.compare(0, filePrefix.size(), filePrefix) == 0) {
        std::string path = uri.substr(filePrefix.size());
        // Decode percent-encoded characters.
        std::string decoded;
        for (size_t i = 0; i < path.size(); ++i) {
            if (path[i] == '%' && i + 2 < path.size()) {
                std::string hex = path.substr(i + 1, 2);
                try {
                    char c = static_cast<char>(std::stoi(hex, nullptr, 16));
                    decoded += c;
                    i += 2;
                } catch (...) {
                    decoded += path[i];
                }
            } else {
                decoded += path[i];
            }
        }
#ifdef _WIN32
        // On Windows, URIs look like file:///C:/path — keep the drive letter.
        return decoded;
#else
        return "/" + decoded;
#endif
    }
    return uri;
}

std::string DocumentStore::stripErrorPrefix(const std::string& msg) {
    // Strip "ErrorType: message at file:line:col" → "message"
    // Pattern: "SomeError: actual message at path:123:45"
    std::regex prefixRegex(R"(^(?:Lex|Parse|Type|Indentation)Error:\s*)");
    std::string stripped = std::regex_replace(msg, prefixRegex, "");

    // Strip trailing " at file:line:col"
    std::regex suffixRegex(R"(\s+at\s+\S+:\d+:\d+$)");
    stripped = std::regex_replace(stripped, suffixRegex, "");

    return stripped;
}

} // namespace lsp
