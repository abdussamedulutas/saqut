#ifndef SAQUT_VM_VALUE
#define SAQUT_VM_VALUE

#include <string>
#include <stdexcept>

// Çalışma zamanı değer tipi.
//
// Bool ayrı bir kind değil — boolean sonuçlar int olarak saklanır
// (0 = yanlış, sıfır-dışı = doğru; C geleneği, JIF_FALSE buna dayanır).
// Float henüz implement edilmedi — IR'de float opcode yok.
enum class ValueKind {
    Int,
    String,
    // Float,  // TODO: float literal + aritmetik eklenince
};

struct Value {
    ValueKind   kind        = ValueKind::Int;
    int         intValue    = 0;
    std::string stringValue; // yalnızca kind == String için geçerli

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
        switch (kind) {
            case ValueKind::Int:    return intValue != 0;
            case ValueKind::String: return !stringValue.empty();
        }
        return false;
    }

    // Yazdırma ve hata mesajları için okunabilir temsil
    std::string toString() const {
        switch (kind) {
            case ValueKind::Int:    return std::to_string(intValue);
            case ValueKind::String: return stringValue;
        }
        return "?";
    }

    // Tip adı — hata mesajları için
    std::string typeName() const {
        switch (kind) {
            case ValueKind::Int:    return "int";
            case ValueKind::String: return "string";
        }
        return "?";
    }
};

#endif // SAQUT_VM_VALUE
