// ============================================================================
// saQut Compiler — Temiz JSON Üretici (JsonObject)
// ============================================================================
//
// DİZİN:   src/parser/ast_json.hpp
// KATMAN:  AST — Sadece AST düğümlerinin toJson() metotları için
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
// ============================================================================

#ifndef SAQUT_AST_JSON
#define SAQUT_AST_JSON

#include <string>
#include <sstream>

// Girinti sabiti (tools.hpp'deki jsonIndent ile uyumlu)
#define JSON_INDENT 2

// jsonEscape ve jsonIndent tools.hpp'de tanımlıdır.

// ============================================================================
// JsonObject — JSON Nesne Builder
// ============================================================================
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
// ============================================================================

class JsonObject {
public:
    JsonObject(int depth)
        : m_indent(jsonIndent(depth)),
          m_indentInner(jsonIndent(depth + 1))
    {
        m_ss << m_indent << "{\n";
    }

    // String alan ekle (değer tırnak içinde yazılır)
    void add(const std::string& key, const std::string& value) {
        addRaw(key, "\"" + jsonEscape(value) + "\"");
    }

    // Sayısal alan ekle (değer olduğu gibi yazılır)
    void add(const std::string& key, int value) {
        addRaw(key, std::to_string(value));
    }

    // Boolean alan ekle
    void add(const std::string& key, bool value) {
        addRaw(key, value ? "true" : "false");
    }

    // Ham JSON değeri ekle (önceden formatlanmış, tırnaklanmamış)
    void addRaw(const std::string& key, const std::string& jsonValue) {
        if (m_hasFields) m_ss << ",\n";
        m_ss << m_indentInner << "\"" << jsonEscape(key) << "\": " << jsonValue;
        m_hasFields = true;
    }

    // Alt nesne ekle (bir alt seviyede JSON nesnesi)
    void addNested(const std::string& key, const std::string& nestedJson) {
        addRaw(key, nestedJson);
    }

    // Koşullu string alan (value boş değilse ekle)
    void addIfNotEmpty(const std::string& key, const std::string& value) {
        if (!value.empty()) add(key, value);
    }

    // Koşullu sayı alan (value varsayılandan farklıysa ekle)
    void addIfNot(const std::string& key, int value, int defaultValue) {
        if (value != defaultValue) add(key, value);
    }

    // Dizi alanı (callback içinde addItem çağrılır)
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

    // Diziye eleman ekle (addArray callback'i içinde kullanılır)
    void addItem(const std::string& itemJson) {
        if (m_hasArrayItem) m_ss << ",";
        // Öğeler m_indentInner'in bir seviye altında (depth + 2)
        std::string itemIndent = "";
        itemIndent.append(m_indentInner.size() + 2, ' ');
        m_ss << "\n" << itemIndent << itemJson;
        m_hasArrayItem = true;
    }

    // Nesneyi kapat ve string olarak döndür
    std::string str() {
        m_ss << "\n" << m_indent << "}";
        return m_ss.str();
    }

private:
    std::ostringstream m_ss;
    std::string m_indent;       // Bu nesnenin girintisi
    std::string m_indentInner;  // Bir alt seviye girinti
    bool m_hasFields = false;
    int m_arrayDepth = 0;       // İç içe dizi seviyesi
    bool m_hasArrayItem = false;
};

#endif // SAQUT_AST_JSON
