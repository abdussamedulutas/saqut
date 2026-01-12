#include "llvm/include/llvm-c/Core.h"
#include "llvm/include/llvm-c/Analysis.h"
#include <stdio.h>

int main() {
    // 1. LLVM Context Oluştur (Bellek yönetimi ve izolasyon için şart)
    LLVMContextRef context = LLVMContextCreate();

    // 2. Modülü bu context içinde oluştur
    LLVMModuleRef mod = LLVMModuleCreateWithNameInContext("saqut_module", context);

    // 3. Basit bir fonksiyon tipi oluştur: int32 f()
    LLVMTypeRef ret_type = LLVMInt32TypeInContext(context);
    LLVMTypeRef func_type = LLVMFunctionType(ret_type, NULL, 0, 0);

    // 4. Fonksiyonu modüle ekle
    LLVMValueRef main_func = LLVMAddFunction(mod, "saqut_main", func_type);

    // 5. Temel bir blok (Entry block) ekle
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, main_func, "entry");
    
    // 6. Bir Builder oluştur ve fonksiyonun içine "return 0" ekle
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMBuildRet(builder, LLVMConstInt(LLVMInt32TypeInContext(context), 0, 0));

    // 7. Oluşturulan IR kodunu terminale yazdır (Gözle kontrol için)
    printf("--- Oluşturulan LLVM IR Kodu ---\n");
    LLVMDumpModule(mod);
    printf("--------------------------------\n");
    printf("saQut: LLVM Modülü ve Fonksiyonu başarıyla oluşturuldu!\n");

    // 8. Temizlik (Bellek sızıntısını önlemek için önemli)
    LLVMDisposeBuilder(builder);
    LLVMDisposeModule(mod);
    LLVMContextDispose(context);

    return 0;
}