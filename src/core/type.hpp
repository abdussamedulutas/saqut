// ============================================================================
// saQut Compiler — Tip Sistemi (Type System)
// ============================================================================
//
// DİZİN:   src/core/type.hpp
// KATMAN:  Katman 0 — Tüm analiz katmanları tarafından kullanılır
// BAĞIMLI: Yok (sadece <string>, <vector>, <memory>)
// KULLANAN: Sembol tablosu (Faz 2), tip denetleyici (Faz 3), optimizasyon (Faz 4)
//
// AMAÇ:
//   Kaynak koddaki her ifadenin/sembolün veri tipini temsil eder. Derleyicinin
//   "bu değer ne?" sorusuna verdiği yapısal cevaptır. Tip, makine-okur (toJson)
//   ve insan-okur (toString) olarak dışa açıktır — "veri birincil, metin bir
//   görünümdür" ilkesine uyar (bkz. readme → Tasarım felsefesi).
//
// TİP TÜRLERİ (TypeKind):
//   Primitive : int, float, double, char, string, bool, void
//   Array     : eleman tipi taşır (örn. int[])
//   Struct    : struct adı taşır (örn. struct Point)
//   Function  : dönüş tipi + parametre tipleri taşır
//   Error     : hatalı/çözümlenememiş tip — ardışık sahte hataları bastırmak için
//               (tip denetleyici, operandı Error olan ifadede yeni hata üretmez)
//
// NOT (kasıtlı sadelik): Gizli tip dönüşümü YOKTUR (ADR-010). equals() yapısal
//   ve katıdır; "int, float'a uyar mı?" gibi kurallar tip denetleyicinin işidir,
//   bu dosyanın değil. Tamsayı literalinin bağlama-göre tiplenmesi de (ADR-010)
//   Faz 3'te ele alınır.
//
// ============================================================================

#ifndef SAQUT_CORE_TYPE
#define SAQUT_CORE_TYPE

#include <string>
#include <vector>
#include <memory>
#include "vendor/nlohmann/json.hpp"

// ============================================================================
// Enum'lar
// ============================================================================

enum class PrimitiveKind { Int, Float, Double, Char, String, Bool, Void };

enum class TypeKind { Primitive, Array, Struct, Function, Error };

// ============================================================================
// Type — Bir veri tipi
// ============================================================================
//
// KULLANIM:
//   Type a = Type::Int();                       // int
//   Type b = Type::array(Type::Int());          // int[]
//   Type c = Type::function(Type::Int(), {Type::Int(), Type::Int()}); // fn(int,int)->int
//   Type d = Type::structType("Point");         // struct Point
//   Type e = Type::error();                      // <error>
//
//   a.equals(Type::Int());   // true
//   a.equals(b);             // false
//   a.toString();            // "int"
//   b.toString();            // "int[]"
//   c.toJson();              // {"kind":"function",...}
//
// İç içe tipler (array elemanı, fonksiyon dönüşü) shared_ptr ile tutulur:
// Type değer-semantiğiyle kopyalanabilir kalır ama özyinelemeli olabilir.
// ============================================================================

struct Type {
    TypeKind kind = TypeKind::Error;

    PrimitiveKind         prim = PrimitiveKind::Void; // kind == Primitive
    std::shared_ptr<Type> elementType;                // kind == Array
    std::shared_ptr<Type> returnType;                 // kind == Function
    std::vector<Type>     paramTypes;                 // kind == Function
    std::string           structName;                 // kind == Struct

    // ------------------------------------------------------------------ //
    // Factory'ler
    // ------------------------------------------------------------------ //
    static Type primitive(PrimitiveKind p) {
        Type t;
        t.kind = TypeKind::Primitive;
        t.prim = p;
        return t;
    }
    static Type Int()    { return primitive(PrimitiveKind::Int); }
    static Type Float()  { return primitive(PrimitiveKind::Float); }
    static Type Double() { return primitive(PrimitiveKind::Double); }
    static Type Char()   { return primitive(PrimitiveKind::Char); }
    static Type String() { return primitive(PrimitiveKind::String); }
    static Type Bool()   { return primitive(PrimitiveKind::Bool); }
    static Type Void()   { return primitive(PrimitiveKind::Void); }

    static Type array(Type elem) {
        Type t;
        t.kind = TypeKind::Array;
        t.elementType = std::make_shared<Type>(std::move(elem));
        return t;
    }
    static Type function(Type ret, std::vector<Type> params) {
        Type t;
        t.kind = TypeKind::Function;
        t.returnType = std::make_shared<Type>(std::move(ret));
        t.paramTypes = std::move(params);
        return t;
    }
    static Type structType(std::string name) {
        Type t;
        t.kind = TypeKind::Struct;
        t.structName = std::move(name);
        return t;
    }
    static Type error() {
        return Type{}; // varsayılan = Error
    }

    // ------------------------------------------------------------------ //
    // Yüklemler (predicates)
    // ------------------------------------------------------------------ //
    bool isError()     const { return kind == TypeKind::Error; }
    bool isPrimitive() const { return kind == TypeKind::Primitive; }
    bool isArray()     const { return kind == TypeKind::Array; }
    bool isStruct()    const { return kind == TypeKind::Struct; }
    bool isFunction()  const { return kind == TypeKind::Function; }
    bool isVoid()      const { return kind == TypeKind::Primitive && prim == PrimitiveKind::Void; }

    // Aritmetik/karşılaştırma operatörlerine uygun sayısal tip mi?
    bool isNumeric() const {
        return kind == TypeKind::Primitive &&
               (prim == PrimitiveKind::Int ||
                prim == PrimitiveKind::Float ||
                prim == PrimitiveKind::Double);
    }

    // ------------------------------------------------------------------ //
    // equals — Yapısal eşitlik (katı; gizli dönüşüm yok, ADR-010)
    // ------------------------------------------------------------------ //
    bool equals(const Type& o) const {
        if (kind != o.kind) return false;
        switch (kind) {
            case TypeKind::Primitive:
                return prim == o.prim;
            case TypeKind::Array:
                return elementType && o.elementType &&
                       elementType->equals(*o.elementType);
            case TypeKind::Struct:
                return structName == o.structName;
            case TypeKind::Function: {
                if (!returnType || !o.returnType) return false;
                if (!returnType->equals(*o.returnType)) return false;
                if (paramTypes.size() != o.paramTypes.size()) return false;
                for (size_t i = 0; i < paramTypes.size(); ++i)
                    if (!paramTypes[i].equals(o.paramTypes[i])) return false;
                return true;
            }
            case TypeKind::Error:
                // Error == Error: ardışık sahte hataların bastırılması tip
                // denetleyicinin sorumluluğundadır (operandı Error ise hata üretme).
                return true;
        }
        return false; // erişilemez (tüm enum değerleri kapsandı)
    }

    // ------------------------------------------------------------------ //
    // İsim yardımcıları
    // ------------------------------------------------------------------ //
    static const char* primName(PrimitiveKind p) {
        switch (p) {
            case PrimitiveKind::Int:    return "int";
            case PrimitiveKind::Float:  return "float";
            case PrimitiveKind::Double: return "double";
            case PrimitiveKind::Char:   return "char";
            case PrimitiveKind::String: return "string";
            case PrimitiveKind::Bool:   return "bool";
            case PrimitiveKind::Void:   return "void";
        }
        return "?";
    }

    // Bir tip adından (parser tipleri string olarak tutar) primitif Type üretir.
    // Bilinen primitif değilse Error döner — bilinmeyen tip adının teşhisi
    // (E007) çağıranın (Faz 2/3) işidir; bu fonksiyon sessizce Error verir.
    static Type fromName(const std::string& n) {
        if (n == "int")    return Int();
        if (n == "float")  return Float();
        if (n == "double") return Double();
        if (n == "char")   return Char();
        if (n == "string") return String();
        if (n == "bool")   return Bool();
        if (n == "void")   return Void();
        // "int[]", "float[]" vb. — suffix [] ile dizi tipi
        if (n.size() > 2 && n.substr(n.size() - 2) == "[]") {
            Type elem = fromName(n.substr(0, n.size() - 2));
            if (!elem.isError()) return array(elem);
        }
        return error();
    }

    // ------------------------------------------------------------------ //
    // toString — İnsan-okur ("int", "int[]", "fn(int,int)->int")
    // ------------------------------------------------------------------ //
    std::string toString() const {
        switch (kind) {
            case TypeKind::Primitive:
                return primName(prim);
            case TypeKind::Array:
                return (elementType ? elementType->toString() : "<?>") + "[]";
            case TypeKind::Struct:
                return "struct " + structName;
            case TypeKind::Function: {
                std::string s = "fn(";
                for (size_t i = 0; i < paramTypes.size(); ++i) {
                    if (i) s += ",";
                    s += paramTypes[i].toString();
                }
                s += ")->";
                s += returnType ? returnType->toString() : "<?>";
                return s;
            }
            case TypeKind::Error:
                return "<error>";
        }
        return "<?>";
    }

    // ------------------------------------------------------------------ //
    // toJson — Makine-okur (cam ilkesi: her tip dışarıdan sorgulanabilir)
    // ------------------------------------------------------------------ //
    nlohmann::json toJsonObj() const {
        nlohmann::json j;
        switch (kind) {
            case TypeKind::Primitive:
                j["kind"] = "primitive";
                j["name"] = primName(prim);
                break;
            case TypeKind::Array:
                j["kind"]    = "array";
                j["element"] = elementType ? elementType->toJsonObj() : nullptr;
                break;
            case TypeKind::Struct:
                j["kind"] = "struct";
                j["name"] = structName;
                break;
            case TypeKind::Function: {
                j["kind"]    = "function";
                j["returns"] = returnType ? returnType->toJsonObj() : nullptr;
                nlohmann::json params = nlohmann::json::array();
                for (const auto& p : paramTypes) params.push_back(p.toJsonObj());
                j["params"] = params;
                break;
            }
            case TypeKind::Error:
                j["kind"] = "error";
                break;
        }
        return j;
    }

    std::string toJson() const { return toJsonObj().dump(); }
};

#endif // SAQUT_CORE_TYPE
