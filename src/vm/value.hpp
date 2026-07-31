// ============================================================================
// saQut VM — Value (Çalışma Zamanı Değeri)
// ============================================================================
//
// DİZİN:   src/vm/value.hpp
// KATMAN:  VM — Tüm çalışma zamanı değerlerinin ortak temsili
//
// AMAÇ:
//   Int, Float, Bool, String, Decimal, Null ve Ref (heap referansı) değerlerini
//   tek bir Value struct'ında birleştirir. VM'in tüm veri alışverişi Value
//   üzerinden yapılır.
//
// ============================================================================

#ifndef SAQUT_VM_VALUE
#define SAQUT_VM_VALUE

#include <string>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include "core/decimal.hpp"
#include "core/float_format.hpp"

// Forward — Object tam tanımı object.hpp'de; Value onu pointer olarak taşır.
struct Object;

// ADR-020: Primitive (int/bool) = değer; bileşik (array/struct/string) = referans.
// ADR-021: Null = nullable referansların null değeri (saQut'ta `null` anahtar sözcüğü).
// Bool ayrı kind değil — boolean sonuçlar int olarak saklanır (0=yanlış, sıfır-dışı=doğru).
enum class ValueKind {
    Int,
    LongInt,  // ADR-040: 64-bit işaretli tamsayı; int64Value alanı taşır
    Float,    // ADR-040: 64-bit IEEE double; floatValue alanı taşır
    Float32,  // ADR-040: 32-bit IEEE single; floatValue'yu tutar ama (float) truncate edilir
    Decimal,  // ADR-028: ondalık hassasiyet; decimalValue alanı taşır
    String,
    Ref,      // ADR-020: array/struct nesnesine Object* referansı
    Null,     // ADR-021: nullable referansın null değeri (saQut kaynağında `null`)
    Date,     // #88 (ADR-036): UTC epoch-ms; int64Value alanı taşır
};

struct Value {
    ValueKind    kind         = ValueKind::Int;
    int          intValue     = 0;
    double       floatValue   = 0.0;          // kind == Float için
    DecimalValue decimalValue;                 // kind == Decimal için (ADR-028)
    std::string  stringValue;                  // kind == String için
    Object*      ref          = nullptr;       // kind == Ref için
    long long    int64Value   = 0;             // kind == Date için (UTC epoch-ms)

    static Value fromInt(int n) {
        Value v; v.kind = ValueKind::Int; v.intValue = n; return v;
    }

    static Value fromLongInt(long long n) {
        Value v; v.kind = ValueKind::LongInt; v.int64Value = n; return v;
    }

    static Value fromDate(long long epochMs) {
        Value v; v.kind = ValueKind::Date; v.int64Value = epochMs; return v;
    }

    static Value fromFloat(double d) {
        Value v; v.kind = ValueKind::Float; v.floatValue = d; return v;
    }

    // ADR-040: 32-bit single — saklarken (float) truncate ederek gerçek precision
    static Value fromFloat32(double d) {
        Value v; v.kind = ValueKind::Float32; v.floatValue = (double)(float)d; return v;
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

    // ADR-040: sayısal karşılaştırma için ortak erişim. Int/LongInt tamsayı
    // alanından, Float/Float32 double alanından okur; karışık int↔float
    // karşılaştırmaları double üzerinden yapılır (typechecker izin verdiği ölçüde).
    bool isFloaty()  const { return kind == ValueKind::Float || kind == ValueKind::Float32; }
    bool isIntegral() const { return kind == ValueKind::Int || kind == ValueKind::LongInt; }
    long long asI64() const {
        return kind == ValueKind::LongInt ? int64Value : (long long)intValue;
    }
    double asDouble() const {
        if (isFloaty()) return floatValue;
        if (kind == ValueKind::LongInt) return (double)int64Value;
        return (double)intValue;
    }

    bool isTruthy() const {
        switch (kind) {
            case ValueKind::Int:     return intValue != 0;
            case ValueKind::LongInt: return int64Value != 0;
            case ValueKind::Float:   return floatValue != 0.0;
            case ValueKind::Float32: return floatValue != 0.0;
            case ValueKind::Decimal: return decimalValue.isTruthy();
            case ValueKind::String:  return !stringValue.empty();
            case ValueKind::Ref:     return ref != nullptr;
            case ValueKind::Null:    return false;
            case ValueKind::Date:    return true; // her zaman geçerli bir andı temsil eder
        }
        return false;
    }

    std::string toString() const {
        switch (kind) {
            case ValueKind::Int:     return std::to_string(intValue);
            case ValueKind::LongInt: return std::to_string(int64Value);
            case ValueKind::Decimal: return decimalValue.toString();
            case ValueKind::Float:
            case ValueKind::Float32: {
                // #114: biçim tek kaynaktan (core/float_format.hpp) — VM/JIT senkronu
                return (kind == ValueKind::Float32)
                    ? formatFloat32Print(floatValue)
                    : formatDoublePrint(floatValue);
            }
            case ValueKind::String: return stringValue;
            case ValueKind::Ref:   return "<ref>";
            case ValueKind::Null:  return "null";
            case ValueKind::Date:  return std::to_string(int64Value);
        }
        return "?";
    }

    std::string typeName() const {
        switch (kind) {
            case ValueKind::Int:     return "int";
            case ValueKind::LongInt: return "longint";
            case ValueKind::Float:   return "float";
            case ValueKind::Float32: return "float";
            case ValueKind::Decimal: return "decimal";
            case ValueKind::String:  return "string";
            case ValueKind::Ref:     return "ref";
            case ValueKind::Null:    return "null";
            case ValueKind::Date:    return "date";
        }
        return "?";
    }
};

#endif // SAQUT_VM_VALUE
