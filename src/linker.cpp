/* ════════════════════════════════════════════════════════════════════════════
 * Linker implementation — merges LLVM modules and emits binaries.
 * ════════════════════════════════════════════════════════════════════════════ */

#include "linker.h"

#include <iostream>
#include <stdexcept>

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <llvm/IR/Function.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Linker/Linker.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Program.h>      // llvm::sys::ExecuteAndWait
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

// ════════════════════════════════════════════════════════════════════════════
// Target initialization helpers
// ════════════════════════════════════════════════════════════════════════════

static void initializeTargets() {
    // Initialize the native target (always available for host compilation).
    // Cross-compilation to other architectures requires an LLVM build that
    // includes the desired target backends (LLVM_TARGETS_TO_BUILD=all or
    // the specific target).  If the requested target is not available,
    // lookupTarget() will fail with a clear error message.
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
}

// Thread-safe one-shot initialization via function-local static.
// Guaranteed by the C++ standard to initialize exactly once.
static void ensureTargetsReady() {
    static const bool ready = []{ initializeTargets(); return true; }();
    (void)ready;
}

// Factored TargetMachine creation shared by optimize() and emitObjectFile().
static std::unique_ptr<llvm::TargetMachine>
createTargetMachine(const TargetSpec& spec, llvm::Module& mod) {
    auto tripleStr = spec.effectiveTriple();

    // setTargetTriple takes std::string in LLVM 17-20, llvm::Triple in 21+.
#if LLVM_VERSION_MAJOR >= 21
    mod.setTargetTriple(llvm::Triple(tripleStr));
#else
    mod.setTargetTriple(tripleStr);
#endif

    std::string err;
    auto* target = llvm::TargetRegistry::lookupTarget(tripleStr, err);
    if (!target) {
        throw std::runtime_error("unsupported target triple '" + tripleStr +
                                 "': " + err);
    }

    llvm::TargetOptions opt;
    auto cpuStr = spec.effectiveCPU();
    auto TM = std::unique_ptr<llvm::TargetMachine>(
        target->createTargetMachine(tripleStr, cpuStr, spec.features, opt,
                                    llvm::Reloc::PIC_));
    mod.setDataLayout(TM->createDataLayout());

    // Attach target-cpu / target-features attributes ONLY for cross-compilation,
    // so native --emit-ir output is byte-identical to today (backward compat
    // with any tests doing golden/string comparison of IR). On native builds,
    // LLVM already uses the host CPU through the TargetMachine directly.
    if (!spec.isNative()) {
        for (auto& F : mod.functions()) {
            if (!F.isDeclaration()) {
                F.addFnAttr("target-cpu", cpuStr);
                if (!spec.features.empty())
                    F.addFnAttr("target-features", spec.features);
            }
        }
    }

    return TM;
}

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

void Linker::optimize(llvm::Module& mod, const TargetSpec& spec) {
    ensureTargetsReady();
    auto TM = createTargetMachine(spec, mod);   // sets triple + data layout + fn attrs

    llvm::LoopAnalysisManager     LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager    CGAM;
    llvm::ModuleAnalysisManager   MAM;

    llvm::PassBuilder PB(TM.get());
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

void Linker::emitObjectFile(llvm::Module& mod, const std::string& path,
                             const TargetSpec& spec) {
    ensureTargetsReady();
    auto TM = createTargetMachine(spec, mod);   // sets triple + data layout + fn attrs

    std::error_code ec;
    llvm::raw_fd_ostream dest(path, ec, llvm::sys::fs::OF_None);
    if (ec) {
        throw std::runtime_error("Could not open object output file: " + path +
                                 ": " + ec.message());
    }

    llvm::legacy::PassManager pass;
    // CodeGenFileType became a scoped enum in LLVM 18 (CGFT_ prefix dropped).
#if LLVM_VERSION_MAJOR <= 17
    if (TM->addPassesToEmitFile(pass, dest, nullptr,
                                 llvm::CodeGenFileType::CGFT_ObjectFile)) {
#else
    if (TM->addPassesToEmitFile(pass, dest, nullptr,
                                 llvm::CodeGenFileType::ObjectFile)) {
#endif
        throw std::runtime_error("TargetMachine cannot emit object file");
    }
    pass.run(mod);
    dest.flush();
}

// ── linkToBinary ───────────────────────────────────────────────────────────

int Linker::linkToBinary(const std::string& objPath,
                          const std::string& outPath,
                          const std::string& exeDir,
                          const std::vector<std::string>& extraLibs,
                          const TargetSpec& spec) {
    // ── Deterministic runtime library lookup ──
    std::string libDir;
    if (!spec.runtimeLibDir.empty()) {
        // Priority 1: explicit --runtime-lib-dir override
        libDir = spec.runtimeLibDir;
    } else if (spec.isNative()) {
        // Priority 2 (native only): legacy flat directory
        libDir = exeDir;
    } else {
        // Priority 2 (cross): per-target subdirectory.
        // Try the normalized triple first, then the user's original triple
        // as a fallback.  This prevents "library not found" when the user
        // built into a directory named with the non-normalized triple
        // (e.g. "aarch64-linux-gnu" vs "aarch64-unknown-linux-gnu").
        std::string normDir = exeDir + "/runtime/" + spec.normalizedTriple();
        if (llvm::sys::fs::exists(normDir)) {
            libDir = normDir;
        } else {
            // Fallback: try the user-supplied triple directory name.
            std::string userDir = exeDir + "/runtime/" + spec.triple;
            if (spec.triple != spec.normalizedTriple() &&
                llvm::sys::fs::exists(userDir)) {
                libDir = userDir;
            } else {
                libDir = normDir;  // will fail below with a clear error
            }
        }
    }

    std::string yyjsonDir = libDir + "/_deps/yyjson-build";

    // ── Pre-link existence check: fail early with a clear message ──
    std::string rtLibName = libDir + "/libvisuall_runtime.a";
    std::string yyjsonLibName = yyjsonDir + "/libyyjson.a";
    if (!llvm::sys::fs::exists(rtLibName)) {
        std::cerr << "error: runtime library not found: " << rtLibName << "\n"
                  << "hint: build the runtime for this target, or use"
                  << " --runtime-lib-dir to specify the location\n";
        // If we fell through to the normalized dir but the user may have
        // tried the non-normalized form, mention both possibilities.
        if (!spec.isNative() && spec.triple != spec.normalizedTriple()) {
            std::string altDir = exeDir + "/runtime/" + spec.triple;
            std::cerr << "note: also tried " << altDir << "\n";
        }
        return 1;
    }
    if (!llvm::sys::fs::exists(yyjsonLibName)) {
        std::cerr << "error: yyjson library not found: " << yyjsonLibName << "\n"
                  << "hint: build yyjson for this target, or use"
                  << " --runtime-lib-dir to specify the location\n";
        return 1;
    }

    // ── Build argument vector (NO shell — each argv entry is separate) ──
    // NOTE: Every concatenated string is stored in a named local variable.
    // Do NOT push temporary "-L" + libDir directly — StringRef is non-owning
    // and the temporary would dangle, causing garbage arguments or a crash.
    std::string linker = spec.linkerExe();
    llvm::SmallVector<llvm::StringRef, 16> args;
    args.push_back(linker);
    args.push_back(objPath);
    args.push_back("-o");
    args.push_back(outPath);

    std::string libFlag = "-L" + libDir;
    args.push_back(libFlag);
    args.push_back("-lvisuall_runtime");

    std::string yyjsonFlag = "-L" + yyjsonDir;
    args.push_back(yyjsonFlag);
    args.push_back("-lyyjson");

    // Platform-conditional: -lws2_32 only when targeting Windows
    llvm::Triple triple(spec.effectiveTriple());
    if (triple.isOSWindows()) {
        args.push_back("-lws2_32");
    }
    args.push_back("-lm");

    std::string sysrootFlag;
    if (!spec.sysroot.empty()) {
        sysrootFlag = "--sysroot=" + spec.sysroot;
        args.push_back(sysrootFlag);
    }

    std::vector<std::string> extraLibFlags;
    extraLibFlags.reserve(extraLibs.size());
    for (const auto& lib : extraLibs) {
        extraLibFlags.push_back("-l" + lib);
        args.push_back(extraLibFlags.back());
    }

    // ── Execute linker directly (no shell, no injection possible) ──
    std::string errMsg;
    bool executionFailed = false;
    int rc = llvm::sys::ExecuteAndWait(
        linker,                              // program path
        args,                                // argument array
        /*Env=*/std::nullopt,                // inherit environment
        /*Redirects=*/{},                    // inherit stdin/stdout/stderr
        /*SecondsToWait=*/0,                 // wait forever
        /*MemoryLimit=*/0,                   // no memory limit
        &errMsg,                             // capture execution errors
        &executionFailed                     // set on failure
    );

    if (executionFailed || !errMsg.empty()) {
        std::cerr << "error: failed to execute linker '" << linker
                  << "': " << errMsg << "\n";
        return 1;
    }
    return rc;
}

// ── compileAndLink ─────────────────────────────────────────────────────────

int Linker::compileAndLink(std::unique_ptr<llvm::Module> mainModule,
                            std::vector<std::unique_ptr<llvm::Module>> others,
                            const std::string& outputPath,
                            const std::string& exeDir,
                            bool emitIR,
                            std::ostream* irStream,
                            const std::vector<std::string>& extraLibs,
                            const TargetSpec& spec) {
    // 1. Link all modules together
    auto merged = link(std::move(mainModule), std::move(others));

    // 2. Optimize (also sets target triple + data layout)
    optimize(*merged, spec);

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
    emitObjectFile(*merged, objPath, spec);

    // 6. Link to final binary
    return linkToBinary(objPath, outputPath, exeDir, extraLibs, spec);
}

} // namespace visuall
