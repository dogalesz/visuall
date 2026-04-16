#include "module_loader.h"
#include "lexer.h"
#include "parser.h"
#include "capture_analyzer.h"
#include "codegen.h"
#include <iostream>
#include <sstream>
int main() {
    std::string src = "x = 10\nf = n -> n + x\ng = n -> n * x\n";
    try {
        visuall::Lexer lexer(src, "test.vsl");
        auto tokens = lexer.tokenize();
        visuall::Parser parser(tokens, "test.vsl");
        auto program = parser.parse();
        visuall::CaptureAnalyzer ca;
        ca.analyze(*program);
        visuall::Codegen codegen("test.vsl");
        codegen.generate(*program);
        std::cout << "OK\n";
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
    }
    return 0;
}
