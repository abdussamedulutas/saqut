#ifndef SAQUT_VM_VALUE
#define SAQUT_VM_VALUE

#include <string>
#include <stdexcept>

// Forward — Object tam tanımı object.hpp'de; Value onu pointer olarak taşır.
struct Object;

// ADR-020: Primitive (int/bool) = değer; bileşik (array/struct/string) = referans.
// ADR-021: Null = nullable referansların null değeri (saQut'ta `null` anahtar sözcüğü).
// Bool ayrı kind değil — boolean sonuçlar int olarak saklanır (0=yanlış, sıfır-dışı=doğru).
// Float henüz implement edilmedi — IR'de float opcode yok.
enum class ValueKind {
    Int,
    String,
    Ref,   // ADR-020: array/struct nesnesine Object* referansı
    Null,  // ADR-021: nullable referansın null değeri (saQut kaynağında `null`)
    // Float,  // TODO(#44)
};

struct Value {
    ValueKind   kind        = ValueKind::Int;
    int         intValue    = 0;
    std::string stringValue; // yalnızca kind == String
    Object*     ref         = nullptr; // yalnızca kind == Ref

    static Value fromInt(int n) {
        Value v; v.kind = ValueKind::Int; v.intValue = n; return v;
    }

    static Value fromString(std::string s) {
        Value v; v.kind = ValueKind::String; v.stringValue = std::move(s); return v;
    }

    static Value fromRef(Object* obj) {
        Value v; v.kind = ValueKind::Ref; v.ref = obj; return v;
    }

    static Value null() {
        Value v; v.kind = ValueKind::Null; return v;
    }

    // JIF_FALSE: int 0 / boş string / null = yanlış; Ref her zaman doğru
    bool isTruthy() const {
        switch (kind) {
            case ValueKind::Int:    return intValue != 0;
            case ValueKind::String: return !stringValue.empty();
            case ValueKind::Ref:   return ref != nullptr;
            case ValueKind::Null:  return false;
        }
        return false;
    }

    std::string toString() const {
        switch (kind) {
            case ValueKind::Int:    return std::to_string(intValue);
            case ValueKind::String: return stringValue;
            case ValueKind::Ref:   return "<array>";
            case ValueKind::Null:  return "null";
        }
        return "?";
    }

    std::string typeName() const {
        switch (kind) {
            case ValueKind::Int:    return "int";
            case ValueKind::String: return "string";
            case ValueKind::Ref:   return "array";
            case ValueKind::Null:  return "null";
        }
        return "?";
    }
};

#endif // SAQUT_VM_VALUE
