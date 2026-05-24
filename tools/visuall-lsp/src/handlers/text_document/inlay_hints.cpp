#include "lsp_server.h"
#include <ast.h>

namespace lsp {

json LspServer::handleInlayHints(const json& params) {
    std::string uri = params["textDocument"]["uri"];

    json hints = json::array();

    if (!store_.has(uri)) {
        return hints;
    }

    const Document& doc = store_.get(uri);
    if (!doc.ast) {
        return hints;
    }

    // Walk the AST to find places where type hints are useful.
    for (const auto& stmtPtr : doc.ast->statements) {
        if (!stmtPtr) continue;

        // Variable assignment without explicit type — show inferred type.
        if (auto* assign = dynamic_cast<const visuall::ast::AssignStmt*>(stmtPtr.get())) {
            if (auto* ident = dynamic_cast<const visuall::ast::Identifier*>(assign->target.get())) {
                // Infer type from the RHS.
                std::string inferredType;

                if (dynamic_cast<const visuall::ast::IntLiteral*>(assign->value.get())) {
                    inferredType = "int";
                } else if (dynamic_cast<const visuall::ast::FloatLiteral*>(assign->value.get())) {
                    inferredType = "float";
                } else if (dynamic_cast<const visuall::ast::StringLiteral*>(assign->value.get()) ||
                           dynamic_cast<const visuall::ast::FStringLiteral*>(assign->value.get()) ||
                           dynamic_cast<const visuall::ast::FStringExpr*>(assign->value.get())) {
                    inferredType = "str";
                } else if (dynamic_cast<const visuall::ast::BoolLiteral*>(assign->value.get())) {
                    inferredType = "bool";
                } else if (dynamic_cast<const visuall::ast::NullLiteral*>(assign->value.get())) {
                    inferredType = "null";
                } else if (dynamic_cast<const visuall::ast::ListExpr*>(assign->value.get())) {
                    inferredType = "list";
                } else if (dynamic_cast<const visuall::ast::DictExpr*>(assign->value.get())) {
                    inferredType = "dict";
                } else if (dynamic_cast<const visuall::ast::TupleExpr*>(assign->value.get())) {
                    inferredType = "tuple";
                } else if (auto* call = dynamic_cast<const visuall::ast::CallExpr*>(assign->value.get())) {
                    // Try to infer return type from function definition.
                    if (auto* callee = dynamic_cast<const visuall::ast::Identifier*>(call->callee.get())) {
                        const SymbolInfo* sym = findSymbolByName(doc, callee->name);
                        if (sym && !sym->typeName.empty()) {
                            inferredType = sym->typeName;
                        }
                    }
                }

                if (!inferredType.empty()) {
                    int hintLine = assign->line > 0 ? assign->line - 1 : 0;
                    int hintCol  = (assign->column > 0 ? assign->column - 1 : 0)
                                   + static_cast<int>(ident->name.size());

                    hints.push_back({
                        {"position", {{"line", hintLine}, {"character", hintCol}}},
                        {"label",    ": " + inferredType},
                        {"kind",     1}, // Type
                        {"paddingLeft", true}
                    });
                }
            }
        }

        // Function definition without return type annotation — show inferred return.
        if (auto* func = dynamic_cast<const visuall::ast::FuncDef*>(stmtPtr.get())) {
            if (func->returnType.empty()) {
                // Simple inference: look for return statements.
                std::string inferredReturn = "void";
                for (const auto& bodyStmt : func->body) {
                    if (auto* ret = dynamic_cast<const visuall::ast::ReturnStmt*>(bodyStmt.get())) {
                        if (ret->value) {
                            if (dynamic_cast<const visuall::ast::IntLiteral*>(ret->value.get())) {
                                inferredReturn = "int";
                            } else if (dynamic_cast<const visuall::ast::FloatLiteral*>(ret->value.get())) {
                                inferredReturn = "float";
                            } else if (dynamic_cast<const visuall::ast::StringLiteral*>(ret->value.get())) {
                                inferredReturn = "str";
                            } else if (dynamic_cast<const visuall::ast::BoolLiteral*>(ret->value.get())) {
                                inferredReturn = "bool";
                            } else {
                                inferredReturn = ""; // Can't infer — skip.
                            }
                        }
                        break;
                    }
                }

                if (!inferredReturn.empty()) {
                    // Place the hint at the end of the function signature line.
                    int hintLine = func->line > 0 ? func->line - 1 : 0;

                    // Find the colon at end of signature by scanning the source line.
                    std::string content = doc.content;
                    int lineStart = 0;
                    int currentLine = 0;
                    for (size_t i = 0; i < content.size(); ++i) {
                        if (currentLine == hintLine) {
                            lineStart = static_cast<int>(i);
                            break;
                        }
                        if (content[i] == '\n') ++currentLine;
                    }
                    int lineEnd = lineStart;
                    while (lineEnd < static_cast<int>(content.size()) && content[lineEnd] != '\n') {
                        ++lineEnd;
                    }

                    // Find the colon at the end of the line.
                    int colonPos = lineEnd;
                    for (int i = lineEnd - 1; i >= lineStart; --i) {
                        if (content[i] == ':') {
                            colonPos = i - lineStart;
                            break;
                        }
                    }

                    hints.push_back({
                        {"position", {{"line", hintLine}, {"character", colonPos}}},
                        {"label",    " -> " + inferredReturn},
                        {"kind",     1}, // Type
                        {"paddingLeft", true}
                    });
                }
            }

            // Lambda parameter types within function bodies.
            for (const auto& bodyStmt : func->body) {
                if (!bodyStmt) continue;
                if (auto* exprStmt = dynamic_cast<const visuall::ast::ExprStmt*>(bodyStmt.get())) {
                    (void)exprStmt; // Lambda detection would go deeper; simplified here.
                }
                if (auto* varAssign = dynamic_cast<const visuall::ast::AssignStmt*>(bodyStmt.get())) {
                    // Check if RHS is a lambda.
                    if (auto* lambda = dynamic_cast<const visuall::ast::LambdaExpr*>(varAssign->value.get())) {
                        // Show parameter type hints for the lambda.
                        int hintLine = lambda->line > 0 ? lambda->line - 1 : 0;
                        int hintCol  = lambda->column > 0 ? lambda->column - 1 : 0;

                        // Simple: we can't infer lambda param types without full type analysis.
                        // Just show the lambda structure.
                        if (!lambda->params.empty()) {
                            // Place hint after the lambda variable name.
                            if (auto* ident = dynamic_cast<const visuall::ast::Identifier*>(varAssign->target.get())) {
                                int varHintCol = (varAssign->column > 0 ? varAssign->column - 1 : 0)
                                                 + static_cast<int>(ident->name.size());

                                std::string label = ": (";
                                for (size_t i = 0; i < lambda->params.size(); ++i) {
                                    if (i > 0) label += ", ";
                                    label += lambda->params[i];
                                }
                                label += ") -> ...";

                                hints.push_back({
                                    {"position", {{"line", hintLine}, {"character", varHintCol}}},
                                    {"label",    label},
                                    {"kind",     1}, // Type
                                    {"paddingLeft", true}
                                });
                            }
                        }
                        (void)hintCol; // suppress unused warning
                    }
                }
            }
        }
    }

    return hints;
}

} // namespace lsp
