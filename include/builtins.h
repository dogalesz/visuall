#pragma once

/* ════════════════════════════════════════════════════════════════════════════
 * Visuall Builtins — Compiler-known function signatures
 *
 * This header declares every builtin function that the Visuall compiler
 * knows about. The codegen checks against these names and emits calls
 * to the corresponding __visuall_* C runtime functions.
 * ════════════════════════════════════════════════════════════════════════════ */

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace visuall {

// ── Builtin function descriptor ────────────────────────────────────────────
struct BuiltinInfo {
    std::string name;           // Visuall-visible name: "len", "range", etc.
    std::string runtimeName;    // C symbol: "__visuall_str_len", etc.
    int minArgs;
    int maxArgs;                // -1 = variadic
    bool isVariadic;
};

// ── Module-qualified names ─────────────────────────────────────────────────
// e.g. "string.upper" -> "__visuall_string_upper"
struct ModuleFuncInfo {
    std::string moduleName;     // "string", "math", "io", "sys", "collections"
    std::string funcName;       // "upper", "sqrt", etc.
    std::string runtimeName;    // "__visuall_string_upper"
    int minArgs;
    int maxArgs;
};

// ── Registry ───────────────────────────────────────────────────────────────

inline bool isBuiltinFunction(const std::string& name) {
    static const std::unordered_set<std::string> builtins = {
        "print", "println", "input", "len", "range", "type",
        "int", "float", "str", "bool",
        "abs", "min", "max", "round",
        "sorted", "reversed", "enumerate", "zip",
        "map", "filter"
    };
    return builtins.count(name) > 0;
}

inline bool isStdlibModule(const std::string& name) {
    static const std::unordered_set<std::string> modules = {
        "string", "math", "io", "sys", "collections"
    };
    return modules.count(name) > 0;
}

// Map a module-qualified call (e.g. "math", "sqrt") to C runtime name.
inline std::string getModuleRuntimeName(const std::string& mod,
                                         const std::string& func) {
    return "__visuall_" + mod + "_" + func;
}

} // namespace visuall

namespace llvm { class Module; class LLVMContext; }

namespace visuall {
void declareRuntimeFunctions(llvm::Module& mod, llvm::LLVMContext& ctx);
} // namespace visuall
