// ============================================================================
// saQut VM — Value (Çalışma Zamanı Değer)
//
// Bir saQut değerinin bellekteki temsilidir.
//
// ŞU AN SADECE INT:
//   fibonacci.sqt tamamen int kullanır, bu dikey dilim için int yeterli.
//   İleride float, bool, string eklenmesi için "kind" alanı iskelet olarak bırakıldı.
//
// BOOLEAN OLARAK KULLANIM:
//   JIF_FALSE talimatı değerin 0 olup olmadığına bakar.
//   0 = yanlış, sıfır-dışı = doğru. C geleneği.
// ============================================================================

#ifndef SAQUT_VM_VALUE
#define SAQUT_VM_VALUE

#include <string>

// Gelecekte float/bool/string eklendiğinde burası genişleyecek.
// Şimdilik sadece int.
enum class ValueKind {
    Int,
    String,
    // Float,   // TODO(vm-genişletme)
    // Bool,    // TODO(vm-genişletme)
};

struct Value {
    ValueKind   kind        = ValueKind::Int;
    int         intValue    = 0;
    std::string stringValue;   // yalnızca kind == String için geçerli

    static Value fromInt(int n) {
        Value v;
        v.kind     = ValueKind::Int;
        v.intValue = n;
        return v;
    }

    static Value fromString(std::string s) {
        Value v;
        v.kind        = ValueKind::String;
        v.stringValue = std::move(s);
        return v;
    }

    // JIF_FALSE için: int 0 = yanlış, boş string = yanlış, diğer = doğru
    bool isTruthy() const {
        if (kind == ValueKind::Int)    return intValue != 0;
        if (kind == ValueKind::String) return !stringValue.empty();
        return false;
    }

    // Okunabilir metin — dump ve hata mesajları için
    std::string typeName() const {
        switch (kind) {
            case ValueKind::Int:    return "int";
            case ValueKind::String: return "string";
        }
        return "?";
    }
};

#endif // SAQUT_VM_VALUE
