#ifndef SAQUT_VM_VALUE
#define SAQUT_VM_VALUE

#include <string>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include "core/decimal.hpp"

// Forward — Object tam tanımı object.hpp'de; Value onu pointer olarak taşır.
struct Object;

// ADR-020: Primitive (int/bool) = değer; bileşik (array/struct/string) = referans.
// ADR-021: Null = nullable referansların null değeri (saQut'ta `null` anahtar sözcüğü).
// Bool ayrı kind değil — boolean sonuçlar int olarak saklanır (0=yanlış, sıfır-dışı=doğru).
enum class ValueKind {
    Int,
    Float,    // #44: float/double tek kind; floatValue alanı taşır
    Decimal,  // ADR-028: ondalık hassasiyet; decimalValue alanı taşır
    String,
    Ref,      // ADR-020: array/struct nesnesine Object* referansı
    Null,     // ADR-021: nullable referansın null değeri (saQut kaynağında `null`)
};

struct Value {
    ValueKind    kind         = ValueKind::Int;
    int          intValue     = 0;
    double       floatValue   = 0.0;          // kind == Float için
    DecimalValue decimalValue;                 // kind == Decimal için (ADR-028)
    std::string  stringValue;                  // kind == String için
    Object*      ref          = nullptr;       // kind == Ref için

    static Value fromInt(int n) {
        Value v; v.kind = ValueKind::Int; v.intValue = n; return v;
    }

    static Value fromFloat(double d) {
        Value v; v.kind = ValueKind::Float; v.floatValue = d; return v;
    }

    static Value fromDecimal(const DecimalValue& d) {
        Value v; v.kind = ValueKind::Decimal; v.decimalValue = d; return v;
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

    bool isTruthy() const {
        switch (kind) {
            case ValueKind::Int:     return intValue != 0;
            case ValueKind::Float:   return floatValue != 0.0;
            case ValueKind::Decimal: return decimalValue.isTruthy();
            case ValueKind::String:  return !stringValue.empty();
            case ValueKind::Ref:     return ref != nullptr;
            case ValueKind::Null:    return false;
        }
        return false;
    }

    std::string toString() const {
        switch (kind) {
            case ValueKind::Int:     return std::to_string(intValue);
            case ValueKind::Decimal: return decimalValue.toString();
            case ValueKind::Float: {
                // Tam sayıysa "3.0", değilse "3.14" gibi — gereksiz sıfırları kırp
                std::ostringstream oss;
                oss << std::setprecision(10) << floatValue;
                std::string s = oss.str();
                // Nokta yoksa ".0" ekle (saQut float değerleri her zaman nokta içerir)
                if (s.find('.') == std::string::npos && s.find('e') == std::string::npos)
                    s += ".0";
                return s;
            }
            case ValueKind::String: return stringValue;
            case ValueKind::Ref:   return "<ref>";
            case ValueKind::Null:  return "null";
        }
        return "?";
    }

    std::string typeName() const {
        switch (kind) {
            case ValueKind::Int:     return "int";
            case ValueKind::Float:   return "float";
            case ValueKind::Decimal: return "decimal";
            case ValueKind::String:  return "string";
            case ValueKind::Ref:     return "ref";
            case ValueKind::Null:    return "null";
        }
        return "?";
    }
};

#endif // SAQUT_VM_VALUE
