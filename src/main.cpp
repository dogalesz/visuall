#include "diagnostic.h"
#include "lexer.h"
#include "parser.h"
#include "ast_printer.h"
#include "capture_analyzer.h"
#include "class_analyzer.h"
#include "codegen.h"
#include "module_loader.h"
#include "linker.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "error: could not open file: " << path << "\n";
        std::exit(1);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

static std::string getDirectory(const std::string& filepath) {
    auto pos = filepath.find_last_of("/\\");
    if (pos == std::string::npos) return ".";
    return filepath.substr(0, pos);
}

// Extract the Nth (1-based) line from a source string; empty if out of range.
static std::string getSourceLine(const std::string& source, int lineNo) {
    if (lineNo <= 0) return "";
    int cur = 1;
    size_t start = 0;
    while (start < source.size()) {
        size_t end = source.find('\n', start);
        if (end == std::string::npos) end = source.size();
        if (cur == lineNo) return source.substr(start, end - start);
        ++cur;
        start = end + 1;
    }
    return "";
}

// Format a Diagnostic; inject source_line from `source` when absent.
static std::string formatDiag(visuall::Diagnostic diag,
                               const std::string& source) {
    if (diag.source_line.empty() && diag.line > 0) {
        // Try to read from the already-loaded source (works for the main file).
        // For imported modules we won't have the source, so leave it blank.
        diag.source_line = getSourceLine(source, diag.line);
        // Rebuild what_ with the injected source line.
        diag.what_ = diag.format();
    }
    return diag.format();
}

static std::string getExeDirectory(const char* argv0) {
    std::string path(argv0);
    auto pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return ".";
    return path.substr(0, pos);
}

static void printUsage(const char* progName) {
    std::cerr << "Usage: " << progName << " [options] <file.vsl>\n"
              << "\n"
              << "Options:\n"
              << "  --tokens          Dump the token stream and exit\n"
              << "  --ast             Dump the AST and exit\n"
              << "  -o <file>         Output binary path (default: a.out)\n"
              << "  --emit-ir         Emit LLVM IR to stdout instead of compiling\n"
              << "  --module-path <d> Add directory to module search path\n"
              << "  --dump-modules    Print resolved module paths during compilation\n"
              << "  --gc-stats        Print GC statistics to stderr at program exit\n"
              << "  -h, --help        Show this message\n";
}

int main(int argc, char* argv[]) {
    std::string inputFile;
    std::string outputFile = "a.out";
    bool dumpTokens   = false;
    bool dumpAST      = false;
    bool emitIR       = false;
    bool dumpModules  = false;
    bool gcStats      = false;
    std::vector<std::string> extraModulePaths;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--tokens") {
            dumpTokens = true;
        } else if (arg == "--ast") {
            dumpAST = true;
        } else if (arg == "--emit-ir") {
            emitIR = true;
        } else if (arg == "-o" && i + 1 < argc) {
            outputFile = argv[++i];
        } else if (arg == "--module-path" && i + 1 < argc) {
            extraModulePaths.push_back(argv[++i]);
        } else if (arg == "--dump-modules") {
            dumpModules = true;
        } else if (arg == "--gc-stats") {
            gcStats = true;
        } else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg[0] == '-') {
            std::cerr << "error: unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        } else {
            inputFile = arg;
        }
    }

    if (inputFile.empty()) {
        std::cerr << "error: no input file\n";
        printUsage(argv[0]);
        return 1;
    }

    // ── Read source ────────────────────────────────────────────────────
    std::string source = readFile(inputFile);

    // ── Single top-level catch for all Diagnostic errors ──────────────
    // All compiler errors (LexError, ParseError, CodegenError, ImportError,
    // TypeError) inherit from Diagnostic, so one handler suffices.
    try {

    // ── Lexer ──────────────────────────────────────────────────────────
    std::vector<visuall::Token> tokens;
    {
        visuall::Lexer lexer(source, inputFile);
        tokens = lexer.tokenize();
    }

    if (dumpTokens) {
        for (const auto& tok : tokens) {
            std::cout << tok << "\n";
        }
        return 0;
    }

    // ── Parser ─────────────────────────────────────────────────────────
    std::unique_ptr<visuall::ast::Program> program;
    {
        visuall::Parser parser(tokens, inputFile);
        program = parser.parse();
    }

    if (dumpAST) {
        visuall::AstPrinter::print(*program, std::cout);
        return 0;
    }

    // ── Capture analysis ─────────────────────────────────────────────
    visuall::CaptureAnalyzer captureAnalyzer;
    captureAnalyzer.analyze(*program);

    // ── Class field analysis ──────────────────────────────────────────
    visuall::ClassAnalyzer classAnalyzer;
    classAnalyzer.analyze(*program);

    // ── Set up module loader ───────────────────────────────────────────
    // Stdlib directory: look next to the compiler executable, then fallback
    // to the source tree's stdlib/ and the current directory.
    std::string exeDir = getExeDirectory(argv[0]);
    std::string stdlibDir = exeDir + "/stdlib";

    // Also add the directory of the input file to module search paths
    std::string inputDir = getDirectory(inputFile);
    std::vector<std::string> modulePaths = extraModulePaths;

    visuall::ModuleLoader moduleLoader(stdlibDir, modulePaths, dumpModules);

    // ── Code generation ────────────────────────────────────────────────
    {
        visuall::Codegen codegen(inputFile);
        codegen.setModuleLoader(&moduleLoader);
        codegen.setSourceFile(inputFile);
        codegen.setGCStats(gcStats);
        codegen.setClassFields(classAnalyzer.classFields());
        moduleLoader.setContext(&codegen.getContext());
        codegen.generate(*program);

        if (emitIR) {
            // For --emit-ir, we still need to link modules for complete IR
            auto mainModule = codegen.takeModule();
            auto otherModules = moduleLoader.takeAllLLVMModules();
            if (!otherModules.empty()) {
                mainModule = visuall::Linker::link(
                    std::move(mainModule), std::move(otherModules));
            }
            visuall::Linker::optimize(*mainModule);
            std::string irStr;
            llvm::raw_string_ostream rso(irStr);
            mainModule->print(rso, nullptr);
            std::cout << irStr;
            return 0;
        }

        // Take the main module and link with any imported modules
        auto mainModule = codegen.takeModule();
        auto otherModules = moduleLoader.takeAllLLVMModules();
        if (!otherModules.empty()) {
            mainModule = visuall::Linker::link(
                std::move(mainModule), std::move(otherModules));
        }
        visuall::Linker::optimize(*mainModule);

        std::string irPath = outputFile + ".ll";
        visuall::Linker::writeIR(*mainModule, irPath);

        std::string objPath = outputFile + ".o";
        visuall::Linker::emitObjectFile(*mainModule, objPath);

        int result = visuall::Linker::linkToBinary(objPath, outputFile);
        if (result != 0) {
            std::cerr << "error: native compilation failed (linker returned "
                      << result << ")\n";
            return 1;
        }

        std::cout << "compiled: " << inputFile << " -> " << outputFile << "\n";

    } catch (const visuall::Diagnostic& diag) {
        // All compiler errors (LexError, ParseError, TypeError, CodegenError,
        // ImportError) are Diagnostic subclasses — print clang-style and exit.
        std::cerr << formatDiag(diag, source) << "\n";
        std::exit(1);
    } catch (const std::exception& e) {
        // Catch any remaining C++ exceptions so users never see raw C++ text.
        std::cerr << inputFile << ": internal error: " << e.what() << "\n";
        std::exit(1);
    }

    return 0;
}
