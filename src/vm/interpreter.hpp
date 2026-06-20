// ============================================================================
// saQut VM — Interpreter (Bytecode Yorumlayıcı)
//
// IRProgram içindeki talimatları çalıştırır.
// "main" fonksiyonundan başlar, RETURN ile biten frame'leri kapatır.
//
// DÖNGÜ GÜVENLİĞİ (referans invalidation):
//   Her iterasyonun başında callStack.back() tazeden alınır.
//   CALL ve RETURN'den sonra `continue` ile döngü başına dönülür;
//   böylece vector büyümesinden kaynaklanan dangling pointer sorunu olmaz.
// ============================================================================

#ifndef SAQUT_VM_INTERPRETER
#define SAQUT_VM_INTERPRETER

#include <vector>
#include "ir/ir_program.hpp"
#include "vm/call_frame.hpp"

class Interpreter {
public:
    explicit Interpreter(IRProgram& program) : program_(program) {}

    // "main" fonksiyonunu bul ve çalıştır.
    // Tamamlandığında main'in dönüş değerini (int) döndürür.
    int run();

private:
    IRProgram&            program_;
    std::vector<CallFrame> callStack_;
    std::vector<Value>    globalSlots_;

    // Host (C++) fonksiyon çağrısı — şu an sadece "print" destekli
    void executeHostFunction(const std::string& name,
                             const std::vector<Value>& slots,
                             const std::vector<int>&   argSlots);
};

#endif // SAQUT_VM_INTERPRETER
