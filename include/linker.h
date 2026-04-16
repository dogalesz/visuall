#pragma once

/* ════════════════════════════════════════════════════════════════════════════
 * Linker — Merges multiple LLVM modules and emits the final binary.
 *
 * Each Visuall module compiles to its own llvm::Module. The Linker merges
 * them into a single module using llvm::Linker, then runs optimization
 * passes and emits the final binary or object file.
 * ════════════════════════════════════════════════════════════════════════════ */

#include <string>
#include <vector>
#include <memory>
#include <iostream>

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace visuall {

class Linker {
public:
    /// Merge multiple LLVM modules into one.
    /// The first module is the "main" module; subsequent modules are linked
    /// into it. The other modules may be in different LLVMContexts — they
    /// will be re-parsed into the main module's context before linking.
    /// Ownership of all modules is consumed.
    /// @return The merged module (owned by the returned unique_ptr).
    static std::unique_ptr<llvm::Module>
    link(std::unique_ptr<llvm::Module> mainModule,
         std::vector<std::unique_ptr<llvm::Module>> others);

    /// Run LLVM optimization passes (O2) on the module.
    static void optimize(llvm::Module& mod);

    /// Write IR to a file.
    static void writeIR(const llvm::Module& mod, const std::string& path);

    /// Emit a native object file.
    static void emitObjectFile(llvm::Module& mod, const std::string& path);

    /// Link object file(s) to a final binary using the system linker.
    static int linkToBinary(const std::string& objPath,
                            const std::string& outPath);

    /// Full pipeline: merge → optimize → IR → object → binary.
    static int compileAndLink(std::unique_ptr<llvm::Module> mainModule,
                               std::vector<std::unique_ptr<llvm::Module>> others,
                               const std::string& outputPath,
                               bool emitIR = false,
                               std::ostream* irStream = nullptr);
};

} // namespace visuall
