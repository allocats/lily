#include "codegen/codegen.h"
#include "diagnostics/diagnostics.h"
#include "driver/types.h"
#include "files/files.h"

#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>

extern DriverCtx driver;

static void codegen_file(FileId id);

void codegen() {
    u32 file_count = driver.file_interner.count;

    for (u32 i = 0; i < file_count; i++) {
        codegen_file(i);
    }
}

static void codegen_file(FileId id) {
    File* file = file_lookup_id(id);

    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmParser();
    LLVMInitializeNativeAsmPrinter();

    LLVMContextRef ctx = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext(file -> path.ptr, ctx);
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(ctx);

    char* host_triple = LLVMGetDefaultTargetTriple();
    LLVMSetTarget(module, host_triple);

    char* msg = null;

    LLVMTargetRef target = null; 

    if (LLVMGetTargetFromTriple(host_triple, &target, &msg) != 0) {
        diagnostic_add_generic(DIAG_ERROR, "LLVM: %s", msg);
        LLVMDisposeMessage(msg);
        return;
    }

    LLVMTargetMachineRef target_machine = LLVMCreateTargetMachine(
        target,
        host_triple,
        "generic",
        "",
        LLVMCodeGenLevelDefault,
        LLVMRelocPIC,
        LLVMCodeModelDefault
    );

    if (LLVMTargetMachineEmitToFile(target_machine, module, file -> object_path, LLVMObjectFile, &msg) != 0) {
        diagnostic_add_generic(DIAG_ERROR, "LLVM: %s", msg);
        LLVMDisposeMessage(msg);
        return;
    }

    LLVMDisposeTargetMachine(target_machine);
    LLVMDisposeBuilder(builder);
    LLVMDisposeModule(module);
    LLVMContextDispose(ctx);
} 
