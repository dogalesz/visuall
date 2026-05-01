/* ════════════════════════════════════════════════════════════════════════════
 * Visuall Builtins — Codegen for builtin functions
 *
 * This file implements the LLVM IR generation for all Visuall builtin
 * functions (len, range, sorted, etc.) and stdlib module functions
 * (string.upper, math.sqrt, io.read_file, etc.).
 *
 * All builtins emit calls to the __visuall_* C runtime functions declared
 * in stdlib/runtime.c.
 * ════════════════════════════════════════════════════════════════════════════ */

#include "builtins.h"
#include "codegen.h"
#include "ast.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>

namespace visuall {

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper: declare an external C function if it doesn't already exist.
 * ═══════════════════════════════════════════════════════════════════════════ */
static llvm::Function* getOrDeclareExtern(llvm::Module& mod,
                                           llvm::LLVMContext& ctx,
                                           const std::string& name,
                                           llvm::Type* retType,
                                           std::vector<llvm::Type*> paramTypes,
                                           bool isVarArg = false) {
    if (auto* fn = mod.getFunction(name)) return fn;
    auto* fnType = llvm::FunctionType::get(retType, paramTypes, isVarArg);
    return llvm::Function::Create(fnType, llvm::Function::ExternalLinkage,
                                  name, &mod);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Declare all runtime functions the compiler may call.
 * Called once during codegen initialization.
 * ═══════════════════════════════════════════════════════════════════════════ */
void declareRuntimeFunctions(llvm::Module& mod, llvm::LLVMContext& ctx) {
    auto* i64  = llvm::Type::getInt64Ty(ctx);
    auto* f64  = llvm::Type::getDoubleTy(ctx);
    auto* i8p  = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(ctx));
    auto* voidTy = llvm::Type::getVoidTy(ctx);
    auto* i32  = llvm::Type::getInt32Ty(ctx);
    (void)i32;

    /* ── Print functions ─────────────────────────────────────────────── */
    getOrDeclareExtern(mod, ctx, "__visuall_print_str",   voidTy, {i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_print_int",   voidTy, {i64});
    getOrDeclareExtern(mod, ctx, "__visuall_print_float", voidTy, {f64});
    getOrDeclareExtern(mod, ctx, "__visuall_print_bool",  voidTy, {i64});
    getOrDeclareExtern(mod, ctx, "__visuall_print_newline", voidTy, {});

    /* ── Input ───────────────────────────────────────────────────────── */
    getOrDeclareExtern(mod, ctx, "__visuall_input", i8p, {i8p});

    /* ── Conversions ─────────────────────────────────────────────────── */
    getOrDeclareExtern(mod, ctx, "__visuall_int_to_str",   i8p, {i64});
    getOrDeclareExtern(mod, ctx, "__visuall_float_to_str", i8p, {f64});
    getOrDeclareExtern(mod, ctx, "__visuall_bool_to_str",  i8p, {i64});
    getOrDeclareExtern(mod, ctx, "__visuall_str_to_int",   i64, {i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_str_to_float", f64, {i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_float_to_int", i64, {f64});
    getOrDeclareExtern(mod, ctx, "__visuall_int_to_float", f64, {i64});
    getOrDeclareExtern(mod, ctx, "__visuall_bool_to_int",  i64, {i64});
    getOrDeclareExtern(mod, ctx, "__visuall_str_to_bool",  i64, {i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_int_to_bool",  i64, {i64});
    getOrDeclareExtern(mod, ctx, "__visuall_float_to_bool", i64, {f64});

    /* ── String concat ───────────────────────────────────────────────── */
    getOrDeclareExtern(mod, ctx, "__visuall_str_concat", i8p, {i8p, i8p});

    /* ── f-string builder ────────────────────────────────────────────── */
    getOrDeclareExtern(mod, ctx, "__visuall_fstring_build", i8p,
                       {llvm::PointerType::getUnqual(i8p), i32});

    /* ── String comparison ────────────────────────────────────────────── */
    getOrDeclareExtern(mod, ctx, "__visuall_str_eq", i64, {i8p, i8p});

    /* ── String length ───────────────────────────────────────────────── */
    getOrDeclareExtern(mod, ctx, "__visuall_str_len", i64, {i8p});

    /* ── List runtime ────────────────────────────────────────────────── */
    getOrDeclareExtern(mod, ctx, "__visuall_list_new",  i8p, {});
    getOrDeclareExtern(mod, ctx, "__visuall_list_push", voidTy, {i8p, i64});
    getOrDeclareExtern(mod, ctx, "__visuall_list_get",  i64, {i8p, i64});
    getOrDeclareExtern(mod, ctx, "__visuall_list_set",  voidTy, {i8p, i64, i64});
    getOrDeclareExtern(mod, ctx, "__visuall_list_len",  i64, {i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_list_remove", voidTy, {i8p, i64});

    /* ── Dict runtime ────────────────────────────────────────────────── */
    getOrDeclareExtern(mod, ctx, "__visuall_dict_new",  i8p, {});
    getOrDeclareExtern(mod, ctx, "__visuall_dict_set",  voidTy, {i8p, i8p, i64});
    getOrDeclareExtern(mod, ctx, "__visuall_dict_get",  i64, {i8p, i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_dict_has",  i64, {i8p, i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_dict_len",  i64, {i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_dict_contains", i64, {i8p, i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_dict_remove", voidTy, {i8p, i8p});

    /* ── Tag introspection & membership ───────────────────────────────── */
    getOrDeclareExtern(mod, ctx, "__visuall_get_tag",        i64, {i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_list_contains",  i64, {i8p, i64});

    /* ── Tuple runtime ───────────────────────────────────────────────── */
    getOrDeclareExtern(mod, ctx, "__visuall_tuple_new", i8p, {i64});

    /* ── Slice runtime ───────────────────────────────────────────────── */
    getOrDeclareExtern(mod, ctx, "__visuall_list_slice", i8p, {i8p, i64, i64, i64});

    /* ── range() ─────────────────────────────────────────────────────── */
    getOrDeclareExtern(mod, ctx, "__visuall_range", i8p, {i64, i64, i64});

    /* ── type() ──────────────────────────────────────────────────────── */
    getOrDeclareExtern(mod, ctx, "__visuall_type_name", i8p, {i64});

    /* ── abs, min, max, round ────────────────────────────────────────── */
    getOrDeclareExtern(mod, ctx, "__visuall_abs_int",   i64, {i64});
    getOrDeclareExtern(mod, ctx, "__visuall_abs_float", f64, {f64});
    getOrDeclareExtern(mod, ctx, "__visuall_min_int",   i64, {i64, i64});
    getOrDeclareExtern(mod, ctx, "__visuall_max_int",   i64, {i64, i64});
    getOrDeclareExtern(mod, ctx, "__visuall_min_float", f64, {f64, f64});
    getOrDeclareExtern(mod, ctx, "__visuall_max_float", f64, {f64, f64});
    getOrDeclareExtern(mod, ctx, "__visuall_round",     i64, {f64});

    /* ── sorted, reversed, enumerate, zip ────────────────────────────── */
    getOrDeclareExtern(mod, ctx, "__visuall_sorted",    i8p, {i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_reversed",  i8p, {i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_enumerate", i8p, {i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_zip",       i8p, {i8p, i8p});

    /* ── map, filter ─────────────────────────────────────────────────── */
    getOrDeclareExtern(mod, ctx, "__visuall_map",    i8p, {i8p, i8p, i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_filter", i8p, {i8p, i8p, i8p});

    /* ── string module ───────────────────────────────────────────────── */
    getOrDeclareExtern(mod, ctx, "__visuall_string_len",         i64, {i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_string_index",       i8p, {i8p, i64});
    getOrDeclareExtern(mod, ctx, "__visuall_string_slice",       i8p, {i8p, i64, i64, i64});
    getOrDeclareExtern(mod, ctx, "__visuall_string_upper",       i8p, {i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_string_lower",       i8p, {i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_string_strip",       i8p, {i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_string_lstrip",      i8p, {i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_string_rstrip",      i8p, {i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_string_split",       i8p, {i8p, i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_string_join",        i8p, {i8p, i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_string_replace",     i8p, {i8p, i8p, i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_string_starts_with", i64, {i8p, i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_string_ends_with",   i64, {i8p, i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_string_contains",    i64, {i8p, i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_string_find",        i64, {i8p, i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_string_repeat",      i8p, {i8p, i64});
    getOrDeclareExtern(mod, ctx, "__visuall_string_format",      i8p, {i8p, i8p});

    /* ── math module ─────────────────────────────────────────────────── */
    getOrDeclareExtern(mod, ctx, "__visuall_math_pi",    f64, {});
    getOrDeclareExtern(mod, ctx, "__visuall_math_e",     f64, {});
    getOrDeclareExtern(mod, ctx, "__visuall_math_sqrt",  f64, {f64});
    getOrDeclareExtern(mod, ctx, "__visuall_math_pow",   f64, {f64, f64});
    getOrDeclareExtern(mod, ctx, "__visuall_math_floor", i64, {f64});
    getOrDeclareExtern(mod, ctx, "__visuall_math_ceil",  i64, {f64});
    getOrDeclareExtern(mod, ctx, "__visuall_math_abs",   f64, {f64});
    getOrDeclareExtern(mod, ctx, "__visuall_math_sin",   f64, {f64});
    getOrDeclareExtern(mod, ctx, "__visuall_math_cos",   f64, {f64});
    getOrDeclareExtern(mod, ctx, "__visuall_math_tan",   f64, {f64});
    getOrDeclareExtern(mod, ctx, "__visuall_math_log",   f64, {f64});
    getOrDeclareExtern(mod, ctx, "__visuall_math_log2",  f64, {f64});
    getOrDeclareExtern(mod, ctx, "__visuall_math_log10", f64, {f64});
    getOrDeclareExtern(mod, ctx, "__visuall_math_clamp", f64, {f64, f64, f64});
    getOrDeclareExtern(mod, ctx, "__visuall_math_lerp",  f64, {f64, f64, f64});

    /* ── io module ───────────────────────────────────────────────────── */
    getOrDeclareExtern(mod, ctx, "__visuall_io_read_file",   i8p,    {i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_io_write_file",  voidTy, {i8p, i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_io_append_file", voidTy, {i8p, i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_io_file_exists", i64,    {i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_io_delete_file", voidTy, {i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_io_list_dir",    i8p,    {i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_io_make_dir",    voidTy, {i8p});

    /* ── sys module ──────────────────────────────────────────────────── */
    getOrDeclareExtern(mod, ctx, "__visuall_sys_get_args",  i8p,    {});
    getOrDeclareExtern(mod, ctx, "__visuall_sys_exit",      voidTy, {i64});
    getOrDeclareExtern(mod, ctx, "__visuall_sys_env",       i8p,    {i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_sys_platform",  i8p,    {});
    getOrDeclareExtern(mod, ctx, "__visuall_sys_time",      f64,    {});

    /* ── collections: Stack ──────────────────────────────────────────── */
    getOrDeclareExtern(mod, ctx, "__visuall_stack_new",      i8p, {});
    getOrDeclareExtern(mod, ctx, "__visuall_stack_push",     voidTy, {i8p, i64});
    getOrDeclareExtern(mod, ctx, "__visuall_stack_pop",      i64, {i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_stack_peek",     i64, {i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_stack_is_empty", i64, {i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_stack_size",     i64, {i8p});

    /* ── collections: Queue ──────────────────────────────────────────── */
    getOrDeclareExtern(mod, ctx, "__visuall_queue_new",      i8p, {});
    getOrDeclareExtern(mod, ctx, "__visuall_queue_enqueue",  voidTy, {i8p, i64});
    getOrDeclareExtern(mod, ctx, "__visuall_queue_dequeue",  i64, {i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_queue_is_empty", i64, {i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_queue_size",     i64, {i8p});

    /* ── collections: Set ────────────────────────────────────────────── */
    getOrDeclareExtern(mod, ctx, "__visuall_set_new",       i8p, {});
    getOrDeclareExtern(mod, ctx, "__visuall_set_add",       voidTy, {i8p, i64});
    getOrDeclareExtern(mod, ctx, "__visuall_set_remove",    voidTy, {i8p, i64});
    getOrDeclareExtern(mod, ctx, "__visuall_set_contains",  i64, {i8p, i64});
    getOrDeclareExtern(mod, ctx, "__visuall_set_size",      i64, {i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_set_to_list",   i8p, {i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_set_union",     i8p, {i8p, i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_set_intersect", i8p, {i8p, i8p});

    /* ── GC runtime ──────────────────────────────────────────────────── */
    auto* i8 = llvm::Type::getInt8Ty(ctx);
    getOrDeclareExtern(mod, ctx, "__visuall_alloc",             i8p,    {i64, i8});
    getOrDeclareExtern(mod, ctx, "__visuall_alloc_object",      i8p,    {i64, i32, i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_register_global",   voidTy, {llvm::PointerType::getUnqual(i8p)});
    getOrDeclareExtern(mod, ctx, "__visuall_collect",           voidTy, {});
    getOrDeclareExtern(mod, ctx, "__visuall_gc_init",           voidTy, {i8p});
    getOrDeclareExtern(mod, ctx, "__visuall_gc_shutdown",       voidTy, {});
    getOrDeclareExtern(mod, ctx, "__visuall_gc_enable_stats",   voidTy, {i32});
    /* ── Exception support ────────────────────────────────────────────── */
    // __visuall_exception_new: allocate a VisualException via __cxa_allocate_exception
    getOrDeclareExtern(mod, ctx, "__visuall_exception_new",          i8p,    {i8p});
    // __visuall_exception_msg: extract the message char* from an exception object
    getOrDeclareExtern(mod, ctx, "__visuall_exception_msg",          i8p,    {i8p});
    // __visuall_get_exception_typeinfo: returns &typeid(VisualException)
    getOrDeclareExtern(mod, ctx, "__visuall_get_exception_typeinfo", i8p,    {});

    // C++ ABI exception functions (provided by libstdc++)
    {
        auto* cxaThrow = getOrDeclareExtern(mod, ctx, "__cxa_throw",
                                            voidTy, {i8p, i8p, i8p});
        cxaThrow->addFnAttr(llvm::Attribute::NoReturn);
    }
    getOrDeclareExtern(mod, ctx, "__cxa_begin_catch", i8p,    {i8p});
    getOrDeclareExtern(mod, ctx, "__cxa_end_catch",   voidTy, {});}

} // namespace visuall
