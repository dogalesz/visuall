#include <iostream>
#include <cstdlib>

// Forward declarations of test suites
int runLexerTests();
int runParserTests();
int runTypeCheckerTests();
int runCodegenTests();
int runOperatorTests();
int runSyntaxTests();
int runTypeSystemTests();
int runClosureTests();
int runModuleLoaderTests();
int runGCTests();
int runClassAnalyzerTests();
int runDiagnosticTests();

int main() {
    int failures = 0;
    std::cout << "=== Visuall Compiler Test Suite ===\n\n";

    std::cout << "--- Lexer Tests ---\n";
    failures += runLexerTests();

    std::cout << "\n--- Parser Tests ---\n";
    failures += runParserTests();

    std::cout << "\n--- Type Checker Tests ---\n";
    failures += runTypeCheckerTests();

    std::cout << "\n--- Codegen Tests ---\n";
    failures += runCodegenTests();

    std::cout << "\n--- Operator Tests ---\n";
    failures += runOperatorTests();

    std::cout << "\n--- Syntax Tests ---\n";
    failures += runSyntaxTests();

    std::cout << "\n--- Type System Tests ---\n";
    failures += runTypeSystemTests();

    std::cout << "\n--- Closure Tests ---\n";
    failures += runClosureTests();

    std::cout << "\n--- Module Loader Tests ---\n";
    failures += runModuleLoaderTests();

    std::cout << "\n--- GC Tests ---\n";
    failures += runGCTests();

    std::cout << "\n--- Class Analyzer Tests ---\n";
    failures += runClassAnalyzerTests();

    std::cout << "\n--- Diagnostic Tests ---\n";
    failures += runDiagnosticTests();

    std::cout << "\n=== Results: ";
    if (failures == 0) {
        std::cout << "All tests passed! ===\n";
    } else {
        std::cout << failures << " test(s) FAILED ===\n";
    }

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
