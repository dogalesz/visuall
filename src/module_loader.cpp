/* ════════════════════════════════════════════════════════════════════════════
 * ModuleLoader implementation — resolves, caches, and compiles .vsl modules.
 * ════════════════════════════════════════════════════════════════════════════ */

#include "module_loader.h"
#include "lexer.h"
#include "parser.h"
#include "capture_analyzer.h"
#include "codegen.h"
#include "builtins.h"

#include <cstdio>
#include <iostream>
#include <algorithm>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

namespace visuall {

// ── Helpers ────────────────────────────────────────────────────────────────

static std::string getDirectory(const std::string& filepath) {
    auto pos = filepath.find_last_of("/\\");
    if (pos == std::string::npos) return ".";
    return filepath.substr(0, pos);
}

static std::string normalizePath(const std::string& path) {
    std::string result = path;
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}

static bool fileExists(const std::string& path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
#endif
}

// ── Constructor ────────────────────────────────────────────────────────────

ModuleLoader::ModuleLoader(const std::string& stdlibDir,
                           const std::vector<std::string>& extraPaths,
                           bool verbose)
    : stdlibDir_(normalizePath(stdlibDir))
    , verbose_(verbose)
{
    // Build search path list from VISUALL_PATH env var and extra paths.
    // The current directory and stdlib dir are handled separately in resolve().
    for (const auto& p : extraPaths) {
        searchPaths_.push_back(normalizePath(p));
    }

    // Parse VISUALL_PATH environment variable
    const char* envPath = std::getenv("VISUALL_PATH");
    if (envPath) {
        std::string envStr(envPath);
#ifdef _WIN32
        char sep = ';';
#else
        char sep = ':';
#endif
        size_t start = 0;
        while (start < envStr.size()) {
            size_t end = envStr.find(sep, start);
            if (end == std::string::npos) end = envStr.size();
            std::string dir = envStr.substr(start, end - start);
            if (!dir.empty()) {
                searchPaths_.push_back(normalizePath(dir));
            }
            start = end + 1;
        }
    }
}

// ── Resolve ────────────────────────────────────────────────────────────────

std::string ModuleLoader::resolve(const std::string& moduleName,
                                   const std::string& importDir) {
    // Convert dots to path separators for submodule access
    std::string relativePath = moduleName;
    std::replace(relativePath.begin(), relativePath.end(), '.', '/');
    relativePath += ".vsl";

    std::vector<std::string> tried;

    // 1. Current directory (directory of the importing file)
    {
        std::string path = normalizePath(importDir + "/" + relativePath);
        tried.push_back(path);
        if (fileExists(path)) return path;
    }

    // 2. Extra paths (--module-path) and VISUALL_PATH
    for (const auto& dir : searchPaths_) {
        std::string path = normalizePath(dir + "/" + relativePath);
        tried.push_back(path);
        if (fileExists(path)) return path;
    }

    // 3. Stdlib directory
    {
        std::string path = normalizePath(stdlibDir_ + "/" + relativePath);
        tried.push_back(path);
        if (fileExists(path)) return path;
    }

    // Module not found — build error message with all paths tried.
    std::string msg = "Cannot find module '" + moduleName + "'\n  Paths tried:";
    for (const auto& p : tried) {
        msg += "\n    - " + p;
    }
    throw ImportError(msg);
}

// ── readFile ───────────────────────────────────────────────────────────────

std::string ModuleLoader::readFile(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        throw ImportError("Could not open module file: " + path);
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string result(static_cast<size_t>(len), '\0');
    fread(&result[0], 1, static_cast<size_t>(len), f);
    fclose(f);
    // Strip carriage returns for cross-platform compatibility.
    result.erase(std::remove(result.begin(), result.end(), '\r'), result.end());
    return result;
}

// ── extractExports ─────────────────────────────────────────────────────────

void ModuleLoader::extractExports(const ast::Program& program,
                                   const std::string& moduleName,
                                   std::unordered_map<std::string, Symbol>& exports) {
    for (const auto& stmt : program.statements) {
        // Top-level function definitions
        if (auto* fn = dynamic_cast<const ast::FuncDef*>(stmt.get())) {
            // Skip private functions (underscore prefix convention)
            if (!fn->name.empty() && fn->name[0] != '_') {
                Symbol sym;
                sym.kind = Symbol::Function;
                sym.name = fn->name;
                sym.mangledName = moduleName + "." + fn->name;
                exports[fn->name] = sym;
            }
        }
        // Top-level assignments (module-level variables)
        else if (auto* assign = dynamic_cast<const ast::AssignStmt*>(stmt.get())) {
            if (auto* ident = dynamic_cast<const ast::Identifier*>(assign->target.get())) {
                if (!ident->name.empty() && ident->name[0] != '_') {
                    Symbol sym;
                    sym.kind = Symbol::Variable;
                    sym.name = ident->name;
                    sym.mangledName = moduleName + "." + ident->name;
                    exports[ident->name] = sym;
                }
            }
        }
        // Top-level class definitions
        else if (auto* cls = dynamic_cast<const ast::ClassDef*>(stmt.get())) {
            if (!cls->name.empty() && cls->name[0] != '_') {
                Symbol sym;
                sym.kind = Symbol::Function; // treat as callable
                sym.name = cls->name;
                sym.mangledName = moduleName + "." + cls->name;
                exports[cls->name] = sym;
            }
        }
    }
}

// ── compile ────────────────────────────────────────────────────────────────

std::unique_ptr<Module> ModuleLoader::compile(const std::string& moduleName,
                                                const std::string& filepath) {
    // 1. Read the source file
    std::string source = readFile(filepath);

    // 2. Lex
    Lexer lexer(source, filepath);
    auto tokens = lexer.tokenize();

    // 3. Parse
    Parser parser(tokens, filepath);
    auto program = parser.parse();

    // 4. Capture analysis
    CaptureAnalyzer captureAnalyzer;
    captureAnalyzer.analyze(*program);

    // 5. Extract exports from the AST
    auto mod = std::make_unique<Module>();
    mod->name = moduleName;
    mod->filepath = filepath;
    extractExports(*program, moduleName, mod->exports);

    // 6. Code generation
    // Modules compile into their own codegen unit. We prefix all top-level
    // function names with the module name to avoid collisions.
    Codegen codegen(moduleName);
    codegen.setModuleMode(true);
    codegen.generate(*program);

    // 7. After codegen, rename top-level functions to module-qualified names
    //    so that they don't collide with names in other modules.
    llvm::Module& llvmMod = codegen.getModule();
    std::vector<std::pair<llvm::Function*, std::string>> renames;
    for (auto& fn : llvmMod) {
        std::string fnName = fn.getName().str();
        // Skip LLVM intrinsics, runtime functions, and main
        if (fnName.empty() || fnName[0] == '_' || fnName.find("llvm.") == 0 ||
            fnName == "main" || fnName.find("__visuall_") == 0 ||
            fn.isDeclaration()) {
            continue;
        }
        // Check if this is an exported symbol
        if (mod->exports.count(fnName)) {
            renames.push_back({&fn, moduleName + "." + fnName});
        }
    }
    for (auto& [fn, newName] : renames) {
        fn->setName(newName);
    }

    // 8. Store the AST and take the LLVM module + context out of Codegen.
    //    The context MUST outlive the module, so store it in Module too.
    mod->ast = std::move(program);
    mod->llvmModule = codegen.takeModule();
    mod->llvmContext = codegen.takeContext();

    return mod;
}

// ── load ───────────────────────────────────────────────────────────────────

Module& ModuleLoader::load(const std::string& moduleName,
                            const std::string& importFromFile) {
    // Check if already cached
    if (cache_.count(moduleName)) {
        return *cache_[moduleName];
    }

    // Check for built-in stdlib modules (handled directly in compiler)
    if (isStdlibModule(moduleName)) {
        // Create a stub module for built-in modules.
        auto mod = std::make_unique<Module>();
        mod->name = moduleName;
        mod->filepath = "<builtin:" + moduleName + ">";
        // No ast, no llvmModule — these are handled by the runtime.
        cache_[moduleName] = std::move(mod);
        return *cache_[moduleName];
    }

    // Circular import detection
    if (loadingStack_.count(moduleName)) {
        std::string cycle;
        for (const auto& m : loadingStack_) {
            cycle += m + " -> ";
        }
        cycle += moduleName;
        throw ImportError("Circular import detected: " + cycle);
    }

    // Resolve to a file path
    std::string importDir = getDirectory(importFromFile);
    std::string filepath = resolve(moduleName, importDir);

    if (verbose_) {
        std::cerr << "[module] " << moduleName << " -> " << filepath << "\n";
    }

    // Push onto loading stack
    loadingStack_.insert(moduleName);

    try {
        auto mod = compile(moduleName, filepath);
        loadingStack_.erase(moduleName);
        cache_[moduleName] = std::move(mod);
        return *cache_[moduleName];
    } catch (...) {
        loadingStack_.erase(moduleName);
        throw;
    }
}

// ── takeAllLLVMModules ─────────────────────────────────────────────────────

std::vector<std::unique_ptr<llvm::Module>> ModuleLoader::takeAllLLVMModules() {
    std::vector<std::unique_ptr<llvm::Module>> result;
    for (auto& [name, mod] : cache_) {
        if (mod->llvmModule) {
            result.push_back(std::move(mod->llvmModule));
        }
    }
    return result;
}

// ── isCached / getCached ───────────────────────────────────────────────────

bool ModuleLoader::isCached(const std::string& moduleName) const {
    return cache_.count(moduleName) > 0;
}

Module& ModuleLoader::getCached(const std::string& moduleName) {
    auto it = cache_.find(moduleName);
    if (it == cache_.end()) {
        throw ImportError("Module '" + moduleName + "' is not cached");
    }
    return *it->second;
}

} // namespace visuall
