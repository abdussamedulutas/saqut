// ============================================================================
// saQut Compiler — Temiz JSON Üretici (JsonObject)
// ============================================================================
//
// DİZİN:   src/parser/ast_json.hpp
// KATMAN:  Katman 3 — Parser (AST JSON serileştirme)
// AMAÇ:    AST düğümlerinin toJson() metotlarında kullanılan builder pattern
// BAĞIMLI: Yok (sadece <string>, <sstream>)
//
// AMAÇ:
//   AST düğümlerinin toJson() metotlarını okunabilir kılmak.
//   stringstream'i el ile yönetmek yerine builder pattern kullanır.
//
// KULLANIM:
//   JsonObject obj(depth);
//   obj.add("kind", "Literal");
//   obj.add("value", 42);
//   obj.add("location", loc.toJson());  // ham JSON gömme
//   return obj.str();
//
// TASARIM KARARLARI:
//   1. Builder pattern: add() çağrıları zincirlenemez ama okunabilirlik kazanır.
//      Zincirleme için: return obj.add("a",1).add("b",2).str() — tercih edilmedi.
//   2. addRaw(): Önceden formatlanmış JSON (alt düğüm çıktısı) gömmek için.
//   3. addArray(): Callback ile dizi oluşturma — C++ lambda'ları sayesinde temiz.
//   4. addIfNotEmpty/addIfNot: Koşullu alanlar — null alanları JSON'da göstermemek için.
//      JSON çıktısını temiz tutar.
//   5. JSON_INDENT = 2: Standart JSON girinti (4 değil, 2 okunabilir).
//
// ============================================================================

#ifndef SAQUT_AST_JSON
#define SAQUT_AST_JSON

#include <string>
#include <sstream>

// ============================================================================
// JSON_INDENT — JSON girinti miktarı (boşluk sayısı)
// ============================================================================
// tools.hpp'deki jsonIndent() ile uyumlu olmalıdır.
// Her seviyede 2 boşluk içe kaydırılır.
#define JSON_INDENT 2

// jsonEscape ve jsonIndent tools.hpp'de tanımlıdır.

// ============================================================================
// JsonObject — JSON Nesne Builder
// ============================================================================
//
// AST düğümlerini JSON formatına dönüştürmek için kullanılır.
// Her çağrıda yeni bir JsonObject oluşturulur, alanlar eklenir ve str() ile
// JSON stringi alınır.
//
// KULLANIM:
//   JsonObject obj(depth);
//   obj.add("kind", "FunctionDecl");
//   obj.add("name", name);
//   obj.add("returnType", returnType);
//   obj.addRaw("location", loc.toJson());     // önceden formatlanmış JSON
//   obj.addArray("children", [&] {            // alt düğümler
//       for (auto* child : children)
//           obj.addChild(child->toJson(depth + 2));
//   });
//   return obj.str();
//
// ÖRNEK ÇIKTI (depth=0):
//   {
//     "kind": "FunctionDecl",
//     "name": "main",
//     "returnType": "int",
//     "children": [ ... ]
//   }
//
// ============================================================================

class JsonObject {
public:
    // JsonObject — Yapıcı
    // PARAMETRE: depth — JSON girinti seviyesi (0 = en dış)
    // YAN ETKİ:  m_ss'e açılış süslü parantezi yazar
    JsonObject(int depth)
        : m_indent(jsonIndent(depth)),
          m_indentInner(jsonIndent(depth + 1))
    {
        m_ss << m_indent << "{\n";
    }

    // add() — String alan ekle
    // PARAMETRELER:
    //   key   — JSON anahtarı (tırnak içinde yazılır)
    //   value — string değer (otomatik tırnaklanır ve escape edilir)
    // YAN ETKİ: m_hasFields true olur
    // ÖRN:   obj.add("name", "main") → "name": "main"
    void add(const std::string& key, const std::string& value) {
        addRaw(key, "\"" + jsonEscape(value) + "\"");
    }

    // add() — Sayısal alan ekle
    // PARAMETRELER:
    //   key   — JSON anahtarı
    //   value — tamsayı değer (tırnaklanmaz, olduğu gibi yazılır)
    // ÖRN:   obj.add("line", 42) → "line": 42
    void add(const std::string& key, int value) {
        addRaw(key, std::to_string(value));
    }

    // add() — Boolean alan ekle
    // PARAMETRELER:
    //   key   — JSON anahtarı
    //   value — true/false
    // ÖRN:   obj.add("isPublic", true) → "isPublic": true
    void add(const std::string& key, bool value) {
        addRaw(key, value ? "true" : "false");
    }

    // addRaw() — Ham JSON değeri ekle (önceden formatlanmış)
    // PARAMETRELER:
    //   key       — JSON anahtarı
    //   jsonValue — önceden JSON'a çevrilmiş değer (tırnaklanmaz!)
    // KULLANIM:   Alt düğüm toJson() çıktısını gömmek için.
    //             addRaw("location", loc.toJson());
    void addRaw(const std::string& key, const std::string& jsonValue) {
        if (m_hasFields) m_ss << ",\n";
        m_ss << m_indentInner << "\"" << jsonEscape(key) << "\": " << jsonValue;
        m_hasFields = true;
    }

    // addNested() — Alt nesne ekle (addRaw alias)
    // PARAMETRELER: addRaw ile aynı
    // KULLANIM: addRaw ile aynı. Sadece okunabilirlik için.
    void addNested(const std::string& key, const std::string& nestedJson) {
        addRaw(key, nestedJson);
    }

    // addIfNotEmpty() — Koşullu string alan
    // PARAMETRELER:
    //   key   — JSON anahtarı
    //   value — string değer (sadece boş DEĞİLSE eklenir)
    // KULLANIM: Opsiyonel alanlar için. JSON çıktısını temiz tutar.
    //           obj.addIfNotEmpty("defaultValue", defaultVal);
    void addIfNotEmpty(const std::string& key, const std::string& value) {
        if (!value.empty()) add(key, value);
    }

    // addIfNot() — Koşullu sayı alan
    // PARAMETRELER:
    //   key          — JSON anahtarı
    //   value        — mevcut değer
    //   defaultValue — varsayılan değer
    // EKLEME KOŞULU: value != defaultValue
    // KULLANIM: Varsayılan değerler JSON'da tekrarlanmaz.
    //           obj.addIfNot("precedence", 0, 14);
    void addIfNot(const std::string& key, int value, int defaultValue) {
        if (value != defaultValue) add(key, value);
    }

    // addArray() — Dizi alanı (callback ile)
    // PARAMETRELER:
    //   key      — JSON anahtarı
    //   callback — dizi elemanlarını addItem ile ekleyen lambda/fonksiyon
    // KULLANIM:
    //   obj.addArray("children", [&] {
    //       for (auto* child : children)
    //           obj.addItem(child->toJson(depth + 2));
    //   });
    // ÖRNEK ÇIKTI:
    //   "children": [
    //     { "kind": "Literal", ... },
    //     { "kind": "Identifier", ... }
    //   ]
    template<typename Fn>
    void addArray(const std::string& key, Fn callback) {
        if (m_hasFields) m_ss << ",\n";
        m_ss << m_indentInner << "\"" << jsonEscape(key) << "\": [\n";
        m_arrayDepth++;
        callback();
        m_arrayDepth--;
        m_ss << "\n" << m_indentInner << "]";
        m_hasFields = true;
    }

    // addItem() — Diziye eleman ekle
    // PARAMETRE: itemJson — JSON formatında dizi elemanı
    // KULLANIM:  Sadece addArray callback'i içinde kullanılır.
    // YAN ETKİ:  m_hasArrayItem true olur (virgül kontrolü için)
    void addItem(const std::string& itemJson) {
        if (m_hasArrayItem) m_ss << ",";
        // Öğeler m_indentInner'in bir seviye altında (depth + 2)
        std::string itemIndent = "";
        itemIndent.append(m_indentInner.size() + 2, ' ');
        m_ss << "\n" << itemIndent << itemJson;
        m_hasArrayItem = true;
    }

    // str() — JSON nesnesini kapat ve string olarak döndür
    // DÖNÜŞ:  Tam JSON stringi ({"key": "value", ...})
    // YAN ETKİ: Kapanış süslü parantezini ekler.
    // KULLANIM:
    //   JsonObject obj(depth);
    //   obj.add("kind", "FunctionDecl");
    //   return obj.str();
    std::string str() {
        m_ss << "\n" << m_indent << "}";
        return m_ss.str();
    }

private:
    /* ====== Builder State ====== */
    std::ostringstream m_ss;      // JSON çıktısının biriktirildiği string stream

    std::string m_indent;         // Bu nesnenin girinti seviyesi (depth * 2 boşluk)
                                  //   Örn: depth=0 → "", depth=1 → "  "

    std::string m_indentInner;    // Bir alt seviye girinti ((depth+1) * 2 boşluk)
                                  //   Örn: depth=0 → "  ", depth=1 → "    "

    bool m_hasFields = false;     // Alan eklendi mi? (virgül kontrolü için)
                                  //   true ise bir sonraki alandan önce virgül + newline

    int m_arrayDepth = 0;         // İç içe dizi seviyesi (şu anda kullanılmıyor,
                                  //   ileride çok boyutlu diziler için)

    bool m_hasArrayItem = false;  // Diziye eleman eklendi mi? (virgül kontrolü)
                                  //   true ise bir sonraki elemandan önce virgül
};

#endif // SAQUT_AST_JSON
