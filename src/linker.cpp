/* ════════════════════════════════════════════════════════════════════════════
 * Linker implementation — merges LLVM modules and emits binaries.
 * ════════════════════════════════════════════════════════════════════════════ */

#include "linker.h"

#include <iostream>
#include <stdexcept>

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Linker/Linker.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace visuall {

// ── link ───────────────────────────────────────────────────────────────────

std::unique_ptr<llvm::Module>
Linker::link(std::unique_ptr<llvm::Module> mainModule,
             std::vector<std::unique_ptr<llvm::Module>> others) {
    auto& mainCtx = mainModule->getContext();

    for (auto& other : others) {
        if (!other) continue;

        // Check if the module is in the same context.
        if (&other->getContext() == &mainCtx) {
            // Same context: link directly.
            if (llvm::Linker::linkModules(*mainModule, std::move(other))) {
                throw std::runtime_error("LLVM Linker failed to merge modules");
            }
        } else {
            // Different context: serialize to IR string and re-parse into
            // the main module's context.
            std::string irStr;
            {
                llvm::raw_string_ostream rso(irStr);
                other->print(rso, nullptr);
            }

            llvm::SMDiagnostic err;
            auto reparsed = llvm::parseIR(
                *llvm::MemoryBuffer::getMemBuffer(irStr, other->getName()),
                err, mainCtx);
            if (!reparsed) {
                throw std::runtime_error(
                    "Failed to re-parse module IR for linking: " +
                    other->getName().str());
            }

            if (llvm::Linker::linkModules(*mainModule, std::move(reparsed))) {
                throw std::runtime_error("LLVM Linker failed to merge modules");
            }
        }
    }
    return mainModule;
}

// ── optimize ───────────────────────────────────────────────────────────────

void Linker::optimize(llvm::Module& mod) {
    llvm::LoopAnalysisManager     LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager    CGAM;
    llvm::ModuleAnalysisManager   MAM;

    llvm::PassBuilder PB;
    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    auto MPM = PB.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);
    MPM.run(mod, MAM);
}

// ── writeIR ────────────────────────────────────────────────────────────────

void Linker::writeIR(const llvm::Module& mod, const std::string& path) {
    std::error_code ec;
    llvm::raw_fd_ostream out(path, ec);
    if (ec) {
        throw std::runtime_error("Could not open IR output file: " + path +
                                 ": " + ec.message());
    }
    mod.print(out, nullptr);
}

// ── emitObjectFile ─────────────────────────────────────────────────────────

void Linker::emitObjectFile(llvm::Module& mod, const std::string& path) {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    auto triple = llvm::sys::getDefaultTargetTriple();
    mod.setTargetTriple(triple);

    std::string err;
    auto* target = llvm::TargetRegistry::lookupTarget(triple, err);
    if (!target) {
        throw std::runtime_error("TargetRegistry::lookupTarget: " + err);
    }

    llvm::TargetOptions opt;
    auto* TM = target->createTargetMachine(
        triple, "generic", "", opt, llvm::Reloc::PIC_);
    mod.setDataLayout(TM->createDataLayout());

    std::error_code ec;
    llvm::raw_fd_ostream dest(path, ec, llvm::sys::fs::OF_None);
    if (ec) {
        throw std::runtime_error("Could not open object output file: " + path +
                                 ": " + ec.message());
    }

    llvm::legacy::PassManager pass;
    if (TM->addPassesToEmitFile(pass, dest, nullptr,
                                 llvm::CodeGenFileType::ObjectFile)) {
        throw std::runtime_error("TargetMachine cannot emit object file");
    }
    pass.run(mod);
    dest.flush();
}

// ── linkToBinary ───────────────────────────────────────────────────────────

int Linker::linkToBinary(const std::string& objPath,
                          const std::string& outPath) {
    std::string cmd = "clang \"" + objPath + "\" -o \"" + outPath + "\" -lm";
    return std::system(cmd.c_str());
}

// ── compileAndLink ─────────────────────────────────────────────────────────

int Linker::compileAndLink(std::unique_ptr<llvm::Module> mainModule,
                            std::vector<std::unique_ptr<llvm::Module>> others,
                            const std::string& outputPath,
                            bool emitIR,
                            std::ostream* irStream) {
    // 1. Link all modules together
    auto merged = link(std::move(mainModule), std::move(others));

    // 2. Optimize
    optimize(*merged);

    // 3. If emit-ir requested, dump and stop
    if (emitIR && irStream) {
        std::string irStr;
        llvm::raw_string_ostream rso(irStr);
        merged->print(rso, nullptr);
        *irStream << irStr;
        return 0;
    }

    // 4. Write IR file (for debugging)
    std::string irPath = outputPath + ".ll";
    writeIR(*merged, irPath);

    // 5. Emit object file
    std::string objPath = outputPath + ".o";
    emitObjectFile(*merged, objPath);

    // 6. Link to final binary
    return linkToBinary(objPath, outputPath);
}

} // namespace visuall
