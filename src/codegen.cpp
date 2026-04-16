#include "codegen.h"
#include "builtins.h"
#include "module_loader.h"

#include <fstream>
#include <iostream>
#include <cstdlib>

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace visuall {

// ════════════════════════════════════════════════════════════════════════════
// Constructor
// ════════════════════════════════════════════════════════════════════════════
Codegen::Codegen(const std::string& moduleName) {
    context_ = std::make_unique<llvm::LLVMContext>();
    module_  = std::make_unique<llvm::Module>(moduleName, *context_);
    builder_ = std::make_unique<llvm::IRBuilder<>>(*context_);
}

// ════════════════════════════════════════════════════════════════════════════
// Scope management
// ════════════════════════════════════════════════════════════════════════════
void Codegen::pushScope() {
    scopes_.emplace_back();
}

void Codegen::popScope() {
    if (!scopes_.empty()) scopes_.pop_back();
}

void Codegen::declareVar(const std::string& name, llvm::AllocaInst* alloca) {
    if (!scopes_.empty()) {
        scopes_.back()[name] = alloca;
    }
}

llvm::AllocaInst* Codegen::lookupVar(const std::string& name) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return found->second;
    }
    return nullptr;
}

// ════════════════════════════════════════════════════════════════════════════
// Entry-block alloca helper — all allocas go in entry for mem2reg.
// ════════════════════════════════════════════════════════════════════════════
llvm::AllocaInst* Codegen::createEntryBlockAlloca(llvm::Function* fn,
                                                    const std::string& name,
                                                    llvm::Type* type) {
    llvm::IRBuilder<> tmpBuilder(&fn->getEntryBlock(),
                                  fn->getEntryBlock().begin());
    return tmpBuilder.CreateAlloca(type, nullptr, name);
}

// ════════════════════════════════════════════════════════════════════════════
// Type mapping
// ════════════════════════════════════════════════════════════════════════════
llvm::Type* Codegen::getLLVMType(const std::string& typeName) {
    if (typeName == "int")   return llvm::Type::getInt64Ty(*context_);
    if (typeName == "float") return llvm::Type::getDoubleTy(*context_);
    if (typeName == "bool")  return llvm::Type::getInt1Ty(*context_);
    if (typeName == "str" || typeName == "string")
        return llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context_));
    if (typeName == "void")  return llvm::Type::getVoidTy(*context_);
    // Function types: "(int)->int" etc. map to the closure struct type.
    if (!typeName.empty() && typeName[0] == '(') {
        return getClosureType();
    }
    // Default: i64
    return llvm::Type::getInt64Ty(*context_);
}

// ════════════════════════════════════════════════════════════════════════════
// Conversion helpers
// ════════════════════════════════════════════════════════════════════════════
llvm::Value* Codegen::promoteToDouble(llvm::Value* v) {
    if (v->getType()->isDoubleTy()) return v;
    if (v->getType()->isIntegerTy(64))
        return builder_->CreateSIToFP(v, llvm::Type::getDoubleTy(*context_), "promo");
    if (v->getType()->isIntegerTy(1))
        return builder_->CreateUIToFP(v, llvm::Type::getDoubleTy(*context_), "promo");
    return builder_->CreateSIToFP(v, llvm::Type::getDoubleTy(*context_), "promo");
}

llvm::Value* Codegen::toBool(llvm::Value* v) {
    if (v->getType()->isIntegerTy(1)) return v;
    if (v->getType()->isDoubleTy())
        return builder_->CreateFCmpONE(
            v, llvm::ConstantFP::get(*context_, llvm::APFloat(0.0)), "tobool");
    if (v->getType()->isIntegerTy())
        return builder_->CreateICmpNE(
            v, llvm::ConstantInt::get(v->getType(), 0), "tobool");
    // Pointer — compare to null
    if (v->getType()->isPointerTy())
        return builder_->CreateICmpNE(
            v, llvm::ConstantPointerNull::get(
                llvm::cast<llvm::PointerType>(v->getType())), "tobool");
    return builder_->CreateICmpNE(
        v, llvm::ConstantInt::get(v->getType(), 0), "tobool");
}

// ════════════════════════════════════════════════════════════════════════════
// Runtime builtins: printf declaration + print() shim
// ════════════════════════════════════════════════════════════════════════════
void Codegen::declarePrintfAndBuiltins() {
    // int printf(const char* fmt, ...)
    auto* i8Ptr = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context_));
    auto* printfType = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(*context_), {i8Ptr}, /*isVarArg=*/true);
    llvm::Function::Create(printfType, llvm::Function::ExternalLinkage,
                           "printf", module_.get());

    // Declare all Visuall runtime functions.
    declareRuntimeFunctions(*module_, *context_);
}

// Emit a print() call → routes each argument through the appropriate
// __visuall_print_* runtime function.
llvm::Value* Codegen::emitPrintCall(const ast::CallExpr& node) {
    for (size_t i = 0; i < node.args.size(); i++) {
        if (i > 0) {
            // Print a space between arguments
            auto* printStr = module_->getFunction("__visuall_print_str");
            if (printStr) {
                auto* space = builder_->CreateGlobalString(" ", "space");
                builder_->CreateCall(printStr, {space});
            }
        }
        llvm::Value* val = codegenExpr(*node.args[i]);

        if (val->getType()->isIntegerTy(64)) {
            auto* fn = module_->getFunction("__visuall_print_int");
            if (fn) builder_->CreateCall(fn, {val});
        } else if (val->getType()->isDoubleTy()) {
            auto* fn = module_->getFunction("__visuall_print_float");
            if (fn) builder_->CreateCall(fn, {val});
        } else if (val->getType()->isIntegerTy(1)) {
            auto* ext = builder_->CreateZExt(val, llvm::Type::getInt64Ty(*context_), "bext");
            auto* fn = module_->getFunction("__visuall_print_bool");
            if (fn) builder_->CreateCall(fn, {ext});
        } else if (val->getType()->isPointerTy()) {
            auto* fn = module_->getFunction("__visuall_print_str");
            if (fn) builder_->CreateCall(fn, {val});
        } else {
            // Fallback: printf
            auto* printfFn = module_->getFunction("printf");
            if (printfFn) {
                auto* fmt = builder_->CreateGlobalString("%lld", "fmt_other");
                if (val->getType()->isIntegerTy())
                    val = builder_->CreateSExt(val, llvm::Type::getInt64Ty(*context_), "sext");
                builder_->CreateCall(printfFn, {fmt, val});
            }
        }
    }

    // Print a newline
    auto* nlFn = module_->getFunction("__visuall_print_newline");
    if (nlFn) {
        builder_->CreateCall(nlFn, {});
    } else {
        auto* printfFn = module_->getFunction("printf");
        if (printfFn) {
            auto* nl = builder_->CreateGlobalString("\n", "nl");
            builder_->CreateCall(printfFn, {nl});
        }
    }

    return llvm::ConstantInt::get(*context_, llvm::APInt(64, 0));
}

// ════════════════════════════════════════════════════════════════════════════
// Integer power: base ** exp via loop (for non-negative integer exponents)
// ════════════════════════════════════════════════════════════════════════════
llvm::Value* Codegen::emitIntPow(llvm::Value* base, llvm::Value* exp) {
    auto* fn = builder_->GetInsertBlock()->getParent();
    auto* i64Ty = llvm::Type::getInt64Ty(*context_);

    auto* preheader = builder_->GetInsertBlock();
    auto* loopBB    = llvm::BasicBlock::Create(*context_, "pow.loop", fn);
    auto* doneBB    = llvm::BasicBlock::Create(*context_, "pow.done", fn);

    builder_->CreateBr(loopBB);

    // Loop: result = result * base, count--
    builder_->SetInsertPoint(loopBB);
    auto* phiResult = builder_->CreatePHI(i64Ty, 2, "pow.result");
    auto* phiCount  = builder_->CreatePHI(i64Ty, 2, "pow.count");

    phiResult->addIncoming(llvm::ConstantInt::get(i64Ty, 1), preheader);
    phiCount->addIncoming(exp, preheader);

    auto* isDone = builder_->CreateICmpSLE(
        phiCount, llvm::ConstantInt::get(i64Ty, 0), "pow.done.cmp");
    auto* bodyBB = llvm::BasicBlock::Create(*context_, "pow.body", fn);
    builder_->CreateCondBr(isDone, doneBB, bodyBB);

    builder_->SetInsertPoint(bodyBB);
    auto* newResult = builder_->CreateMul(phiResult, base, "pow.mul");
    auto* newCount  = builder_->CreateSub(
        phiCount, llvm::ConstantInt::get(i64Ty, 1), "pow.dec");
    builder_->CreateBr(loopBB);

    phiResult->addIncoming(newResult, bodyBB);
    phiCount->addIncoming(newCount, bodyBB);

    builder_->SetInsertPoint(doneBB);
    return phiResult;
}

// ════════════════════════════════════════════════════════════════════════════
// Top-level: generate()
// ════════════════════════════════════════════════════════════════════════════
void Codegen::generate(const ast::Program& program) {
    declarePrintfAndBuiltins();

    // First pass: declare all top-level functions so forward calls work.
    for (const auto& stmt : program.statements) {
        if (auto* f = dynamic_cast<const ast::FuncDef*>(stmt.get())) {
            // Skip generic functions — they are monomorphized at call sites.
            if (!f->typeParams.empty()) {
                genericFuncDefs_[f->name] = f;
                continue;
            }
            llvm::Type* retTy = f->returnType.empty()
                ? llvm::Type::getVoidTy(*context_)
                : getLLVMType(f->returnType);
            std::vector<llvm::Type*> paramTys;
            for (const auto& p : f->params)
                paramTys.push_back(
                    p.typeAnnotation.empty() ? llvm::Type::getInt64Ty(*context_)
                                             : getLLVMType(p.typeAnnotation));
            auto* fnTy = llvm::FunctionType::get(retTy, paramTys, false);
            llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage,
                                   f->name, module_.get());
        }
    }

    if (moduleMode_) {
        // Module mode: only compile top-level functions and classes.
        // No main() is created. Module-level variable initializations
        // are placed in a __init_<module> function.
        std::string initName = "__init_" + module_->getName().str();
        auto* initType = llvm::FunctionType::get(
            llvm::Type::getVoidTy(*context_), false);
        auto* initFn = llvm::Function::Create(
            initType, llvm::Function::ExternalLinkage, initName, module_.get());
        auto* entry = llvm::BasicBlock::Create(*context_, "entry", initFn);
        builder_->SetInsertPoint(entry);
        currentFunction_ = initFn;
        pushScope();

        bool hasNonFuncStmts = false;
        for (const auto& stmt : program.statements) {
            if (dynamic_cast<const ast::FuncDef*>(stmt.get()) ||
                dynamic_cast<const ast::ClassDef*>(stmt.get())) {
                auto* savedBB = builder_->GetInsertBlock();
                auto* savedFn = currentFunction_;
                codegenStmt(*stmt);
                currentFunction_ = savedFn;
                if (savedBB) builder_->SetInsertPoint(savedBB);
            } else {
                hasNonFuncStmts = true;
                codegenStmt(*stmt);
            }
        }

        if (!builder_->GetInsertBlock()->getTerminator()) {
            builder_->CreateRetVoid();
        }
        popScope();

        // If no non-function statements, remove the empty init function.
        if (!hasNonFuncStmts) {
            initFn->eraseFromParent();
        }
    } else {
        // Normal mode: create main() and wrap top-level code in it.
        auto* mainType = llvm::FunctionType::get(
            llvm::Type::getInt32Ty(*context_), false);
        auto* mainFn = llvm::Function::Create(
            mainType, llvm::Function::ExternalLinkage, "main", module_.get());
        auto* entry = llvm::BasicBlock::Create(*context_, "entry", mainFn);
        builder_->SetInsertPoint(entry);
        currentFunction_ = mainFn;
        pushScope();

        // ── GC init: __visuall_gc_init(&stack_anchor) ──────────────────
        {
            auto* i8Ptr = llvm::PointerType::getUnqual(
                llvm::Type::getInt8Ty(*context_));
            auto* voidTy = llvm::Type::getVoidTy(*context_);

            // Create a stack anchor alloca (address near bottom of the
            // C stack) and pass it to gc_init.
            auto* anchorTy = llvm::Type::getInt8Ty(*context_);
            auto* anchor = createEntryBlockAlloca(mainFn, "gc.anchor", anchorTy);
            auto* anchorPtr = builder_->CreateBitCast(anchor, i8Ptr, "gc.anchor.ptr");

            auto* gcInitTy = llvm::FunctionType::get(voidTy, {i8Ptr}, false);
            auto* gcInitFn = llvm::Function::Create(
                gcInitTy, llvm::Function::ExternalLinkage,
                "__visuall_gc_init", module_.get());
            builder_->CreateCall(gcInitFn, {anchorPtr});

            // If --gc-stats was requested, call __visuall_gc_enable_stats(1).
            auto* i32Ty = llvm::Type::getInt32Ty(*context_);
            auto* enableStatsTy = llvm::FunctionType::get(voidTy, {i32Ty}, false);
            auto* enableStatsFn = llvm::Function::Create(
                enableStatsTy, llvm::Function::ExternalLinkage,
                "__visuall_gc_enable_stats", module_.get());
            if (gcStatsEnabled_) {
                builder_->CreateCall(enableStatsFn,
                    {llvm::ConstantInt::get(i32Ty, 1)});
            }
        }

        // Second pass: codegen all statements.
        // Top-level FuncDefs/ClassDefs are emitted as separate functions;
        // other statements go into main().
        for (const auto& stmt : program.statements) {
            if (dynamic_cast<const ast::FuncDef*>(stmt.get()) ||
                dynamic_cast<const ast::ClassDef*>(stmt.get())) {
                // Save main context, emit function, restore.
                auto* savedBB = builder_->GetInsertBlock();
                auto* savedFn = currentFunction_;
                codegenStmt(*stmt);
                currentFunction_ = savedFn;
                if (savedBB) builder_->SetInsertPoint(savedBB);
            } else {
                codegenStmt(*stmt);
            }
        }

        // Return 0 from main (if not already terminated)
        if (!builder_->GetInsertBlock()->getTerminator()) {
            // ── GC shutdown before exit ────────────────────────────────
            auto* gcShutdownTy = llvm::FunctionType::get(
                llvm::Type::getVoidTy(*context_), false);
            auto* gcShutdownFn = module_->getFunction("__visuall_gc_shutdown");
            if (!gcShutdownFn) {
                gcShutdownFn = llvm::Function::Create(
                    gcShutdownTy, llvm::Function::ExternalLinkage,
                    "__visuall_gc_shutdown", module_.get());
            }
            builder_->CreateCall(gcShutdownFn, {});

            builder_->CreateRet(llvm::ConstantInt::get(*context_, llvm::APInt(32, 0)));
        }
        popScope();
    }

    // Verify
    std::string errStr;
    llvm::raw_string_ostream errStream(errStr);
    if (llvm::verifyModule(*module_, &errStream)) {
        throw std::runtime_error("LLVM module verification failed:\n" + errStr);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Optimization passes (new PassManager, LLVM 17+)
// ════════════════════════════════════════════════════════════════════════════
void Codegen::optimize() {
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

    // O2 pipeline includes mem2reg, instcombine, reassociate, gvn, simplifycfg.
    auto MPM = PB.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);
    MPM.run(*module_, MAM);
}

// ════════════════════════════════════════════════════════════════════════════
// Output
// ════════════════════════════════════════════════════════════════════════════
void Codegen::printIR(std::ostream& os) const {
    llvm::raw_os_ostream llvmOs(os);
    module_->print(llvmOs, nullptr);
}

void Codegen::writeIRToFile(const std::string& path) const {
    std::error_code ec;
    llvm::raw_fd_ostream out(path, ec);
    if (ec) {
        throw std::runtime_error("Could not open IR output file: " + path +
                                 ": " + ec.message());
    }
    module_->print(out, nullptr);
}

void Codegen::emitObjectFile(const std::string& path) const {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    auto triple = llvm::sys::getDefaultTargetTriple();
    module_->setTargetTriple(triple);

    std::string err;
    auto* target = llvm::TargetRegistry::lookupTarget(triple, err);
    if (!target) {
        throw std::runtime_error("TargetRegistry::lookupTarget: " + err);
    }

    llvm::TargetOptions opt;
    auto* TM = target->createTargetMachine(
        triple, "generic", "", opt, llvm::Reloc::PIC_);
    module_->setDataLayout(TM->createDataLayout());

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
    pass.run(*module_);
    dest.flush();
}

int Codegen::linkToBinary(const std::string& objPath,
                           const std::string& outPath) {
    // Link with the Visuall runtime library
    std::string cmd = "clang \"" + objPath + "\" -o \"" + outPath + "\" -lm";
    return std::system(cmd.c_str());
}

void Codegen::compileToNative(const std::string& basePath) const {
    std::string irPath  = basePath + ".ll";
    std::string objPath = basePath + ".o";
    writeIRToFile(irPath);
    emitObjectFile(objPath);
    int rc = linkToBinary(objPath, basePath);
    if (rc != 0) {
        throw std::runtime_error("Linker failed (exit code " +
                                 std::to_string(rc) + ")");
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Statement codegen dispatch
// ════════════════════════════════════════════════════════════════════════════
void Codegen::codegenStmtList(const ast::StmtList& stmts) {
    for (const auto& s : stmts) {
        codegenStmt(*s);
        // If the current block is already terminated (return/break/continue),
        // stop emitting into this block.
        if (builder_->GetInsertBlock()->getTerminator()) break;
    }
}

void Codegen::codegenStmt(const ast::Stmt& stmt) {
    if (auto* p = dynamic_cast<const ast::FuncDef*>(&stmt))
        return codegenFuncDef(*p);
    if (auto* p = dynamic_cast<const ast::ClassDef*>(&stmt))
        return codegenClassDef(*p);
    if (auto* p = dynamic_cast<const ast::InitDef*>(&stmt))
        return codegenInitDef(*p);
    if (auto* p = dynamic_cast<const ast::IfStmt*>(&stmt))
        return codegenIfStmt(*p);
    if (auto* p = dynamic_cast<const ast::ForStmt*>(&stmt))
        return codegenForStmt(*p);
    if (auto* p = dynamic_cast<const ast::WhileStmt*>(&stmt))
        return codegenWhileStmt(*p);
    if (auto* p = dynamic_cast<const ast::ReturnStmt*>(&stmt))
        return codegenReturnStmt(*p);
    if (auto* p = dynamic_cast<const ast::TryStmt*>(&stmt))
        return codegenTryStmt(*p);
    if (auto* p = dynamic_cast<const ast::ThrowStmt*>(&stmt))
        return codegenThrowStmt(*p);
    if (auto* p = dynamic_cast<const ast::ImportStmt*>(&stmt))
        return codegenImportStmt(*p);
    if (auto* p = dynamic_cast<const ast::FromImportStmt*>(&stmt))
        return codegenFromImportStmt(*p);
    if (auto* p = dynamic_cast<const ast::AssignStmt*>(&stmt))
        return codegenAssignStmt(*p);
    if (auto* p = dynamic_cast<const ast::ExprStmt*>(&stmt))
        return codegenExprStmt(*p);
    if (auto* p = dynamic_cast<const ast::TupleUnpackStmt*>(&stmt))
        return codegenTupleUnpackStmt(*p);
    if (auto* p = dynamic_cast<const ast::InterfaceDef*>(&stmt))
        return codegenInterfaceDef(*p);

    // break
    if (dynamic_cast<const ast::BreakStmt*>(&stmt)) {
        if (!loopStack_.empty()) {
            builder_->CreateBr(loopStack_.back().second); // exit block
        }
        return;
    }
    // continue
    if (dynamic_cast<const ast::ContinueStmt*>(&stmt)) {
        if (!loopStack_.empty()) {
            builder_->CreateBr(loopStack_.back().first); // header block
        }
        return;
    }
    // pass — no-op
}

// ════════════════════════════════════════════════════════════════════════════
// Statement implementations
// ════════════════════════════════════════════════════════════════════════════

// ── FuncDef ────────────────────────────────────────────────────────────────
void Codegen::codegenFuncDef(const ast::FuncDef& node) {
    // Skip generic functions — they are monomorphized at call sites.
    if (!node.typeParams.empty()) {
        genericFuncDefs_[node.name] = &node;
        return;
    }

    // Look up the pre-declared function.
    llvm::Function* fn = module_->getFunction(node.name);
    if (!fn) {
        // Not forward-declared — happens for nested/class functions.
        llvm::Type* retTy = node.returnType.empty()
            ? llvm::Type::getVoidTy(*context_)
            : getLLVMType(node.returnType);
        std::vector<llvm::Type*> paramTys;
        for (const auto& p : node.params)
            paramTys.push_back(
                p.typeAnnotation.empty() ? llvm::Type::getInt64Ty(*context_)
                                         : getLLVMType(p.typeAnnotation));
        auto* fnTy = llvm::FunctionType::get(retTy, paramTys, false);
        fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage,
                                    node.name, module_.get());
    }

    // Name parameters
    size_t idx = 0;
    for (auto& arg : fn->args()) {
        if (idx < node.params.size())
            arg.setName(node.params[idx++].name);
    }

    auto* savedBB = builder_->GetInsertBlock();
    auto* savedFn = currentFunction_;

    auto* bb = llvm::BasicBlock::Create(*context_, "entry", fn);
    builder_->SetInsertPoint(bb);
    currentFunction_ = fn;
    pushScope();

    // Alloca + store for each parameter
    for (auto& arg : fn->args()) {
        auto* alloca = createEntryBlockAlloca(fn, std::string(arg.getName()),
                                               arg.getType());
        builder_->CreateStore(&arg, alloca);
        declareVar(std::string(arg.getName()), alloca);
    }

    // Track parameters with function type annotations as closure variables.
    for (const auto& p : node.params) {
        if (!p.typeAnnotation.empty() && p.typeAnnotation[0] == '(') {
            closureVars_.insert(p.name);
        }
    }

    // Default parameter values are handled at call sites for now.
    // Decorators are applied conceptually but emit no extra IR (stubs).

    codegenStmtList(node.body);

    // Implicit return if not terminated
    if (!builder_->GetInsertBlock()->getTerminator()) {
        if (fn->getReturnType()->isVoidTy()) {
            builder_->CreateRetVoid();
        } else {
            builder_->CreateRet(llvm::Constant::getNullValue(fn->getReturnType()));
        }
    }

    popScope();
    currentFunction_ = savedFn;
    if (savedBB) builder_->SetInsertPoint(savedBB);
}

// ── ClassDef ───────────────────────────────────────────────────────────────
void Codegen::codegenClassDef(const ast::ClassDef& node) {
    std::string savedClass = currentClassName_;
    currentClassName_ = node.name;

    for (const auto& s : node.body) {
        if (auto* m = dynamic_cast<const ast::FuncDef*>(s.get())) {
            // Emit class methods as ClassName_methodName
            // Build a modified FuncDef with the prefixed name and an implicit
            // i8* this parameter.
            std::vector<ast::Param> params;
            ast::Param thisParam;
            thisParam.name = "this";
            thisParam.typeAnnotation = "str"; // i8* for now
            params.push_back(std::move(thisParam));
            for (const auto& p : m->params) {
                ast::Param cp;
                cp.name = p.name;
                cp.typeAnnotation = p.typeAnnotation;
                cp.isVariadic = p.isVariadic;
                // defaultValue is not copied (not needed for codegen)
                params.push_back(std::move(cp));
            }

            ast::FuncDef prefixed(
                node.name + "_" + m->name,
                std::move(params),
                m->returnType,
                {} // empty body — we'll codegen manually
            );

            // Declare
            llvm::Type* retTy = m->returnType.empty()
                ? llvm::Type::getVoidTy(*context_)
                : getLLVMType(m->returnType);
            std::vector<llvm::Type*> paramTys;
            paramTys.push_back(
                llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context_)));
            for (const auto& p : m->params)
                paramTys.push_back(
                    p.typeAnnotation.empty() ? llvm::Type::getInt64Ty(*context_)
                                             : getLLVMType(p.typeAnnotation));
            auto* fnTy = llvm::FunctionType::get(retTy, paramTys, false);
            auto* fn = llvm::Function::Create(
                fnTy, llvm::Function::ExternalLinkage,
                node.name + "_" + m->name, module_.get());

            // Name params
            auto argIt = fn->arg_begin();
            argIt->setName("this"); ++argIt;
            for (size_t i = 0; i < m->params.size() && argIt != fn->arg_end();
                 ++i, ++argIt) {
                argIt->setName(m->params[i].name);
            }

            auto* savedBB = builder_->GetInsertBlock();
            auto* savedFn = currentFunction_;

            auto* bb = llvm::BasicBlock::Create(*context_, "entry", fn);
            builder_->SetInsertPoint(bb);
            currentFunction_ = fn;
            pushScope();

            for (auto& arg : fn->args()) {
                auto* alloca = createEntryBlockAlloca(
                    fn, std::string(arg.getName()), arg.getType());
                builder_->CreateStore(&arg, alloca);
                declareVar(std::string(arg.getName()), alloca);
            }

            codegenStmtList(m->body);

            if (!builder_->GetInsertBlock()->getTerminator()) {
                if (fn->getReturnType()->isVoidTy()) builder_->CreateRetVoid();
                else builder_->CreateRet(llvm::Constant::getNullValue(fn->getReturnType()));
            }

            popScope();
            currentFunction_ = savedFn;
            if (savedBB) builder_->SetInsertPoint(savedBB);
        } else if (auto* init = dynamic_cast<const ast::InitDef*>(s.get())) {
            // Emit as ClassName_init
            (void)init;
            codegenInitDef(*init);
        } else {
            codegenStmt(*s);
        }
    }

    currentClassName_ = savedClass;
}

// ── InitDef ────────────────────────────────────────────────────────────────
void Codegen::codegenInitDef(const ast::InitDef& node) {
    std::string name = currentClassName_.empty()
                           ? "__init"
                           : currentClassName_ + "_init";

    auto* i8Ptr = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context_));
    std::vector<llvm::Type*> paramTys = {i8Ptr}; // this
    for (const auto& p : node.params)
        paramTys.push_back(
            p.typeAnnotation.empty() ? llvm::Type::getInt64Ty(*context_)
                                     : getLLVMType(p.typeAnnotation));

    auto* fnTy = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*context_), paramTys, false);
    auto* fn = llvm::Function::Create(
        fnTy, llvm::Function::ExternalLinkage, name, module_.get());

    auto argIt = fn->arg_begin();
    argIt->setName("this"); ++argIt;
    for (size_t i = 0; i < node.params.size() && argIt != fn->arg_end();
         ++i, ++argIt) {
        argIt->setName(node.params[i].name);
    }

    auto* savedBB = builder_->GetInsertBlock();
    auto* savedFn = currentFunction_;

    auto* bb = llvm::BasicBlock::Create(*context_, "entry", fn);
    builder_->SetInsertPoint(bb);
    currentFunction_ = fn;
    pushScope();

    for (auto& arg : fn->args()) {
        auto* alloca = createEntryBlockAlloca(
            fn, std::string(arg.getName()), arg.getType());
        builder_->CreateStore(&arg, alloca);
        declareVar(std::string(arg.getName()), alloca);
    }

    codegenStmtList(node.body);

    if (!builder_->GetInsertBlock()->getTerminator())
        builder_->CreateRetVoid();

    popScope();
    currentFunction_ = savedFn;
    if (savedBB) builder_->SetInsertPoint(savedBB);
}

// ── IfStmt ─────────────────────────────────────────────────────────────────
void Codegen::codegenIfStmt(const ast::IfStmt& node) {
    llvm::Value* condVal = toBool(codegenExpr(*node.condition));
    auto* fn = builder_->GetInsertBlock()->getParent();

    auto* thenBB  = llvm::BasicBlock::Create(*context_, "if.then", fn);
    auto* mergeBB = llvm::BasicBlock::Create(*context_, "if.end");

    // Build the else/elsif chain backwards to know where to branch.
    llvm::BasicBlock* elseBB = nullptr;
    if (!node.elsifBranches.empty() || !node.elseBranch.empty()) {
        elseBB = llvm::BasicBlock::Create(*context_, "if.else");
    }

    builder_->CreateCondBr(condVal, thenBB, elseBB ? elseBB : mergeBB);

    // ── then ───────────────────────────────────────────────────────────
    builder_->SetInsertPoint(thenBB);
    pushScope();
    codegenStmtList(node.thenBranch);
    popScope();
    if (!builder_->GetInsertBlock()->getTerminator())
        builder_->CreateBr(mergeBB);

    // ── elsif / else ───────────────────────────────────────────────────
    if (elseBB) {
        fn->insert(fn->end(), elseBB);
        builder_->SetInsertPoint(elseBB);

        if (!node.elsifBranches.empty()) {
            for (size_t i = 0; i < node.elsifBranches.size(); i++) {
                auto& [cond, body] = node.elsifBranches[i];
                llvm::Value* ec = toBool(codegenExpr(*cond));

                auto* eifThen = llvm::BasicBlock::Create(*context_, "elsif.then", fn);
                llvm::BasicBlock* nextBB = nullptr;
                if (i + 1 < node.elsifBranches.size() || !node.elseBranch.empty()) {
                    nextBB = llvm::BasicBlock::Create(*context_, "elsif.else");
                } else {
                    nextBB = mergeBB;
                }

                builder_->CreateCondBr(ec, eifThen, nextBB);

                builder_->SetInsertPoint(eifThen);
                pushScope();
                codegenStmtList(body);
                popScope();
                if (!builder_->GetInsertBlock()->getTerminator())
                    builder_->CreateBr(mergeBB);

                if (nextBB != mergeBB) {
                    fn->insert(fn->end(), nextBB);
                    builder_->SetInsertPoint(nextBB);
                }
            }

            if (!node.elseBranch.empty()) {
                pushScope();
                codegenStmtList(node.elseBranch);
                popScope();
                if (!builder_->GetInsertBlock()->getTerminator())
                    builder_->CreateBr(mergeBB);
            }
        } else {
            pushScope();
            codegenStmtList(node.elseBranch);
            popScope();
            if (!builder_->GetInsertBlock()->getTerminator())
                builder_->CreateBr(mergeBB);
        }
    }

    fn->insert(fn->end(), mergeBB);
    builder_->SetInsertPoint(mergeBB);
}

// ── ForStmt ────────────────────────────────────────────────────────────────
// for x in iterable → iterate over a list or range
void Codegen::codegenForStmt(const ast::ForStmt& node) {
    auto* fn = builder_->GetInsertBlock()->getParent();
    auto* i64Ty = llvm::Type::getInt64Ty(*context_);

    // Evaluate the iterable.
    llvm::Value* iterVal = codegenExpr(*node.iterable);

    // If the iterable is a pointer (list), iterate using list_len and list_get.
    if (iterVal->getType()->isPointerTy()) {
        auto* listLenFn = module_->getFunction("__visuall_list_len");
        auto* listGetFn = module_->getFunction("__visuall_list_get");

        llvm::Value* bound;
        if (listLenFn) {
            bound = builder_->CreateCall(listLenFn, {iterVal}, "list.len");
        } else {
            bound = llvm::ConstantInt::get(i64Ty, 0);
        }

        auto* idxAlloca = createEntryBlockAlloca(currentFunction_, "__for_idx", i64Ty);
        builder_->CreateStore(llvm::ConstantInt::get(i64Ty, 0), idxAlloca);

        auto* condBB = llvm::BasicBlock::Create(*context_, "for.cond", fn);
        auto* bodyBB = llvm::BasicBlock::Create(*context_, "for.body", fn);
        auto* exitBB = llvm::BasicBlock::Create(*context_, "for.end", fn);

        builder_->CreateBr(condBB);

        builder_->SetInsertPoint(condBB);
        auto* idx = builder_->CreateLoad(i64Ty, idxAlloca, "idx");
        auto* cmp = builder_->CreateICmpSLT(idx, bound, "for.cmp");
        builder_->CreateCondBr(cmp, bodyBB, exitBB);

        builder_->SetInsertPoint(bodyBB);
        pushScope();

        // Get element from list
        auto* curIdx = builder_->CreateLoad(i64Ty, idxAlloca, "cur_idx");
        llvm::Value* elemVal;
        if (listGetFn) {
            elemVal = builder_->CreateCall(listGetFn, {iterVal, curIdx}, "elem");
        } else {
            elemVal = curIdx;
        }

        auto* varAlloca = createEntryBlockAlloca(currentFunction_, node.variable, i64Ty);
        builder_->CreateStore(elemVal, varAlloca);
        declareVar(node.variable, varAlloca);

        loopStack_.push_back({condBB, exitBB});
        codegenStmtList(node.body);
        loopStack_.pop_back();

        popScope();

        if (!builder_->GetInsertBlock()->getTerminator()) {
            auto* next = builder_->CreateAdd(
                builder_->CreateLoad(i64Ty, idxAlloca, "idx"),
                llvm::ConstantInt::get(i64Ty, 1), "inc");
            builder_->CreateStore(next, idxAlloca);
            builder_->CreateBr(condBB);
        }

        builder_->SetInsertPoint(exitBB);
        return;
    }

    // Fallback: treat iterVal as integer upper bound (like range(N)).
    llvm::Value* bound = iterVal;
    if (!bound->getType()->isIntegerTy(64)) {
        bound = llvm::ConstantInt::get(i64Ty, 0);
    }

    auto* idxAlloca = createEntryBlockAlloca(currentFunction_, "__for_idx", i64Ty);
    builder_->CreateStore(llvm::ConstantInt::get(i64Ty, 0), idxAlloca);

    auto* condBB = llvm::BasicBlock::Create(*context_, "for.cond", fn);
    auto* bodyBB = llvm::BasicBlock::Create(*context_, "for.body", fn);
    auto* exitBB = llvm::BasicBlock::Create(*context_, "for.end", fn);

    builder_->CreateBr(condBB);

    // Condition: idx < bound
    builder_->SetInsertPoint(condBB);
    auto* idx = builder_->CreateLoad(i64Ty, idxAlloca, "idx");
    auto* cmp = builder_->CreateICmpSLT(idx, bound, "for.cmp");
    builder_->CreateCondBr(cmp, bodyBB, exitBB);

    // Body
    builder_->SetInsertPoint(bodyBB);
    pushScope();

    auto* varAlloca = createEntryBlockAlloca(currentFunction_, node.variable, i64Ty);
    auto* curIdx = builder_->CreateLoad(i64Ty, idxAlloca, "cur_idx");
    builder_->CreateStore(curIdx, varAlloca);
    declareVar(node.variable, varAlloca);

    loopStack_.push_back({condBB, exitBB});
    codegenStmtList(node.body);
    loopStack_.pop_back();

    popScope();

    // Increment index
    if (!builder_->GetInsertBlock()->getTerminator()) {
        auto* next = builder_->CreateAdd(
            builder_->CreateLoad(i64Ty, idxAlloca, "idx"),
            llvm::ConstantInt::get(i64Ty, 1), "inc");
        builder_->CreateStore(next, idxAlloca);
        builder_->CreateBr(condBB);
    }

    builder_->SetInsertPoint(exitBB);
}

// ── WhileStmt ──────────────────────────────────────────────────────────────
void Codegen::codegenWhileStmt(const ast::WhileStmt& node) {
    auto* fn = builder_->GetInsertBlock()->getParent();
    auto* condBB = llvm::BasicBlock::Create(*context_, "while.cond", fn);
    auto* bodyBB = llvm::BasicBlock::Create(*context_, "while.body", fn);
    auto* exitBB = llvm::BasicBlock::Create(*context_, "while.end", fn);

    builder_->CreateBr(condBB);

    builder_->SetInsertPoint(condBB);
    llvm::Value* condVal = toBool(codegenExpr(*node.condition));
    builder_->CreateCondBr(condVal, bodyBB, exitBB);

    builder_->SetInsertPoint(bodyBB);
    pushScope();
    loopStack_.push_back({condBB, exitBB});
    codegenStmtList(node.body);
    loopStack_.pop_back();
    popScope();
    if (!builder_->GetInsertBlock()->getTerminator())
        builder_->CreateBr(condBB);

    builder_->SetInsertPoint(exitBB);
}

// ── ReturnStmt ─────────────────────────────────────────────────────────────
void Codegen::codegenReturnStmt(const ast::ReturnStmt& node) {
    if (node.value) {
        llvm::Value* val = codegenExpr(*node.value);
        auto* retTy = currentFunction_->getReturnType();
        // Type coercion: if function returns double and value is int, promote.
        if (retTy->isDoubleTy() && val->getType()->isIntegerTy()) {
            val = promoteToDouble(val);
        }
        // If function returns i32 (main) and value is i64, truncate.
        if (retTy->isIntegerTy(32) && val->getType()->isIntegerTy(64)) {
            val = builder_->CreateTrunc(val, retTy, "trunc");
        }
        builder_->CreateRet(val);
    } else {
        builder_->CreateRetVoid();
    }
}

// ── TryStmt ────────────────────────────────────────────────────────────────
void Codegen::codegenTryStmt(const ast::TryStmt& node) {
    // Exceptions require a full personality function and landing pads.
    // For now: emit try body; on catch/finally, just emit the body.
    pushScope();
    codegenStmtList(node.tryBody);
    popScope();
    // Catch and finally are emitted linearly (no actual unwinding).
    for (const auto& c : node.catchClauses) {
        pushScope();
        codegenStmtList(c.body);
        popScope();
    }
    if (!node.finallyBody.empty()) {
        pushScope();
        codegenStmtList(node.finallyBody);
        popScope();
    }
}

// ── ThrowStmt ──────────────────────────────────────────────────────────────
void Codegen::codegenThrowStmt(const ast::ThrowStmt& node) {
    // Stub: evaluate the expression (for side effects), then trap.
    codegenExpr(*node.expr);
    builder_->CreateUnreachable();
}

// ── Imports ────────────────────────────────────────────────────────────────
void Codegen::codegenImportStmt(const ast::ImportStmt& node) {
    // Track imported modules for module-qualified call dispatch.
    if (isStdlibModule(node.module)) {
        importedModules_.insert(node.module);
        return;
    }

    // File-based module: use the module loader if available.
    if (moduleLoader_) {
        try {
            Module& mod = moduleLoader_->load(node.module, sourceFile_);
            importedModules_.insert(node.module);
            loadedUserModules_.insert(node.module);

            // Declare external function stubs for each exported function
            // so that call sites in this module can reference them.
            for (const auto& [name, sym] : mod.exports) {
                if (sym.kind == Symbol::Function) {
                    // Declare as external: we'll link later.
                    // For now, declare as i64(...) variadic.
                    auto* fnType = llvm::FunctionType::get(
                        llvm::Type::getInt64Ty(*context_), {}, true);
                    module_->getOrInsertFunction(sym.mangledName, fnType);
                }
            }
        } catch (const ImportError& e) {
            throw CodegenError(e.what(), node.line, node.column);
        }
    }
}

void Codegen::codegenFromImportStmt(const ast::FromImportStmt& node) {
    // Track imported modules.
    if (isStdlibModule(node.module)) {
        importedModules_.insert(node.module);
        return;
    }

    // File-based module: use the module loader if available.
    if (moduleLoader_) {
        try {
            Module& mod = moduleLoader_->load(node.module, sourceFile_);
            importedModules_.insert(node.module);
            loadedUserModules_.insert(node.module);

            // Inject requested symbols into scope.
            for (const auto& name : node.names) {
                auto it = mod.exports.find(name);
                if (it == mod.exports.end()) {
                    throw CodegenError(
                        "'" + name + "' is not exported by module '" +
                        node.module + "'", node.line, node.column);
                }
                const Symbol& sym = it->second;
                importedSymbols_[name] = {node.module, sym.mangledName};

                if (sym.kind == Symbol::Function) {
                    // Declare external function so calls resolve.
                    auto* fnType = llvm::FunctionType::get(
                        llvm::Type::getInt64Ty(*context_), {}, true);
                    module_->getOrInsertFunction(sym.mangledName, fnType);
                }
            }
        } catch (const ImportError& e) {
            throw CodegenError(e.what(), node.line, node.column);
        }
    }
}

// ── AssignStmt ─────────────────────────────────────────────────────────────
void Codegen::codegenAssignStmt(const ast::AssignStmt& node) {
    llvm::Value* val = codegenExpr(*node.value);

    if (auto* ident = dynamic_cast<const ast::Identifier*>(node.target.get())) {
        // Track whether the RHS is a closure (fat pointer).
        if (val->getType() == getClosureType()) {
            closureVars_.insert(ident->name);
        }

        auto* existing = lookupVar(ident->name);
        if (existing) {
            // If the alloca type matches, store directly.
            if (existing->getAllocatedType() == val->getType()) {
                builder_->CreateStore(val, existing);
            } else {
                // Type changed — create a new alloca.
                auto* alloca = createEntryBlockAlloca(
                    currentFunction_, ident->name, val->getType());
                builder_->CreateStore(val, alloca);
                declareVar(ident->name, alloca);
            }
        } else {
            auto* alloca = createEntryBlockAlloca(
                currentFunction_, ident->name, val->getType());
            builder_->CreateStore(val, alloca);
            declareVar(ident->name, alloca);
        }
    }
    // TODO: member/index assignment
}

// ── ExprStmt ───────────────────────────────────────────────────────────────
void Codegen::codegenExprStmt(const ast::ExprStmt& node) {
    codegenExpr(*node.expr);
}

// ════════════════════════════════════════════════════════════════════════════
// Expression codegen dispatch
// ════════════════════════════════════════════════════════════════════════════
llvm::Value* Codegen::codegenExpr(const ast::Expr& expr) {
    if (auto* p = dynamic_cast<const ast::IntLiteral*>(&expr))
        return codegenIntLiteral(*p);
    if (auto* p = dynamic_cast<const ast::FloatLiteral*>(&expr))
        return codegenFloatLiteral(*p);
    if (auto* p = dynamic_cast<const ast::StringLiteral*>(&expr))
        return codegenStringLiteral(*p);
    if (auto* p = dynamic_cast<const ast::FStringLiteral*>(&expr))
        return codegenFStringLiteral(*p);
    if (auto* p = dynamic_cast<const ast::FStringExpr*>(&expr))
        return codegenFStringExpr(*p);
    if (auto* p = dynamic_cast<const ast::BoolLiteral*>(&expr))
        return codegenBoolLiteral(*p);
    if (auto* p = dynamic_cast<const ast::NullLiteral*>(&expr))
        return codegenNullLiteral(*p);
    if (auto* p = dynamic_cast<const ast::Identifier*>(&expr))
        return codegenIdentifier(*p);
    if (auto* p = dynamic_cast<const ast::BinaryExpr*>(&expr))
        return codegenBinaryExpr(*p);
    if (auto* p = dynamic_cast<const ast::UnaryExpr*>(&expr))
        return codegenUnaryExpr(*p);
    if (auto* p = dynamic_cast<const ast::CallExpr*>(&expr))
        return codegenCallExpr(*p);
    if (auto* p = dynamic_cast<const ast::MemberExpr*>(&expr))
        return codegenMemberExpr(*p);
    if (auto* p = dynamic_cast<const ast::IndexExpr*>(&expr))
        return codegenIndexExpr(*p);
    if (auto* p = dynamic_cast<const ast::LambdaExpr*>(&expr))
        return codegenLambdaExpr(*p);
    if (auto* p = dynamic_cast<const ast::ListExpr*>(&expr))
        return codegenListExpr(*p);
    if (auto* p = dynamic_cast<const ast::DictExpr*>(&expr))
        return codegenDictExpr(*p);
    if (auto* p = dynamic_cast<const ast::TupleExpr*>(&expr))
        return codegenTupleExpr(*p);
    if (auto* p = dynamic_cast<const ast::TernaryExpr*>(&expr))
        return codegenTernaryExpr(*p);
    if (auto* p = dynamic_cast<const ast::SliceExpr*>(&expr))
        return codegenSliceExpr(*p);
    if (auto* p = dynamic_cast<const ast::ListComprehension*>(&expr))
        return codegenListComprehension(*p);
    if (auto* p = dynamic_cast<const ast::DictComprehension*>(&expr))
        return codegenDictComprehension(*p);
    if (auto* p = dynamic_cast<const ast::SpreadExpr*>(&expr))
        return codegenSpreadExpr(*p);
    if (dynamic_cast<const ast::ThisExpr*>(&expr)) {
        auto* thisAlloca = lookupVar("this");
        if (thisAlloca) {
            return builder_->CreateLoad(thisAlloca->getAllocatedType(),
                                         thisAlloca, "this");
        }
        return llvm::ConstantPointerNull::get(
            llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context_)));
    }
    if (dynamic_cast<const ast::SuperExpr*>(&expr)) {
        // super is effectively this — method dispatch is resolved statically.
        auto* thisAlloca = lookupVar("this");
        if (thisAlloca) {
            return builder_->CreateLoad(thisAlloca->getAllocatedType(),
                                         thisAlloca, "this");
        }
        return llvm::ConstantPointerNull::get(
            llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context_)));
    }

    return llvm::ConstantInt::get(*context_, llvm::APInt(64, 0));
}

// ════════════════════════════════════════════════════════════════════════════
// Expression implementations
// ════════════════════════════════════════════════════════════════════════════

llvm::Value* Codegen::codegenIntLiteral(const ast::IntLiteral& node) {
    return llvm::ConstantInt::get(*context_,
        llvm::APInt(64, static_cast<uint64_t>(node.value), true));
}

llvm::Value* Codegen::codegenFloatLiteral(const ast::FloatLiteral& node) {
    return llvm::ConstantFP::get(*context_, llvm::APFloat(node.value));
}

llvm::Value* Codegen::codegenStringLiteral(const ast::StringLiteral& node) {
    return builder_->CreateGlobalString(node.value, "str");
}

llvm::Value* Codegen::codegenFStringLiteral(const ast::FStringLiteral& node) {
    // Legacy fallback: treat f-strings as plain string constants.
    // Full interpolation is handled by FStringExpr.
    return builder_->CreateGlobalString(node.raw, "fstr");
}

llvm::Value* Codegen::codegenFStringExpr(const ast::FStringExpr& node) {
    auto* i8Ptr = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context_));
    auto* concatFn = module_->getFunction("__visuall_str_concat");
    auto* intToStrFn = module_->getFunction("__visuall_int_to_str");
    auto* floatToStrFn = module_->getFunction("__visuall_float_to_str");
    auto* boolToStrFn = module_->getFunction("__visuall_bool_to_str");

    // Start with empty string
    llvm::Value* result = builder_->CreateGlobalString("", "fstr.empty");

    for (const auto& part : node.parts) {
        llvm::Value* partStr = nullptr;
        if (!part.isExpr) {
            // Literal part
            partStr = builder_->CreateGlobalString(part.literal, "fstr.lit");
        } else {
            // Expression part — evaluate and convert to string
            llvm::Value* val = codegenExpr(*part.expr);
            if (val->getType()->isPointerTy()) {
                // Already a string (char*)
                partStr = val;
            } else if (val->getType()->isIntegerTy(64)) {
                if (intToStrFn)
                    partStr = builder_->CreateCall(intToStrFn, {val}, "fstr.int");
                else
                    partStr = builder_->CreateGlobalString("<int>", "fstr.stub");
            } else if (val->getType()->isDoubleTy()) {
                if (floatToStrFn)
                    partStr = builder_->CreateCall(floatToStrFn, {val}, "fstr.float");
                else
                    partStr = builder_->CreateGlobalString("<float>", "fstr.stub");
            } else if (val->getType()->isIntegerTy(1)) {
                auto* ext = builder_->CreateZExt(val, llvm::Type::getInt64Ty(*context_), "bext");
                if (boolToStrFn)
                    partStr = builder_->CreateCall(boolToStrFn, {ext}, "fstr.bool");
                else
                    partStr = builder_->CreateGlobalString("<bool>", "fstr.stub");
            } else {
                // Fallback: treat as int
                if (val->getType()->isIntegerTy()) {
                    val = builder_->CreateSExt(val, llvm::Type::getInt64Ty(*context_), "sext");
                    if (intToStrFn)
                        partStr = builder_->CreateCall(intToStrFn, {val}, "fstr.int");
                    else
                        partStr = builder_->CreateGlobalString("<value>", "fstr.stub");
                } else {
                    partStr = builder_->CreateGlobalString("<value>", "fstr.stub");
                }
            }
        }

        if (concatFn && partStr) {
            result = builder_->CreateCall(concatFn, {result, partStr}, "fstr.cat");
        }
    }

    return result;
}

llvm::Value* Codegen::codegenBoolLiteral(const ast::BoolLiteral& node) {
    return llvm::ConstantInt::get(*context_, llvm::APInt(1, node.value ? 1 : 0));
}

llvm::Value* Codegen::codegenNullLiteral(const ast::NullLiteral& /*node*/) {
    return llvm::ConstantPointerNull::get(
        llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context_)));
}

llvm::Value* Codegen::codegenIdentifier(const ast::Identifier& node) {
    auto* alloca = lookupVar(node.name);
    if (alloca) {
        return builder_->CreateLoad(alloca->getAllocatedType(),
                                     alloca, node.name);
    }
    // Check for a global function (for first-class function references).
    auto* fn = module_->getFunction(node.name);
    if (fn) return fn;

    throw CodegenError("Unknown variable: " + node.name, node.line, node.column);
}

// ── BinaryExpr ─────────────────────────────────────────────────────────────
llvm::Value* Codegen::codegenBinaryExpr(const ast::BinaryExpr& node) {
    llvm::Value* L = codegenExpr(*node.left);
    llvm::Value* R = codegenExpr(*node.right);

    bool isFloat = L->getType()->isDoubleTy() || R->getType()->isDoubleTy();

    // ── Float operations (with promotion) ──────────────────────────────
    if (isFloat) {
        L = promoteToDouble(L);
        R = promoteToDouble(R);

        switch (node.op) {
            case ast::BinOp::Add: return builder_->CreateFAdd(L, R, "fadd");
            case ast::BinOp::Sub: return builder_->CreateFSub(L, R, "fsub");
            case ast::BinOp::Mul: return builder_->CreateFMul(L, R, "fmul");
            case ast::BinOp::Div: return builder_->CreateFDiv(L, R, "fdiv");
            case ast::BinOp::Mod: return builder_->CreateFRem(L, R, "fmod");
            case ast::BinOp::Pow: {
                // Use llvm.pow.f64 intrinsic
                auto* powFn = llvm::Intrinsic::getOrInsertDeclaration(
                    module_.get(), llvm::Intrinsic::pow,
                    {llvm::Type::getDoubleTy(*context_)});
                return builder_->CreateCall(powFn, {L, R}, "fpow");
            }
            case ast::BinOp::IntDiv: {
                // Float integer division: fptosi(fdiv) then sitofp back? No — return int.
                auto* div = builder_->CreateFDiv(L, R, "fdiv");
                return builder_->CreateFPToSI(div, llvm::Type::getInt64Ty(*context_), "idiv");
            }
            case ast::BinOp::Eq:  return builder_->CreateFCmpOEQ(L, R, "feq");
            case ast::BinOp::Neq: return builder_->CreateFCmpONE(L, R, "fne");
            case ast::BinOp::Lt:  return builder_->CreateFCmpOLT(L, R, "flt");
            case ast::BinOp::Gt:  return builder_->CreateFCmpOGT(L, R, "fgt");
            case ast::BinOp::Lte: return builder_->CreateFCmpOLE(L, R, "fle");
            case ast::BinOp::Gte: return builder_->CreateFCmpOGE(L, R, "fge");
            case ast::BinOp::And:
            case ast::BinOp::Or:
                break; // fall through to integer logic below after bool conversion
            default: break;
        }
    }

    // ── Logical And/Or — convert both sides to i1 first ────────────────
    if (node.op == ast::BinOp::And) {
        return builder_->CreateAnd(toBool(L), toBool(R), "and");
    }
    if (node.op == ast::BinOp::Or) {
        return builder_->CreateOr(toBool(L), toBool(R), "or");
    }

    // ── Integer operations ─────────────────────────────────────────────
    switch (node.op) {
        case ast::BinOp::Add:    return builder_->CreateAdd(L, R, "add");
        case ast::BinOp::Sub:    return builder_->CreateSub(L, R, "sub");
        case ast::BinOp::Mul:    return builder_->CreateMul(L, R, "mul");
        case ast::BinOp::Div:    return builder_->CreateSDiv(L, R, "div");
        case ast::BinOp::IntDiv: return builder_->CreateSDiv(L, R, "idiv");
        case ast::BinOp::Mod:    return builder_->CreateSRem(L, R, "mod");
        case ast::BinOp::Pow:    return emitIntPow(L, R);
        case ast::BinOp::Eq:     return builder_->CreateICmpEQ(L, R, "eq");
        case ast::BinOp::Neq:    return builder_->CreateICmpNE(L, R, "ne");
        case ast::BinOp::Lt:     return builder_->CreateICmpSLT(L, R, "lt");
        case ast::BinOp::Gt:     return builder_->CreateICmpSGT(L, R, "gt");
        case ast::BinOp::Lte:    return builder_->CreateICmpSLE(L, R, "le");
        case ast::BinOp::Gte:    return builder_->CreateICmpSGE(L, R, "ge");
        case ast::BinOp::In:
        case ast::BinOp::NotIn:
            // Membership tests require runtime support; return false for now.
            return llvm::ConstantInt::get(*context_, llvm::APInt(1, 0));
        case ast::BinOp::BitAnd: return builder_->CreateAnd(L, R, "bitand");
        case ast::BinOp::BitOr:  return builder_->CreateOr(L, R, "bitor");
        case ast::BinOp::BitXor: return builder_->CreateXor(L, R, "bitxor");
        case ast::BinOp::Shl:    return builder_->CreateShl(L, R, "shl");
        case ast::BinOp::Shr:    return builder_->CreateAShr(L, R, "ashr");
        default: break;
    }

    return llvm::ConstantInt::get(*context_, llvm::APInt(64, 0));
}

// ── UnaryExpr ──────────────────────────────────────────────────────────────
llvm::Value* Codegen::codegenUnaryExpr(const ast::UnaryExpr& node) {
    llvm::Value* operand = codegenExpr(*node.operand);
    switch (node.op) {
        case ast::UnaryOp::Neg:
            if (operand->getType()->isDoubleTy())
                return builder_->CreateFNeg(operand, "fneg");
            return builder_->CreateNeg(operand, "neg");
        case ast::UnaryOp::Not:
            return builder_->CreateNot(toBool(operand), "not");
        case ast::UnaryOp::BitNot:
            return builder_->CreateNot(operand, "bitnot");
    }
    return operand;
}

// ── CallExpr ───────────────────────────────────────────────────────────────
llvm::Value* Codegen::codegenCallExpr(const ast::CallExpr& node) {
    auto* i8Ptr = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context_));
    auto* i64Ty = llvm::Type::getInt64Ty(*context_);

    // Handle print() / println() builtins.
    if (auto* ident = dynamic_cast<const ast::Identifier*>(node.callee.get())) {
        if (ident->name == "print" || ident->name == "println") {
            return emitPrintCall(node);
        }

        // ── Handle other builtin functions ─────────────────────────────
        if (isBuiltinFunction(ident->name) && ident->name != "print" && ident->name != "println") {
            return emitBuiltinCall(ident->name, node);
        }

        // ── Check if the callee is a closure variable (fat pointer) ────
        if (closureVars_.count(ident->name)) {
            auto* closureAlloca = lookupVar(ident->name);
            if (closureAlloca) {
                auto* closureTy = getClosureType();
                auto* closureVal = builder_->CreateLoad(closureTy, closureAlloca,
                                                         ident->name + ".closure");

                // Extract env and fn_ptr from the fat pointer.
                auto* envPtr = builder_->CreateExtractValue(closureVal, 0, "env");
                auto* fnRaw = builder_->CreateExtractValue(closureVal, 1, "fn.raw");

                // Build argument list.
                std::vector<llvm::Value*> argsWithEnv;
                argsWithEnv.push_back(envPtr); // env is always first arg
                for (const auto& a : node.args) {
                    argsWithEnv.push_back(codegenExpr(*a));
                }

                // We always call with env as first arg — the lambda function
                // signature includes env_ptr when it has captures, and doesn't
                // when it has no captures.  Since we don't know at the call site,
                // we always pass env (captured lambdas expect it, non-captured
                // lambdas need a different approach).
                //
                // For simplicity, we always generate the call with env.
                // Non-capturing lambdas also get env_ptr in their signature
                // (it's just null and ignored).

                // Build function type: i64(i8*, i64, i64, ...)
                std::vector<llvm::Type*> paramTys;
                paramTys.push_back(i8Ptr); // env
                for (size_t i = 0; i < node.args.size(); i++) {
                    paramTys.push_back(i64Ty);
                }
                auto* callFnTy = llvm::FunctionType::get(i64Ty, paramTys, false);

                // Cast fn_raw from i8* to the correct function pointer type.
                auto* fnPtr = builder_->CreateBitCast(
                    fnRaw, llvm::PointerType::getUnqual(callFnTy), "fn.ptr");

                return builder_->CreateCall(callFnTy, fnPtr, argsWithEnv, "closure.call");
            }
        }

        // Handle generic function calls via monomorphization.
        std::string funcName = ident->name;
        if (!node.typeArgs.empty()) {
            // Build mangled name: identity__int__str
            std::string mangled = ident->name;
            for (const auto& ta : node.typeArgs)
                mangled += "__" + ta;
            funcName = mangled;

            // Emit monomorphization if not already done.
            if (emittedMonomorphizations_.find(mangled) == emittedMonomorphizations_.end()) {
                auto it = genericFuncDefs_.find(ident->name);
                if (it != genericFuncDefs_.end()) {
                    emitMonomorphization(it->second, node.typeArgs, mangled);
                }
            }
        }

        // Check if this is an imported symbol (from X import Y).
        if (importedSymbols_.count(funcName)) {
            auto& [modName, mangledName] = importedSymbols_[funcName];
            auto* callee = module_->getFunction(mangledName);
            if (callee) {
                std::vector<llvm::Value*> args;
                for (const auto& a : node.args) {
                    args.push_back(codegenExpr(*a));
                }
                if (callee->getReturnType()->isVoidTy()) {
                    builder_->CreateCall(callee, args);
                    return llvm::ConstantInt::get(*context_, llvm::APInt(64, 0));
                }
                return builder_->CreateCall(callee, args, "modcall");
            }
        }

        auto* callee = module_->getFunction(funcName);
        if (!callee) {
            throw CodegenError("Unknown function: " + funcName,
                               node.line, node.column);
        }

        std::vector<llvm::Value*> args;
        size_t paramIdx = 0;
        for (const auto& a : node.args) {
            llvm::Value* val = codegenExpr(*a);
            // Coerce argument type to match parameter type.
            if (paramIdx < callee->arg_size()) {
                auto* expectedTy = callee->getFunctionType()->getParamType(paramIdx);
                if (expectedTy->isDoubleTy() && val->getType()->isIntegerTy())
                    val = promoteToDouble(val);
                else if (expectedTy->isIntegerTy(64) && val->getType()->isIntegerTy(1))
                    val = builder_->CreateZExt(val, llvm::Type::getInt64Ty(*context_), "zext");
                // If parameter expects a closure struct but we have a closure, pass it.
                // If parameter expects i8* and we have a closure, bitcast.
            }
            args.push_back(val);
            paramIdx++;
        }

        if (callee->getReturnType()->isVoidTy()) {
            builder_->CreateCall(callee, args);
            return llvm::ConstantInt::get(*context_, llvm::APInt(64, 0));
        }
        return builder_->CreateCall(callee, args, "call");
    }

    // Member call: object.method(args) — route through module or class method lookup.
    if (auto* member = dynamic_cast<const ast::MemberExpr*>(node.callee.get())) {
        // Check if it's a module call (e.g., math.sqrt(x), string.upper(s))
        if (auto* modIdent = dynamic_cast<const ast::Identifier*>(member->object.get())) {
            // Stdlib modules
            if (isStdlibModule(modIdent->name)) {
                return emitModuleCall(modIdent->name, member->member, node);
            }
            // User modules (imported via `import X`)
            if (importedModules_.count(modIdent->name) &&
                loadedUserModules_.count(modIdent->name)) {
                // Call module_name.func_name
                std::string mangledName = modIdent->name + "." + member->member;
                auto* callee = module_->getFunction(mangledName);
                if (callee) {
                    std::vector<llvm::Value*> args;
                    for (const auto& a : node.args) {
                        args.push_back(codegenExpr(*a));
                    }
                    if (callee->getReturnType()->isVoidTy()) {
                        builder_->CreateCall(callee, args);
                        return llvm::ConstantInt::get(*context_, llvm::APInt(64, 0));
                    }
                    return builder_->CreateCall(callee, args, "modcall");
                }
            }
        }
        // Stub: would need to look up ClassName_methodName.
    }

    return llvm::ConstantInt::get(*context_, llvm::APInt(64, 0));
}

// ── MemberExpr ─────────────────────────────────────────────────────────────
llvm::Value* Codegen::codegenMemberExpr(const ast::MemberExpr& node) {
    // Check if the object is a module identifier (e.g., math.PI, string.upper)
    if (auto* ident = dynamic_cast<const ast::Identifier*>(node.object.get())) {
        if (importedModules_.count(ident->name) || isStdlibModule(ident->name)) {
            // Module constant access (e.g., math.PI, math.E)
            std::string runtimeName = getModuleRuntimeName(ident->name, node.member);
            auto* fn = module_->getFunction(runtimeName);
            if (fn && fn->arg_size() == 0 && !fn->getReturnType()->isVoidTy()) {
                // It's a zero-arg function returning a value (constant accessor)
                return builder_->CreateCall(fn, {}, node.member);
            }
            // If it's a function, return a reference for later call
            if (fn) return fn;
        }
    }
    // Stub: requires struct layout for non-module member access.
    return llvm::ConstantInt::get(*context_, llvm::APInt(64, 0));
}

// ── IndexExpr ──────────────────────────────────────────────────────────────
llvm::Value* Codegen::codegenIndexExpr(const ast::IndexExpr& node) {
    llvm::Value* obj = codegenExpr(*node.object);
    llvm::Value* idx = codegenExpr(*node.index);

    if (obj->getType()->isPointerTy()) {
        // List or string indexing
        auto* listGetFn = module_->getFunction("__visuall_list_get");
        if (listGetFn) {
            return builder_->CreateCall(listGetFn, {obj, idx}, "idx.get");
        }
    }

    return llvm::ConstantInt::get(*context_, llvm::APInt(64, 0));
}

// ── Closure helpers ─────────────────────────────────────────────────────────

// The closure fat pointer struct: { i8* env, i8* fn_ptr }
llvm::StructType* Codegen::getClosureType() {
    auto* existing = llvm::StructType::getTypeByName(*context_, "__visuall_closure");
    if (existing) return existing;
    auto* i8Ptr = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context_));
    return llvm::StructType::create(*context_, {i8Ptr, i8Ptr}, "__visuall_closure");
}

llvm::Function* Codegen::getOrDeclareMalloc() {
    auto* fn = module_->getFunction("malloc");
    if (fn) return fn;
    auto* i8Ptr = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context_));
    auto* mallocTy = llvm::FunctionType::get(i8Ptr,
        {llvm::Type::getInt64Ty(*context_)}, false);
    return llvm::Function::Create(mallocTy, llvm::Function::ExternalLinkage,
                                  "malloc", module_.get());
}

llvm::Function* Codegen::getOrDeclareVisualAlloc() {
    auto* fn = module_->getFunction("__visuall_alloc");
    if (fn) return fn;
    auto* i8Ptr = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context_));
    auto* allocTy = llvm::FunctionType::get(i8Ptr,
        {llvm::Type::getInt64Ty(*context_),
         llvm::Type::getInt8Ty(*context_)}, false);
    return llvm::Function::Create(allocTy, llvm::Function::ExternalLinkage,
                                  "__visuall_alloc", module_.get());
}

// ── LambdaExpr ─────────────────────────────────────────────────────────────
llvm::Value* Codegen::codegenLambdaExpr(const ast::LambdaExpr& node) {
    static int lambdaCounter = 0;
    std::string name = "__lambda_" + std::to_string(lambdaCounter++);

    auto* i8Ptr = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context_));
    auto* i64Ty = llvm::Type::getInt64Ty(*context_);

    bool hasCaptures = !node.captures.empty();

    // ── 1. Build environment struct type ───────────────────────────────
    llvm::StructType* envStructTy = nullptr;
    std::vector<llvm::Type*> envFieldTypes;
    if (hasCaptures) {
        for (size_t i = 0; i < node.captures.size(); i++) {
            // All captured values are i64 for now.
            envFieldTypes.push_back(i64Ty);
        }
        envStructTy = llvm::StructType::create(*context_, envFieldTypes,
                                                name + "_env");
    }

    // ── 2. Build the lambda function ───────────────────────────────────
    // All lambdas get an env_ptr as the first parameter for uniform calling
    // convention.  Non-capturing lambdas simply ignore it.
    std::vector<llvm::Type*> paramTys;
    paramTys.push_back(i8Ptr); // env_ptr (always present)
    for (size_t i = 0; i < node.params.size(); i++) {
        paramTys.push_back(i64Ty);
    }

    auto* fnTy = llvm::FunctionType::get(i64Ty, paramTys, false);
    auto* fn = llvm::Function::Create(
        fnTy, llvm::Function::InternalLinkage, name, module_.get());

    // Name the parameters.
    auto argIt = fn->arg_begin();
    argIt->setName("env_ptr");
    ++argIt;
    for (size_t i = 0; i < node.params.size(); i++, ++argIt) {
        argIt->setName(node.params[i]);
    }

    auto* savedBB = builder_->GetInsertBlock();
    auto* savedFn = currentFunction_;

    auto* bb = llvm::BasicBlock::Create(*context_, "entry", fn);
    builder_->SetInsertPoint(bb);
    currentFunction_ = fn;
    pushScope();

    // Alloca + store for each user-visible parameter.
    argIt = fn->arg_begin();
    ++argIt; // skip env_ptr (always present)
    for (size_t i = 0; i < node.params.size(); i++, ++argIt) {
        auto* alloca = createEntryBlockAlloca(fn, node.params[i], i64Ty);
        builder_->CreateStore(&*argIt, alloca);
        declareVar(node.params[i], alloca);
    }

    // ── 2b. Load captured variables from the environment ───────────────
    if (hasCaptures) {
        llvm::Value* envPtr = &*fn->arg_begin();
        llvm::Value* envCast = builder_->CreateBitCast(
            envPtr, llvm::PointerType::getUnqual(envStructTy), "env");

        for (size_t i = 0; i < node.captures.size(); i++) {
            auto* gep = builder_->CreateStructGEP(envStructTy, envCast,
                                                    static_cast<unsigned>(i),
                                                    node.captures[i].name + ".ptr");
            auto* val = builder_->CreateLoad(i64Ty, gep,
                                              node.captures[i].name + ".val");
            auto* alloca = createEntryBlockAlloca(fn, node.captures[i].name, i64Ty);
            builder_->CreateStore(val, alloca);
            declareVar(node.captures[i].name, alloca);
        }
    }

    // ── 2c. Generate the lambda body ───────────────────────────────────
    llvm::Value* bodyVal = codegenExpr(*node.body);
    builder_->CreateRet(bodyVal);

    popScope();
    currentFunction_ = savedFn;
    if (savedBB) builder_->SetInsertPoint(savedBB);

    // ── 3. At the caller side: build the fat pointer ───────────────────
    auto* closureTy = getClosureType();

    if (!hasCaptures) {
        // No captures: env is null.
        auto* closureAlloca = createEntryBlockAlloca(
            currentFunction_, name + ".closure", closureTy);
        auto* envField = builder_->CreateStructGEP(closureTy, closureAlloca, 0, "env.field");
        builder_->CreateStore(llvm::ConstantPointerNull::get(
            llvm::cast<llvm::PointerType>(i8Ptr)), envField);
        auto* fnField = builder_->CreateStructGEP(closureTy, closureAlloca, 1, "fn.field");
        builder_->CreateStore(builder_->CreateBitCast(fn, i8Ptr, "fn.cast"), fnField);
        return builder_->CreateLoad(closureTy, closureAlloca, name + ".val");
    }

    // ── 3b. Heap-allocate the environment and fill it ──────────────────
    // Closure environments are GC-managed via __visuall_alloc (TAG_CLOSURE=4).
    auto* allocFn = getOrDeclareVisualAlloc();
    auto* dataLayout = &module_->getDataLayout();
    uint64_t envSize = dataLayout->getTypeAllocSize(envStructTy);
    auto* envRaw = builder_->CreateCall(
        allocFn,
        {llvm::ConstantInt::get(i64Ty, envSize),
         llvm::ConstantInt::get(llvm::Type::getInt8Ty(*context_), 4)},
        "env.raw");

    auto* envTyped = builder_->CreateBitCast(
        envRaw, llvm::PointerType::getUnqual(envStructTy), "env.typed");

    // Store each captured variable into the environment struct.
    for (size_t i = 0; i < node.captures.size(); i++) {
        auto* capturedAlloca = lookupVar(node.captures[i].name);
        if (!capturedAlloca) {
            throw CodegenError("Captured variable not found: " +
                               node.captures[i].name, node.line, node.column);
        }
        auto* capturedVal = builder_->CreateLoad(
            capturedAlloca->getAllocatedType(), capturedAlloca,
            node.captures[i].name + ".cap");
        // If captured value is not i64, convert it.
        llvm::Value* storeVal = capturedVal;
        if (capturedVal->getType() != i64Ty) {
            if (capturedVal->getType()->isIntegerTy()) {
                storeVal = builder_->CreateSExt(capturedVal, i64Ty, "cap.sext");
            } else if (capturedVal->getType()->isDoubleTy()) {
                storeVal = builder_->CreateBitCast(capturedVal, i64Ty, "cap.dcast");
            } else if (capturedVal->getType()->isPointerTy()) {
                storeVal = builder_->CreatePtrToInt(capturedVal, i64Ty, "cap.ptoi");
            }
        }
        auto* gep = builder_->CreateStructGEP(envStructTy, envTyped,
                                                static_cast<unsigned>(i),
                                                node.captures[i].name + ".env.ptr");
        builder_->CreateStore(storeVal, gep);
    }

    // ── 3c. Build the fat pointer { env, fn_ptr } ─────────────────────
    auto* closureAlloca = createEntryBlockAlloca(
        currentFunction_, name + ".closure", closureTy);
    auto* envField = builder_->CreateStructGEP(closureTy, closureAlloca, 0, "env.field");
    builder_->CreateStore(envRaw, envField);
    auto* fnField = builder_->CreateStructGEP(closureTy, closureAlloca, 1, "fn.field");
    builder_->CreateStore(builder_->CreateBitCast(fn, i8Ptr, "fn.cast"), fnField);

    return builder_->CreateLoad(closureTy, closureAlloca, name + ".val");
}

// ── ListExpr / DictExpr / TupleExpr ────────────────────────────────────────
llvm::Value* Codegen::codegenListExpr(const ast::ListExpr& node) {
    auto* i64Ty = llvm::Type::getInt64Ty(*context_);

    // Create a new list via __visuall_list_new()
    auto* newFn = module_->getFunction("__visuall_list_new");
    auto* pushFn = module_->getFunction("__visuall_list_push");
    if (!newFn || !pushFn) {
        return llvm::ConstantInt::get(i64Ty, 0);
    }

    llvm::Value* list = builder_->CreateCall(newFn, {}, "list.new");

    // Push each element
    for (const auto& elem : node.elements) {
        llvm::Value* val = codegenExpr(*elem);
        // Convert to i64 for storage
        if (val->getType()->isIntegerTy(1)) {
            val = builder_->CreateZExt(val, i64Ty, "bext");
        } else if (val->getType()->isPointerTy()) {
            val = builder_->CreatePtrToInt(val, i64Ty, "ptoi");
        } else if (val->getType()->isDoubleTy()) {
            val = builder_->CreateBitCast(val, i64Ty, "dcast");
        } else if (val->getType()->isIntegerTy() && val->getType() != i64Ty) {
            val = builder_->CreateSExt(val, i64Ty, "sext");
        }
        builder_->CreateCall(pushFn, {list, val});
    }

    return list;
}

llvm::Value* Codegen::codegenDictExpr(const ast::DictExpr& /*node*/) {
    // Stub: dicts require hash map runtime.
    return llvm::ConstantInt::get(*context_, llvm::APInt(64, 0));
}

llvm::Value* Codegen::codegenTupleExpr(const ast::TupleExpr& /*node*/) {
    // Stub: tuples → LLVM struct literals.
    return llvm::ConstantInt::get(*context_, llvm::APInt(64, 0));
}

// ── TupleUnpackStmt ────────────────────────────────────────────────────────
void Codegen::codegenTupleUnpackStmt(const ast::TupleUnpackStmt& node) {
    // Evaluate the RHS; for now, each target gets the same value (stub).
    llvm::Value* val = codegenExpr(*node.value);
    auto* i64Ty = llvm::Type::getInt64Ty(*context_);
    for (const auto& target : node.targets) {
        auto* alloca = createEntryBlockAlloca(currentFunction_, target,
                                               val->getType()->isIntegerTy() ? val->getType() : i64Ty);
        builder_->CreateStore(val->getType()->isIntegerTy() ? val
                              : llvm::ConstantInt::get(i64Ty, 0), alloca);
        declareVar(target, alloca);
    }
}

// ── SliceExpr ──────────────────────────────────────────────────────────────
llvm::Value* Codegen::codegenSliceExpr(const ast::SliceExpr& /*node*/) {
    // Slicing requires runtime list/string support; stub returns 0.
    return llvm::ConstantInt::get(*context_, llvm::APInt(64, 0));
}

// ── ListComprehension ──────────────────────────────────────────────────────
llvm::Value* Codegen::codegenListComprehension(const ast::ListComprehension& /*node*/) {
    // List comprehensions require runtime list allocation; stub returns 0.
    return llvm::ConstantInt::get(*context_, llvm::APInt(64, 0));
}

// ── DictComprehension ──────────────────────────────────────────────────────
llvm::Value* Codegen::codegenDictComprehension(const ast::DictComprehension& /*node*/) {
    // Dict comprehensions require runtime hash map; stub returns 0.
    return llvm::ConstantInt::get(*context_, llvm::APInt(64, 0));
}

// ── SpreadExpr ─────────────────────────────────────────────────────────────
llvm::Value* Codegen::codegenSpreadExpr(const ast::SpreadExpr& node) {
    // Spread is handled at call sites; just return the operand value.
    return codegenExpr(*node.operand);
}

// ── TernaryExpr ────────────────────────────────────────────────────────────
llvm::Value* Codegen::codegenTernaryExpr(const ast::TernaryExpr& node) {
    llvm::Value* condVal = toBool(codegenExpr(*node.condition));
    auto* fn = builder_->GetInsertBlock()->getParent();

    auto* thenBB  = llvm::BasicBlock::Create(*context_, "tern.then", fn);
    auto* elseBB  = llvm::BasicBlock::Create(*context_, "tern.else", fn);
    auto* mergeBB = llvm::BasicBlock::Create(*context_, "tern.end", fn);

    builder_->CreateCondBr(condVal, thenBB, elseBB);

    builder_->SetInsertPoint(thenBB);
    llvm::Value* thenVal = codegenExpr(*node.thenExpr);
    auto* thenEndBB = builder_->GetInsertBlock(); // may have changed
    builder_->CreateBr(mergeBB);

    builder_->SetInsertPoint(elseBB);
    llvm::Value* elseVal = codegenExpr(*node.elseExpr);
    auto* elseEndBB = builder_->GetInsertBlock();
    builder_->CreateBr(mergeBB);

    builder_->SetInsertPoint(mergeBB);

    // Unify types for PHI
    llvm::Type* phiTy = thenVal->getType();
    if (thenVal->getType() != elseVal->getType()) {
        // Promote to double if mixed
        if (thenVal->getType()->isDoubleTy() || elseVal->getType()->isDoubleTy()) {
            phiTy = llvm::Type::getDoubleTy(*context_);
            if (!thenVal->getType()->isDoubleTy()) {
                builder_->SetInsertPoint(thenEndBB->getTerminator());
                thenVal = promoteToDouble(thenVal);
                builder_->SetInsertPoint(mergeBB);
            }
            if (!elseVal->getType()->isDoubleTy()) {
                builder_->SetInsertPoint(elseEndBB->getTerminator());
                elseVal = promoteToDouble(elseVal);
                builder_->SetInsertPoint(mergeBB);
            }
        }
    }

    auto* phi = builder_->CreatePHI(phiTy, 2, "tern");
    phi->addIncoming(thenVal, thenEndBB);
    phi->addIncoming(elseVal, elseEndBB);
    return phi;
}

// ── InterfaceDef ───────────────────────────────────────────────────────────
void Codegen::codegenInterfaceDef(const ast::InterfaceDef& /*node*/) {
    // Interfaces are type-level only; no IR emitted.
}

// ════════════════════════════════════════════════════════════════════════════
// Builtin function call emission
// ════════════════════════════════════════════════════════════════════════════
llvm::Value* Codegen::emitBuiltinCall(const std::string& name, const ast::CallExpr& node) {
    auto* i64Ty = llvm::Type::getInt64Ty(*context_);
    auto* f64Ty = llvm::Type::getDoubleTy(*context_);
    auto* i8Ptr = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context_));

    // ── input(prompt) ──────────────────────────────────────────────────
    if (name == "input") {
        llvm::Value* prompt = builder_->CreateGlobalString("", "empty_prompt");
        if (!node.args.empty()) {
            prompt = codegenExpr(*node.args[0]);
        }
        auto* fn = module_->getFunction("__visuall_input");
        if (fn) return builder_->CreateCall(fn, {prompt}, "input.result");
        return prompt;
    }

    // ── len(x) ─────────────────────────────────────────────────────────
    if (name == "len") {
        if (node.args.empty())
            throw CodegenError("len() requires 1 argument", node.line, node.column);
        llvm::Value* arg = codegenExpr(*node.args[0]);
        if (arg->getType()->isPointerTy()) {
            // Could be string or list; try string length first
            auto* fn = module_->getFunction("__visuall_str_len");
            if (fn) return builder_->CreateCall(fn, {arg}, "len");
        }
        // For list pointers (also i8*), use list_len
        auto* fn = module_->getFunction("__visuall_list_len");
        if (fn && arg->getType()->isPointerTy())
            return builder_->CreateCall(fn, {arg}, "len");
        return llvm::ConstantInt::get(i64Ty, 0);
    }

    // ── range(stop) / range(start, stop) / range(start, stop, step) ───
    if (name == "range") {
        llvm::Value* start = llvm::ConstantInt::get(i64Ty, 0);
        llvm::Value* stop  = llvm::ConstantInt::get(i64Ty, 0);
        llvm::Value* step  = llvm::ConstantInt::get(i64Ty, 1);

        if (node.args.size() == 1) {
            stop = codegenExpr(*node.args[0]);
        } else if (node.args.size() == 2) {
            start = codegenExpr(*node.args[0]);
            stop  = codegenExpr(*node.args[1]);
        } else if (node.args.size() >= 3) {
            start = codegenExpr(*node.args[0]);
            stop  = codegenExpr(*node.args[1]);
            step  = codegenExpr(*node.args[2]);
        }

        auto* fn = module_->getFunction("__visuall_range");
        if (fn) return builder_->CreateCall(fn, {start, stop, step}, "range.result");
        return llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(i8Ptr));
    }

    // ── type(x) ────────────────────────────────────────────────────────
    if (name == "type") {
        int64_t tag = 0; // default: int
        if (!node.args.empty()) {
            llvm::Value* arg = codegenExpr(*node.args[0]);
            if (arg->getType()->isDoubleTy()) tag = 1;
            else if (arg->getType()->isPointerTy()) tag = 2;
            else if (arg->getType()->isIntegerTy(1)) tag = 3;
            else tag = 0;
        }
        auto* fn = module_->getFunction("__visuall_type_name");
        if (fn) return builder_->CreateCall(fn, {llvm::ConstantInt::get(i64Ty, tag)}, "type.name");
        return builder_->CreateGlobalString("unknown", "type.stub");
    }

    // ── int(x) ─────────────────────────────────────────────────────────
    if (name == "int") {
        if (node.args.empty()) return llvm::ConstantInt::get(i64Ty, 0);
        llvm::Value* arg = codegenExpr(*node.args[0]);
        if (arg->getType()->isIntegerTy(64)) return arg;
        if (arg->getType()->isDoubleTy()) {
            auto* fn = module_->getFunction("__visuall_float_to_int");
            if (fn) return builder_->CreateCall(fn, {arg}, "toint");
            return builder_->CreateFPToSI(arg, i64Ty, "toint");
        }
        if (arg->getType()->isIntegerTy(1)) {
            return builder_->CreateZExt(arg, i64Ty, "toint");
        }
        if (arg->getType()->isPointerTy()) {
            auto* fn = module_->getFunction("__visuall_str_to_int");
            if (fn) return builder_->CreateCall(fn, {arg}, "toint");
        }
        return llvm::ConstantInt::get(i64Ty, 0);
    }

    // ── float(x) ───────────────────────────────────────────────────────
    if (name == "float") {
        if (node.args.empty()) return llvm::ConstantFP::get(f64Ty, 0.0);
        llvm::Value* arg = codegenExpr(*node.args[0]);
        if (arg->getType()->isDoubleTy()) return arg;
        if (arg->getType()->isIntegerTy(64)) {
            auto* fn = module_->getFunction("__visuall_int_to_float");
            if (fn) return builder_->CreateCall(fn, {arg}, "tofloat");
            return builder_->CreateSIToFP(arg, f64Ty, "tofloat");
        }
        if (arg->getType()->isIntegerTy(1)) {
            return builder_->CreateUIToFP(arg, f64Ty, "tofloat");
        }
        if (arg->getType()->isPointerTy()) {
            auto* fn = module_->getFunction("__visuall_str_to_float");
            if (fn) return builder_->CreateCall(fn, {arg}, "tofloat");
        }
        return llvm::ConstantFP::get(f64Ty, 0.0);
    }

    // ── str(x) ─────────────────────────────────────────────────────────
    if (name == "str") {
        if (node.args.empty()) return builder_->CreateGlobalString("", "empty");
        llvm::Value* arg = codegenExpr(*node.args[0]);
        if (arg->getType()->isPointerTy()) return arg;
        if (arg->getType()->isIntegerTy(64)) {
            auto* fn = module_->getFunction("__visuall_int_to_str");
            if (fn) return builder_->CreateCall(fn, {arg}, "tostr");
        }
        if (arg->getType()->isDoubleTy()) {
            auto* fn = module_->getFunction("__visuall_float_to_str");
            if (fn) return builder_->CreateCall(fn, {arg}, "tostr");
        }
        if (arg->getType()->isIntegerTy(1)) {
            auto* ext = builder_->CreateZExt(arg, i64Ty, "bext");
            auto* fn = module_->getFunction("__visuall_bool_to_str");
            if (fn) return builder_->CreateCall(fn, {ext}, "tostr");
        }
        return builder_->CreateGlobalString("<value>", "tostr.stub");
    }

    // ── bool(x) ────────────────────────────────────────────────────────
    if (name == "bool") {
        if (node.args.empty()) return llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context_), 0);
        llvm::Value* arg = codegenExpr(*node.args[0]);
        return toBool(arg);
    }

    // ── abs(x) ─────────────────────────────────────────────────────────
    if (name == "abs") {
        if (node.args.empty()) return llvm::ConstantInt::get(i64Ty, 0);
        llvm::Value* arg = codegenExpr(*node.args[0]);
        if (arg->getType()->isDoubleTy()) {
            auto* fn = module_->getFunction("__visuall_abs_float");
            if (fn) return builder_->CreateCall(fn, {arg}, "abs");
        } else {
            auto* fn = module_->getFunction("__visuall_abs_int");
            if (fn) return builder_->CreateCall(fn, {arg}, "abs");
        }
        return arg;
    }

    // ── min(a, b) ──────────────────────────────────────────────────────
    if (name == "min") {
        if (node.args.size() < 2) return llvm::ConstantInt::get(i64Ty, 0);
        llvm::Value* a = codegenExpr(*node.args[0]);
        llvm::Value* b = codegenExpr(*node.args[1]);
        if (a->getType()->isDoubleTy() || b->getType()->isDoubleTy()) {
            a = promoteToDouble(a); b = promoteToDouble(b);
            auto* fn = module_->getFunction("__visuall_min_float");
            if (fn) return builder_->CreateCall(fn, {a, b}, "min");
        } else {
            auto* fn = module_->getFunction("__visuall_min_int");
            if (fn) return builder_->CreateCall(fn, {a, b}, "min");
        }
        return a;
    }

    // ── max(a, b) ──────────────────────────────────────────────────────
    if (name == "max") {
        if (node.args.size() < 2) return llvm::ConstantInt::get(i64Ty, 0);
        llvm::Value* a = codegenExpr(*node.args[0]);
        llvm::Value* b = codegenExpr(*node.args[1]);
        if (a->getType()->isDoubleTy() || b->getType()->isDoubleTy()) {
            a = promoteToDouble(a); b = promoteToDouble(b);
            auto* fn = module_->getFunction("__visuall_max_float");
            if (fn) return builder_->CreateCall(fn, {a, b}, "max");
        } else {
            auto* fn = module_->getFunction("__visuall_max_int");
            if (fn) return builder_->CreateCall(fn, {a, b}, "max");
        }
        return a;
    }

    // ── round(x) ───────────────────────────────────────────────────────
    if (name == "round") {
        if (node.args.empty()) return llvm::ConstantInt::get(i64Ty, 0);
        llvm::Value* arg = codegenExpr(*node.args[0]);
        arg = promoteToDouble(arg);
        auto* fn = module_->getFunction("__visuall_round");
        if (fn) return builder_->CreateCall(fn, {arg}, "round");
        return llvm::ConstantInt::get(i64Ty, 0);
    }

    // ── sorted(list) ───────────────────────────────────────────────────
    if (name == "sorted") {
        if (node.args.empty()) return llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(i8Ptr));
        llvm::Value* arg = codegenExpr(*node.args[0]);
        auto* fn = module_->getFunction("__visuall_sorted");
        if (fn) return builder_->CreateCall(fn, {arg}, "sorted");
        return arg;
    }

    // ── reversed(list) ─────────────────────────────────────────────────
    if (name == "reversed") {
        if (node.args.empty()) return llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(i8Ptr));
        llvm::Value* arg = codegenExpr(*node.args[0]);
        auto* fn = module_->getFunction("__visuall_reversed");
        if (fn) return builder_->CreateCall(fn, {arg}, "reversed");
        return arg;
    }

    // ── enumerate(list) ────────────────────────────────────────────────
    if (name == "enumerate") {
        if (node.args.empty()) return llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(i8Ptr));
        llvm::Value* arg = codegenExpr(*node.args[0]);
        auto* fn = module_->getFunction("__visuall_enumerate");
        if (fn) return builder_->CreateCall(fn, {arg}, "enumerate");
        return arg;
    }

    // ── zip(a, b) ──────────────────────────────────────────────────────
    if (name == "zip") {
        if (node.args.size() < 2) return llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(i8Ptr));
        llvm::Value* a = codegenExpr(*node.args[0]);
        llvm::Value* b = codegenExpr(*node.args[1]);
        auto* fn = module_->getFunction("__visuall_zip");
        if (fn) return builder_->CreateCall(fn, {a, b}, "zip");
        return a;
    }

    // ── map(fn, list) ──────────────────────────────────────────────────
    if (name == "map") {
        if (node.args.size() < 2) return llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(i8Ptr));
        llvm::Value* closure = codegenExpr(*node.args[0]);
        llvm::Value* list    = codegenExpr(*node.args[1]);
        // closure is a fat pointer { env, fn_ptr }
        auto* closureTy = getClosureType();
        llvm::Value* env   = builder_->CreateExtractValue(closure, 0, "map.env");
        llvm::Value* fnPtr = builder_->CreateExtractValue(closure, 1, "map.fn");
        auto* fn = module_->getFunction("__visuall_map");
        if (fn) return builder_->CreateCall(fn, {env, fnPtr, list}, "map.result");
        return list;
    }

    // ── filter(fn, list) ───────────────────────────────────────────────
    if (name == "filter") {
        if (node.args.size() < 2) return llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(i8Ptr));
        llvm::Value* closure = codegenExpr(*node.args[0]);
        llvm::Value* list    = codegenExpr(*node.args[1]);
        auto* closureTy = getClosureType();
        (void)closureTy;
        llvm::Value* env   = builder_->CreateExtractValue(closure, 0, "filter.env");
        llvm::Value* fnPtr = builder_->CreateExtractValue(closure, 1, "filter.fn");
        auto* fn = module_->getFunction("__visuall_filter");
        if (fn) return builder_->CreateCall(fn, {env, fnPtr, list}, "filter.result");
        return list;
    }

    // Fallback
    return llvm::ConstantInt::get(i64Ty, 0);
}

// ════════════════════════════════════════════════════════════════════════════
// Module function call emission (e.g., math.sqrt, string.upper)
// ════════════════════════════════════════════════════════════════════════════
llvm::Value* Codegen::emitModuleCall(const std::string& moduleName,
                                      const std::string& funcName,
                                      const ast::CallExpr& node) {
    auto* i64Ty = llvm::Type::getInt64Ty(*context_);
    std::string runtimeName = getModuleRuntimeName(moduleName, funcName);

    auto* fn = module_->getFunction(runtimeName);
    if (!fn) {
        throw CodegenError("Unknown module function: " + moduleName + "." + funcName,
                           node.line, node.column);
    }

    // Build argument list, coercing types as needed.
    std::vector<llvm::Value*> args;
    for (size_t i = 0; i < node.args.size(); i++) {
        llvm::Value* val = codegenExpr(*node.args[i]);
        if (i < fn->arg_size()) {
            auto* expectedTy = fn->getFunctionType()->getParamType(static_cast<unsigned>(i));
            if (expectedTy->isDoubleTy() && val->getType()->isIntegerTy())
                val = promoteToDouble(val);
            else if (expectedTy->isIntegerTy(64) && val->getType()->isIntegerTy(1))
                val = builder_->CreateZExt(val, i64Ty, "zext");
        }
        args.push_back(val);
    }

    if (fn->getReturnType()->isVoidTy()) {
        builder_->CreateCall(fn, args);
        return llvm::ConstantInt::get(i64Ty, 0);
    }
    return builder_->CreateCall(fn, args, runtimeName + ".result");
}

// ── emitMonomorphization ───────────────────────────────────────────────────
void Codegen::emitMonomorphization(const ast::FuncDef* def,
                                    const std::vector<std::string>& typeArgs,
                                    const std::string& mangledName) {
    emittedMonomorphizations_.insert(mangledName);

    // Build type param → concrete type mapping.
    std::unordered_map<std::string, std::string> typeMap;
    for (size_t i = 0; i < def->typeParams.size() && i < typeArgs.size(); i++) {
        typeMap[def->typeParams[i]] = typeArgs[i];
    }

    auto resolve = [&](const std::string& t) -> std::string {
        auto it = typeMap.find(t);
        return it != typeMap.end() ? it->second : t;
    };

    // Determine return type.
    std::string retTypeStr = def->returnType.empty() ? "void" : resolve(def->returnType);
    llvm::Type* retTy = getLLVMType(retTypeStr);

    // Determine param types.
    std::vector<llvm::Type*> paramTys;
    for (const auto& p : def->params) {
        std::string pt = p.typeAnnotation.empty() ? "int" : resolve(p.typeAnnotation);
        paramTys.push_back(getLLVMType(pt));
    }

    auto* fnTy = llvm::FunctionType::get(retTy, paramTys, false);
    auto* fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage,
                                       mangledName, module_.get());

    // Name params.
    size_t idx = 0;
    for (auto& arg : fn->args()) {
        if (idx < def->params.size())
            arg.setName(def->params[idx++].name);
    }

    auto* savedBB = builder_->GetInsertBlock();
    auto* savedFn = currentFunction_;

    auto* bb = llvm::BasicBlock::Create(*context_, "entry", fn);
    builder_->SetInsertPoint(bb);
    currentFunction_ = fn;
    pushScope();

    for (auto& arg : fn->args()) {
        auto* alloca = createEntryBlockAlloca(fn, std::string(arg.getName()),
                                               arg.getType());
        builder_->CreateStore(&arg, alloca);
        declareVar(std::string(arg.getName()), alloca);
    }

    codegenStmtList(def->body);

    if (!builder_->GetInsertBlock()->getTerminator()) {
        if (fn->getReturnType()->isVoidTy()) {
            builder_->CreateRetVoid();
        } else {
            builder_->CreateRet(llvm::Constant::getNullValue(fn->getReturnType()));
        }
    }

    popScope();
    currentFunction_ = savedFn;
    if (savedBB) builder_->SetInsertPoint(savedBB);
}

} // namespace visuall
