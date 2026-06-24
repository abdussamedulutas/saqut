// ============================================================================
// saQut — Merkezi Built-in Method Registry
//
// Sözdizimi: ElementTipi::method(args)  →  int::push(arr, 12)
//
// Üç tüketici bu tablodan okur; tabloya YAZMA yapmaz:
//   - TypeChecker   (imza doğrulama + dönüş tipi çözme)
//   - SymbolTable resolver (built-in çözümleme)
//   - LSP           (otomatik tamamlama + hata mesajı)
//
// Kategori:
//   Array      — herhangi bir E[] üzerinde: E::push, E::pop, E::length ...
//   StringVal  — string değeri üzerinde:   string::upper, string::split ...
//   StructVal  — struct değeri üzerinde:   S::toJson, S::dump
// ============================================================================

#ifndef SAQUT_BUILTIN_METHODS
#define SAQUT_BUILTIN_METHODS

#include <string>
#include <vector>
#include <unordered_map>
#include "core/type.hpp"

// ── Parametre kuralı ─────────────────────────────────────────────────────────
// Her argümanın beklenen tipi nasıl hesaplanır?

enum class ParamKind {
    Fixed,      // sabit tip (ör. int indeks, string ayraç)
    ElemType,   // E tipi — çağrının sol tarafından türetilir  (ör. push 2. arg)
    ElemArray,  // E[] tipi — çağrının sol tarafından türetilir (ör. push 1. arg)
    StringVal,  // string tipi — StringVal metodlarının receiver'ı için
};

struct ParamRule {
    ParamKind kind;
    Type      fixedType; // kind == Fixed ise kullanılır
};

inline ParamRule fixedParam(Type t)    { return { ParamKind::Fixed,     std::move(t) }; }
inline ParamRule elemParam()           { return { ParamKind::ElemType,  {} }; }
inline ParamRule elemArrayParam()      { return { ParamKind::ElemArray, {} }; }
inline ParamRule stringValParam()      { return { ParamKind::StringVal, {} }; }

// ── Dönüş tipi kuralı ────────────────────────────────────────────────────────

enum class ReturnKind {
    Fixed,      // sabit tip (ör. int, bool, void, string)
    ElemType,   // E döner  (ör. pop, remove)
    ElemArray,  // E[] döner (ör. slice, concat, reverse)
};

struct ReturnRule {
    ReturnKind kind;
    Type       fixedType; // kind == Fixed ise kullanılır
};

inline ReturnRule fixedReturn(Type t)  { return { ReturnKind::Fixed,     std::move(t) }; }
inline ReturnRule elemReturn()         { return { ReturnKind::ElemType,  {} }; }
inline ReturnRule elemArrayReturn()    { return { ReturnKind::ElemArray, {} }; }

// ── Method kategorisi ────────────────────────────────────────────────────────

enum class MethodCategory {
    Array,      // E[] receiver — herhangi bir element tipi için
    StringVal,  // string receiver — yalnızca string tipi için
    StructVal,  // S receiver — tanımlı struct tipleri için (toJson, dump)
};

// ── Tek metod kaydı ──────────────────────────────────────────────────────────

struct BuiltinMethod {
    std::string            name;
    MethodCategory         category;
    // params[0] = receiver (ElemArray/StringVal/ElemType), params[1..] = diğer argümanlar
    std::vector<ParamRule> params;
    ReturnRule             ret;
    bool                   mutating;   // true → receiver'ı yerinde değiştirir
    int                    runtimeId;  // VM dispatch tablosundaki sabit index (registry doldurur)
};

// ── Registry ─────────────────────────────────────────────────────────────────

class BuiltinMethodRegistry {
public:
    static const BuiltinMethodRegistry& instance() {
        static BuiltinMethodRegistry reg;
        return reg;
    }

    // TypeChecker ve IR codegen çağırır.
    // leftName: "int", "string", "Person", ...
    // isStruct: TypeChecker hasStruct() sonucu — struct array metodları için gerekli
    // Dönüş: nullptr = bu tip için metod yok
    const BuiltinMethod* lookup(const std::string& leftName,
                                const std::string& methodName,
                                bool               isStruct,
                                bool               isReceiverArray) const
    {
        if (!isReceiverArray) {
            // 1. String value metodları (string:: → upper, lower, ...)
            if (leftName == "string") {
                auto it = byName_.find("sv:" + methodName);
                if (it != byName_.end()) return &all_[it->second];
            }
            // 2. Struct value metodları (S:: → toJson, dump)
            if (isStruct) {
                auto it = byName_.find("st:" + methodName);
                if (it != byName_.end()) return &all_[it->second];
            }
        }
        // 3. Array metodları (herhangi E[] için, hem scalar hem struct array)
        auto it = byName_.find("ar:" + methodName);
        if (it != byName_.end()) return &all_[it->second];
        return nullptr;
    }

    // runtimeId'den metodu getir (VM ve IR codegen için)
    const BuiltinMethod* byId(int id) const {
        if (id < 0 || id >= (int)all_.size()) return nullptr;
        return &all_[id];
    }

    int count() const { return (int)all_.size(); }

    // TypeChecker yardımcısı: leftName'den E tipini çöz
    // "int" → int   "string" → string   "Person" → struct Person
    static Type resolveElemType(const std::string& leftName) {
        Type t = Type::fromName(leftName);
        if (!t.isError()) return t;
        // Struct tipi — fromName tanımıyor ama structType ile üretilebilir
        if (!leftName.empty() && std::isupper((unsigned char)leftName[0]))
            return Type::structType(leftName);
        return Type::error();
    }

private:
    BuiltinMethodRegistry() { init(); }

    void reg(MethodCategory cat, const char* prefix,
             std::string name,
             std::vector<ParamRule> params,
             ReturnRule ret,
             bool mutating)
    {
        BuiltinMethod m;
        m.name      = name;
        m.category  = cat;
        m.params    = std::move(params);
        m.ret       = std::move(ret);
        m.mutating  = mutating;
        m.runtimeId = (int)all_.size();
        byName_[std::string(prefix) + ":" + name] = m.runtimeId;
        all_.push_back(std::move(m));
    }

    void init() {
        // ── Array metodları (category=Array, prefix="ar") ──────────────────
        // E::length(E[]) -> int
        reg(MethodCategory::Array, "ar", "length",
            { elemArrayParam() },
            fixedReturn(Type::Int()), false);

        // E::push(E[], E) -> int   (eklenen öğenin indeksi döner)
        reg(MethodCategory::Array, "ar", "push",
            { elemArrayParam(), elemParam() },
            fixedReturn(Type::Int()), true);

        // E::pop(E[]) -> E
        reg(MethodCategory::Array, "ar", "pop",
            { elemArrayParam() },
            elemReturn(), true);

        // E::insert(E[], int, E) -> int   (eklenen indeks döner)
        reg(MethodCategory::Array, "ar", "insert",
            { elemArrayParam(), fixedParam(Type::Int()), elemParam() },
            fixedReturn(Type::Int()), true);

        // E::remove(E[], int) -> E
        reg(MethodCategory::Array, "ar", "remove",
            { elemArrayParam(), fixedParam(Type::Int()) },
            elemReturn(), true);

        // E::slice(E[], int, int) -> E[]   (yeni array)
        reg(MethodCategory::Array, "ar", "slice",
            { elemArrayParam(), fixedParam(Type::Int()), fixedParam(Type::Int()) },
            elemArrayReturn(), false);

        // E::reverse(E[]) -> E[]   (yerinde; aynı referansı döner)
        reg(MethodCategory::Array, "ar", "reverse",
            { elemArrayParam() },
            elemArrayReturn(), true);

        // E::concat(E[], E[]) -> E[]   (yeni array)
        reg(MethodCategory::Array, "ar", "concat",
            { elemArrayParam(), elemArrayParam() },
            elemArrayReturn(), false);

        // E::contains(E[], E) -> bool
        reg(MethodCategory::Array, "ar", "contains",
            { elemArrayParam(), elemParam() },
            fixedReturn(Type::Bool()), false);

        // E::indexOf(E[], E) -> int?   (bulamazsa null)
        reg(MethodCategory::Array, "ar", "indexOf",
            { elemArrayParam(), elemParam() },
            fixedReturn(Type::Int().asNullable()), false);

        // E::clear(E[]) -> void
        reg(MethodCategory::Array, "ar", "clear",
            { elemArrayParam() },
            fixedReturn(Type::Void()), true);

        // ── String value metodları (category=StringVal, prefix="sv") ───────
        // string::length(string) -> int
        reg(MethodCategory::StringVal, "sv", "length",
            { stringValParam() },
            fixedReturn(Type::Int()), false);

        // string::upper(string) -> string
        reg(MethodCategory::StringVal, "sv", "upper",
            { stringValParam() },
            fixedReturn(Type::String()), false);

        // string::lower(string) -> string
        reg(MethodCategory::StringVal, "sv", "lower",
            { stringValParam() },
            fixedReturn(Type::String()), false);

        // string::trim(string) -> string
        reg(MethodCategory::StringVal, "sv", "trim",
            { stringValParam() },
            fixedReturn(Type::String()), false);

        // string::split(string, string) -> string[]
        reg(MethodCategory::StringVal, "sv", "split",
            { stringValParam(), fixedParam(Type::String()) },
            fixedReturn(Type::array(Type::String())), false);

        // string::substring(string, int, int) -> string
        reg(MethodCategory::StringVal, "sv", "substring",
            { stringValParam(), fixedParam(Type::Int()), fixedParam(Type::Int()) },
            fixedReturn(Type::String()), false);

        // string::replace(string, string, string) -> string
        reg(MethodCategory::StringVal, "sv", "replace",
            { stringValParam(), fixedParam(Type::String()), fixedParam(Type::String()) },
            fixedReturn(Type::String()), false);

        // string::repeat(string, int) -> string
        reg(MethodCategory::StringVal, "sv", "repeat",
            { stringValParam(), fixedParam(Type::Int()) },
            fixedReturn(Type::String()), false);

        // string::charAt(string, int) -> string
        reg(MethodCategory::StringVal, "sv", "charAt",
            { stringValParam(), fixedParam(Type::Int()) },
            fixedReturn(Type::String()), false);

        // string::indexOf(string, string) -> int?
        reg(MethodCategory::StringVal, "sv", "indexOf",
            { stringValParam(), fixedParam(Type::String()) },
            fixedReturn(Type::Int().asNullable()), false);

        // string::contains(string, string) -> bool
        reg(MethodCategory::StringVal, "sv", "contains",
            { stringValParam(), fixedParam(Type::String()) },
            fixedReturn(Type::Bool()), false);

        // string::startsWith(string, string) -> bool
        reg(MethodCategory::StringVal, "sv", "startsWith",
            { stringValParam(), fixedParam(Type::String()) },
            fixedReturn(Type::Bool()), false);

        // string::endsWith(string, string) -> bool
        reg(MethodCategory::StringVal, "sv", "endsWith",
            { stringValParam(), fixedParam(Type::String()) },
            fixedReturn(Type::Bool()), false);

        // ── Struct value metodları (category=StructVal, prefix="st") ───────
        // S::toJson(S) -> string   (makine-okunur JSON)
        reg(MethodCategory::StructVal, "st", "toJson",
            { elemParam() },
            fixedReturn(Type::String()), false);

        // S::dump(S) -> string    (insan-okunur debug)
        reg(MethodCategory::StructVal, "st", "dump",
            { elemParam() },
            fixedReturn(Type::String()), false);
    }

    std::vector<BuiltinMethod>             all_;
    std::unordered_map<std::string, int>   byName_; // "prefix:name" → index
};

#endif // SAQUT_BUILTIN_METHODS
