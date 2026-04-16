#pragma once

/* ════════════════════════════════════════════════════════════════════════════
 * ModuleLoader — File-based module system for Visuall.
 *
 * Every .vsl file is a module. The module name is its filename without the
 * extension. The loader resolves, caches, and compiles modules on demand.
 * ════════════════════════════════════════════════════════════════════════════ */

#include "ast.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <stdexcept>

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace visuall {

// ════════════════════════════════════════════════════════════════════════════
// ImportError — thrown on module‐resolution failures.
// ════════════════════════════════════════════════════════════════════════════
class ImportError : public std::runtime_error {
public:
    ImportError(const std::string& msg)
        : std::runtime_error("ImportError: " + msg) {}
};

// ════════════════════════════════════════════════════════════════════════════
// Symbol — a single exported symbol from a module.
// ════════════════════════════════════════════════════════════════════════════
struct Symbol {
    enum Kind { Function, Variable };
    Kind        kind;
    std::string name;            // Visuall‐visible name
    std::string mangledName;     // LLVM symbol name (module_name.func_name)
};

// ════════════════════════════════════════════════════════════════════════════
// Module — a compiled Visuall module.
// ════════════════════════════════════════════════════════════════════════════
struct Module {
    std::string                              name;
    std::string                              filepath;
    std::unordered_map<std::string, Symbol>  exports;
    std::unique_ptr<ast::Program>            ast;
    std::unique_ptr<llvm::LLVMContext>       llvmContext;  // must outlive llvmModule
    std::unique_ptr<llvm::Module>            llvmModule;
};

// ════════════════════════════════════════════════════════════════════════════
// ModuleLoader — resolves, caches, and compiles .vsl modules.
// ════════════════════════════════════════════════════════════════════════════
class ModuleLoader {
public:
    /// @param stdlibDir   Path to the stdlib/ directory (where stdlib .vsl lives).
    /// @param extraPaths  Extra search directories (from --module-path or VISUALL_PATH).
    /// @param verbose     Print resolved module paths (--dump-modules).
    explicit ModuleLoader(const std::string& stdlibDir,
                          const std::vector<std::string>& extraPaths = {},
                          bool verbose = false);

    /// Load (or return cached) a module by name.
    /// @param moduleName      Simple name: "utils", "math_helpers", etc.
    /// @param importFromFile  Absolute path of the file that contains the import.
    ///                        Used to derive the "current directory" for resolution.
    /// @return Reference to the loaded Module (owned by the cache).
    Module& load(const std::string& moduleName,
                 const std::string& importFromFile);

    /// Return all compiled LLVM modules. Ownership is transferred out.
    std::vector<std::unique_ptr<llvm::Module>> takeAllLLVMModules();

    /// Check if a module with this name has already been loaded.
    bool isCached(const std::string& moduleName) const;

    /// Get a cached module (throws if not cached).
    Module& getCached(const std::string& moduleName);

    /// Set the LLVM context that modules should compile into.
    void setContext(llvm::LLVMContext* ctx) { context_ = ctx; }

    /// Get the search paths for testing.
    const std::vector<std::string>& searchPaths() const { return searchPaths_; }

private:
    std::string                               stdlibDir_;
    std::vector<std::string>                  searchPaths_;
    bool                                      verbose_;
    llvm::LLVMContext*                        context_ = nullptr;

    // Module cache: module name → Module
    std::unordered_map<std::string, std::unique_ptr<Module>> cache_;

    // Circular-import detection: modules currently being loaded
    std::unordered_set<std::string> loadingStack_;

    /// Resolve module name to an absolute file path.
    /// @param moduleName      e.g. "utils"
    /// @param importDir       Directory of the importing file
    /// @return Absolute path, or throws ImportError
    std::string resolve(const std::string& moduleName,
                        const std::string& importDir);

    /// Read file contents (throws on failure).
    static std::string readFile(const std::string& path);

    /// Compile a module: lex → parse → capture-analyze → codegen.
    std::unique_ptr<Module> compile(const std::string& moduleName,
                                     const std::string& filepath);

    /// Extract public symbols from the AST (all top-level defs).
    static void extractExports(const ast::Program& program,
                               const std::string& moduleName,
                               std::unordered_map<std::string, Symbol>& exports);
};

} // namespace visuall
