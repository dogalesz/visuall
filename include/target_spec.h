#pragma once

/* ════════════════════════════════════════════════════════════════════════════
 * TargetSpec — Describes a compilation target for the linker/optimizer.
 *
 * Default-constructed (all strings empty) = native host compilation.
 * When --target is provided, the struct carries the cross-compilation
 * parameters through the compiler pipeline.
 * ════════════════════════════════════════════════════════════════════════════ */

#include <string>

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <llvm/TargetParser/Host.h>    // getDefaultTargetTriple, getHostCPUName
#include <llvm/TargetParser/Triple.h>   // Triple::normalize
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace visuall {

struct TargetSpec {
    /// LLVM target triple, e.g. "aarch64-linux-gnu". Empty = native host.
    std::string triple;

    /// Target CPU, e.g. "cortex-a72". Empty = auto (host CPU for native,
    /// "generic" for cross).
    std::string cpu;

    /// Target CPU features string (comma-separated), e.g. "+neon,+fp-armv8".
    std::string features;

    /// Explicit linker path. Takes priority over linkerPrefix.
    std::string linker;

    /// Cross-compilation toolchain prefix, e.g. "aarch64-linux-gnu-".
    std::string linkerPrefix;

    /// Optional --sysroot path passed to the linker.
    std::string sysroot;

    /// Override directory containing runtime libraries (libvisuall_runtime.a,
    /// libyyjson.a). Empty = auto-detect relative to the compiler binary.
    std::string runtimeLibDir;

    /// True when this is a native (host) build.
    bool isNative() const { return triple.empty(); }

    /// The target triple for LLVM APIs (target lookup, module triple).
    /// Returns the user-provided triple, or the host default if empty.
    std::string effectiveTriple() const {
        return triple.empty() ? llvm::sys::getDefaultTargetTriple() : triple;
    }

    /// Canonical (normalized) triple for filesystem paths.
    /// e.g. "aarch64-linux-gnu" → "aarch64-unknown-linux-gnu"
    /// This prevents "lib not found" errors when the user's triple spelling
    /// differs from the canonical form.
    ///
    /// WARNING: For native builds (triple empty), this returns the host triple
    /// via effectiveTriple().  Do NOT use this to construct a runtime library
    /// path on native builds — you would get e.g. runtime/x86_64-linux-gnu/
    /// instead of the legacy flat directory.  Always check isNative() first.
    std::string normalizedTriple() const {
        llvm::Triple t(effectiveTriple());
        return t.normalize();
    }

    /// The target CPU for TargetMachine creation.
    /// Safety rule: host CPU only for native builds. Cross builds default
    /// to "generic" to avoid emitting instructions the target cannot execute.
    std::string effectiveCPU() const {
        if (!cpu.empty()) return cpu;                             // explicit --cpu
        if (triple.empty()) return llvm::sys::getHostCPUName().str(); // native
        return "generic";                                          // cross: safe
    }

    /// Resolve the linker executable name.
    /// Priority: --linker > --linker-prefix+gcc > "gcc"
    std::string linkerExe() const {
        if (!linker.empty()) return linker;
        if (!linkerPrefix.empty()) return linkerPrefix + "gcc";
        return "gcc";
    }
};

} // namespace visuall
