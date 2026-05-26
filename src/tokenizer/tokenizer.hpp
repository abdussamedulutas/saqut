// ============================================================================
// saQut Compiler — Tokenizer (Token Seviyesinde Tarayıcı)
// ============================================================================
//
// DİZİN:   src/tokenizer/tokenizer.hpp
// KATMAN:  Katman 2 — Lexer üzerine kurulu
// BAĞIMLI: Lexer (src/lexer/lexer.hpp)
// KULLANAN: Parser (src/parser/parser.hpp), ParserToken (src/parser/token.hpp)
//
// AMAÇ:
//   Lexer tarafından sağlanan karakter akışını alıp anlamlı token'lara dönüştürür.
//   Token'lar derleyicinin "kelime"leridir — parser'ın anlayacağı en küçük birim.
//
//   Üretilen token tipleri (6 adet polimorfik sınıf):
//   ┌─────────────────┬──────────────────────────────────┐
//   │ Sınıf           │ Örnek token'lar                  │
//   ├─────────────────┼──────────────────────────────────┤
//   │ NumberToken     │ 42, 0xFF, 3.14, 1e10            │
//   │ StringToken     │ "merhaba", "satır\niki"         │
//   │ OperatorToken   │ +, -, *, /, ==, !=, ++, --      │
//   │ DelimiterToken  │ (, ), {, }, [, ], ;, ,, ., ->   │
//   │ KeywordToken    │ if, for, while, int, void        │
//   │ IdentifierToken │ x, myVar, _private               │
//   └─────────────────┴──────────────────────────────────┘
//
// ADR-004: Neden Polimorfik Token Sınıfları?
//   Seçenek 1 — Tagged union (std::variant): Tüm veriyi tek struct'ta
//     +: Bellek tek parça, cache-friendly
//     -: Tip eklemek için union'ı değiştirmek gerek
//   Seçenek 2 — Class hierarchy (seçilen): Base Token, alt sınıflar
//     +: Yeni token tipi eklemek kolay (yeni sınıf türet)
//     +: Her token kendi verisini taşır (NumberToken.isFloat, StringToken.context)
//     -: Heap tahsisi (new) gerektirir
//     -: virtual destructor çağrısı (maliyet: 1 vtable lookup)
//
//   Karar: Class hierarchy. Derleyici gibi bir araçta kod netliği ve
//   genişletilebilirlik, mikro-performanstan daha önemlidir.
//
// TASARIM KARARLARI:
//   1. Tablolar (operators, delimiters, keywords): constexpr std::string_view dizileri.
//      Derleme zamanında sabit, heap tahsisi yok. Sıralama önceliği:
//      - Önce keyword'ler: if/for/while gibi kelimeler identifier'lardan önce yakalanmalı
//      - Sonra delimiter'lar: -> ve :: gibi 2 karakterliler önce, tek karakterliler sonra
//      - Sonra operator'ler: != ve == gibi 2 karakterliler önce, tek karakterliler sonra
//      - En son identifier: yukarıdakilerden hiçbirine uymayan her şey
//
//   2. Keyword boundary check: "do" keyword'ü "double" ile karışmasın diye,
//      keyword eşleşmesinden sonraki karakter kontrol edilir. Sonraki karakter
//      harf/rakam/_/$ ise bu bir keyword değil, identifier'dır.
//
//   3. scope() metodu: Her çağrıldığında bir sonraki token'ı döndürür.
//      EOF'da "EOL" isimli özel bir token döndürür (Token tipi, özel değil).
//      Bu, boş token listesi sorununu çözer (parser her zaman bir token görür).
//
//   4. Yorum satırları: // (tek satır) ve /* */ (çok satırlı) desteklenir.
//      Yorumlar token üretmez, sessizce atlanır.
//      NOT: İç içe /* */ yorumları desteklenmez (C standardı gibi).
//
// BİLİNEN SINIRLAMALAR (TODO):
//   TODO: String escape sequence'leri tam değil (\x, \u, \U eksik)
//   TODO: Char literal: 'a' formatı okunamıyor
//   TODO: Raw string: R"(...)" formatı yok
//   TODO: Token konum bilgisi (satır/sütun) token'lara eklenmeli
//   TODO: Bellek sızıntısı: Token'lar heap'te new ile oluşturuluyor, silme sorumluluğu çağıranda
//
// ============================================================================

#ifndef SAQUT_TOKENIZER
#define SAQUT_TOKENIZER

#include <iostream>
#include <string>
#include <vector>
#include "lexer/lexer.hpp"

#include "tokenizer/token.hpp"

// ============================================================================
// Token Tanıma Tabloları (Derleme Zamanı Sabitleri)
// ============================================================================
//
// Bu tablolar, Tokenizer::scope() tarafından ham karakterlerden token üretmek
// için kullanılır. constexpr std::string_view ile tanımlanmıştır, böylece
// heap tahsisi yapılmaz ve derleme zamanında optimize edilir.
//
// SIRALAMA ÖNEMLİDİR!
//   scope() fonksiyonu bu tabloları sırasıyla tarar ve İLK eşleşmede durur.
//   Bu nedenle:
//   - Çok karakterli operatörler (==) tek karakterlilerden (=) ÖNCE gelmeli
//   - Çok karakterli delimiter'lar (->) tek karakterlilerden (.) ÖNCE gelmeli
//   - Keyword'ler, identifier'lardan ÖNCE kontrol edilmeli
//
//   Mevcut sıralama: keywords → delimiters → operators → identifier (fallback)
//
// ============================================================================

#include <string_view>

// Operatör tablosu. Çok karakterliler (==, !=, ++, +=, vb.) önce gelir.
// NOT: Bu tablo ParserToken'daki OPERATOR_MAP ile eşleşmelidir.
inline constexpr std::string_view operators[] = {
    // --- 2 karakterli: karşılaştırma ---
    "==", "!=", "<=", ">=", "&&", "||",
    // --- 2 karakterli: aritmetik ---
    "++", "--", "<<", ">>",
    // --- 2 karakterli: birleşik atama ---
    "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=",
    // --- 1 karakterli: aritmetik ---
    "+",  "-",  "*",  "/",  "%",  "<",  ">",
    // --- 1 karakterli: bitsel/mantıksal ---
    "^",  "!",  "~",  "&",  "|",
    // --- 1 karakterli: temel atama ---
    "="
};

// Delimiter tablosu. Çok karakterliler (->, ::) önce gelir.
inline constexpr std::string_view delimiters[] = {
    "->", "::",                              // 2 karakterli bağlayıcılar
    "[",  "]",  "(",  ")",  "{",  "}",       // gruplama
    ";",  ",",  ":",                          // ayırıcılar
    "."                                        // üye erişimi
};

// Keyword tablosu. Dilin tüm rezerve edilmiş kelimeleri.
// Gruplandırılmıştır:
//   - Kontrol akışı: if, else, for, while, do, switch, case, vb.
//   - Tipler: void, int, float, double, char, string, bool
//   - Literal'lar: true, false, null
//   - OOP: class, interface, enum, extends, public, private, vb.
//   - Modüller: import, package
//   - C/C++ ekleri: const, extern, typedef, sizeof, auto, vb.
//
// BUG FIX (commit 438bc0e):
//   Eskiden tip keyword'leri bu listede yoktu. int, float gibi kelimeler
//   identifier olarak tokenize ediliyordu. Parser'da KW_INT gibi tipler
//   tanımlı olmasına rağmen tokenizer'dan gelmediği için değişken tanımlama
//   çalışmıyordu. Tüm eksik keyword'ler eklendi.
//
inline constexpr std::string_view keywords[] = {
    // Control flow
    "if",       "else",     "for",      "while",    "do",
    "switch",   "case",     "default",  "break",    "continue",
    "return",   "try",      "catch",    "finally",  "throw",
    "throws",   "assert",
    // Types
    "void",     "int",      "float",    "double",   "char",
    "string",   "bool",
    // Literals
    "true",     "false",    "null",
    // OOP
    "class",    "struct",   "interface","enum",     "extends",  "implements",
    "new",      "public",   "private",  "protected",
    "static",   "final",    "abstract",
    // Modules
    "import",   "package",
    // C/C++
    "const",    "extern",   "typedef",  "sizeof",
    "auto",     "constexpr","noexcept",
    "native",   "synchronized", "volatile", "transient"
};

// ============================================================================
// Tokenizer — Lexer Üzerinde Token Üretici
// ============================================================================
//
// Tek sorumluluğu: karakter akışından token listesi üretmek.
// Durum bilgisi: Lexer'ı içerir (hmx), kendi durumu yok.
//
// KULLANIM:
//   Tokenizer tokenizer;
//   auto tokens = tokenizer.scan(sourceCode);
//   // tokens artık kullanılabilir. İş bitince:
//   for (auto* t : tokens) delete t;
//
class Tokenizer {
public:
    Lexer hmx;  // İç Lexer. "hmx" adı tarihsel.

    std::vector<Token*> scan(std::string input);

private:
    Token*           scope();             // Bir sonraki token'ı döndür
    IdentifierToken* readIdentifier();    // Tanımlayıcı oku
    StringToken*     readString();        // String literal oku
    void skipOneLineComment();            // // yorum satırını atla
    void skipMultiLineComment();          // /* */ yorum bloğunu atla
};

// ============================================================================
// GERÇEKLEME (Implementation)
// ============================================================================

// --------------------------------------------------------------------------
// scan: Kaynak kodu tara, token listesi üret.
//
// Akış:
//   1. Lexer'a kaynak kodu yükle
//   2. scope() ile tek tek token oku
//   3. "EOL" (End Of Line) token'ı gelene kadar devam et
//   4. Token listesini döndür
//
// "EOL" token'ı: scope() EOF'da üretilen özel bir Token. Parser'a "bitti" sinyali.
// Neden nullptr değil? Çünkü scope() her zaman geçerli bir pointer döndürmeli,
// aksi takdirde null kontrolü gerekir. "EOL" ile bu kontrol token tipine indirgenir.
//
// TODO: std::unique_ptr veya std::vector<std::unique_ptr<Token>> ile bellek yönetimi
// --------------------------------------------------------------------------
inline std::vector<Token*> Tokenizer::scan(std::string input) {
    std::vector<Token*> tokens;
    hmx.setText(input);
    while (true) {
        Token* token = scope();
        if (token->token == "EOL") break;  // Dosya sonu sinyali
        tokens.push_back(token);
        if (hmx.isEnd()) break;            // Güvenlik kontrolü
    }
    return tokens;
}

// --------------------------------------------------------------------------
// scope: Bir sonraki token'ı tanı ve döndür.
//
// Token tanıma sıralaması (önemli!):
//   1. Boşlukları atla
//   2. Yorum satırlarını atla (//, /* */)
//   3. EOF kontrolü → "EOL" token'ı
//   4. String literal ("...")
//   5. Sayısal literal (0-9 ile başlayan)
//   6. Keyword'ler (sınır kontrolü ile)
//   7. Delimiter'lar
//   8. Operatörler
//   9. Identifier (fallback — yukarıdakilerden hiçbiri değilse)
//
// Keyword boundary check:
//   include(kw, false) ile önce eşleşme kontrolü yapılır (konum değişmez).
//   Sonra keyword'ün hemen sonrasındaki karaktere bakılır.
//   Eğer bu karakter harf/rakam/_/$ ise, bu bir keyword değil, daha uzun bir
//   identifier'ın parçasıdır. Örnek: "do" → "double"ın başlangıcı olabilir.
//
//   BUG FIX (commit 438bc0e): Eskiden boundary check yoktu. "double" kelimesi
//   "do" + "uble" olarak iki token'a ayrılıyordu.
// --------------------------------------------------------------------------
inline Token* Tokenizer::scope() {
    hmx.skipWhiteSpace();

    // Yorum satırları: sessizce atla, token üretme
    if (hmx.include("//", true))  { skipOneLineComment(); return scope(); }
    if (hmx.include("/*", true))  { skipMultiLineComment(); return scope(); }

    // EOF kontrolü
    if (hmx.isEnd()) {
        Token* t = new Token();
        t->token = "EOL";  // Özel sinyal token'ı
        return t;
    }

    // String literal: " ile başlar
    if (hmx.getchar() == '"')
        return readString();

    // Sayısal literal: rakam ile başlar (isNumeric: 0-9)
    if (hmx.isNumeric()) {
        INumber lem = hmx.readNumeric();
        NumberToken* nt = new NumberToken();
        nt->base       = lem.base;
        nt->start      = lem.start;
        nt->end        = lem.end;
        nt->hasEpsilon = lem.hasEpsilon;
        nt->isFloat    = lem.isFloat;
        nt->token      = lem.token;
        return nt;
    }

    // Keyword'ler: sınır kontrolü ile tarama
    // include(kw, false) → eşleşme kontrolü yap ama konumu değiştirme
    // getchar(kw.size()) → keyword sonrası karaktere bak
    // Sonraki karakter harf/rakam/_/$ ise → bu bir keyword değil, devam et
    for (const auto& kw : keywords) {
        if (hmx.include(std::string(kw), false)) {
            char next = hmx.getchar(static_cast<int>(kw.size()));
            if ((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z') ||
                (next >= '0' && next <= '9') || next == '_' || next == '$') {
                continue;  // Daha uzun bir identifier'ın parçası
            }
            KeywordToken* kt = new KeywordToken();
            kt->start = hmx.getOffset();
            hmx.toChar(static_cast<int>(kw.size()));
            kt->end   = hmx.getOffset();
            kt->token = kw;
            return kt;
        }
    }

    // Delimiter'lar
    for (const auto& del : delimiters) {
        if (hmx.include(std::string(del), false)) {
            DelimiterToken* dt = new DelimiterToken();
            dt->start = hmx.getOffset();
            hmx.toChar(static_cast<int>(del.size()));
            dt->end   = hmx.getOffset();
            dt->token = del;
            return dt;
        }
    }

    // Operatörler
    for (const auto& op : operators) {
        if (hmx.include(std::string(op), false)) {
            OperatorToken* ot = new OperatorToken();
            ot->start = hmx.getOffset();
            hmx.toChar(static_cast<int>(op.size()));
            ot->end   = hmx.getOffset();
            ot->token = op;
            return ot;
        }
    }

    // Identifier (fallback): hiçbir özel token tipine uymayan her şey
    return readIdentifier();
}

// --------------------------------------------------------------------------
// readIdentifier: Bir tanımlayıcı (identifier) oku.
//
// Tanımlayıcı = harf ile başlayan, harf/rakam/_/$ ile devam eden karakter dizisi.
// NOT: Rakam ile başlayamaz (o zaman sayı olurdu).
//
// Karakter seti:
//   a-z, A-Z: Latin harfleri
//   0-9: Rakamlar (ilk karakter hariç)
//   _ (alt çizgi): Yaygın ayraç
//   $ (dolar): Java/C# uyumluluğu için
//
// TODO: Unicode harf desteği (Türkçe karakterler, Çince, Arapça, vb.)
// --------------------------------------------------------------------------
inline IdentifierToken* Tokenizer::readIdentifier() {
    hmx.beginPosition();
    IdentifierToken* it = new IdentifierToken();
    it->start = hmx.getOffset();

    while (!hmx.isEnd()) {
        char c = hmx.getchar();
        bool read = false;

        // Harf veya rakam kontrolü (ASCII)
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            read = true;
            it->token.push_back(c);
        } else if (c == '_' || c == '$') {
            read = true;
            it->token.push_back(c);
        }

        if (read) {
            hmx.nextChar();
        } else {
            if (it->token.empty()) { hmx.nextChar(); } break;
        }
    }

    it->end  = hmx.getOffset();
    it->size = static_cast<int>(it->context.size());
    hmx.acceptPosition();  // Başarılı okuma → konumu kalıcı yap
    return it;
}

// --------------------------------------------------------------------------
// readString: Bir string literal oku ("...")
//
// Desteklenen escape sequence'ler:
//   \"  → çift tırnak
//   \\  → ters bölü
//   \n  → satırsonu
//   \t  → sekme
//   \r  → satırbaşı
//
// Algoritma:
//   1. Açılış tırnağını (" ) gör → started = true
//   2. Karakterleri oku:
//      - \ ise → sonraki karakteri escape olarak işle, context'e ekle
//      - " ise → started zaten true, bu kapanış tırnağı → ended = true
//      - Diğer → context'e ekle
//   3. Kapanış tırnağında dur
//
// token: Tüm karakterler (tırnaklar ve escape'ler dahil)
// context: Sadece gerçek string içeriği (escape'ler çözülmüş)
//
// Örnek: "a\"b\\n" → token = "\"a\\\"b\\\\n\"", context = "a\"b\n"
//
// TODO: \xNN (hex escape), \uNNNN (Unicode), \UNNNNNNNN (geniş Unicode)
// TODO: Çok satırlı string desteği ("""...""" veya backtick `...`)
// --------------------------------------------------------------------------
inline StringToken* Tokenizer::readString() {
    hmx.beginPosition();
    StringToken* st = new StringToken();
    bool started = false;   // Açılış tırnağı görüldü mü?
    bool ended   = false;   // Kapanış tırnağı görüldü mü?
    st->start = hmx.getOffset();

    while (!hmx.isEnd()) {
        char c = hmx.getchar();
        st->token.push_back(c);
        switch (c) {
            case '"':
                if (!started) {
                    started = true;   // Açılış tırnağı
                } else {
                    ended = true;     // Kapanış tırnağı
                }
                break;
            case '\\':
                // Escape sequence: sonraki karakteri olduğu gibi al
                hmx.nextChar();
                c = hmx.getchar();
                st->token.push_back(c);
                st->context.push_back(c);
                break;
            default:
                st->context.push_back(c);
                break;
        }
        hmx.nextChar();
        if (ended) break;
    }

    st->end  = hmx.getOffset();
    st->size = static_cast<int>(st->context.size());
    hmx.acceptPosition();
    return st;
}

// --------------------------------------------------------------------------
// skipOneLineComment: // ile başlayan yorum satırını satırsonuna kadar atla
// --------------------------------------------------------------------------
inline void Tokenizer::skipOneLineComment() {
    while (!hmx.isEnd()) {
        if (hmx.getchar() == '\n') {
            hmx.nextChar();
            hmx.skipWhiteSpace();  // Satırsonu sonrası boşlukları da temizle
            return;
        }
        hmx.nextChar();
    }
}

// --------------------------------------------------------------------------
// skipMultiLineComment: /* */ bloğunu atla
// NOT: İç içe yorum blokları desteklenmez (C standardı gibi).
// --------------------------------------------------------------------------
inline void Tokenizer::skipMultiLineComment() {
    while (!hmx.isEnd()) {
        if (hmx.include("*/", true)) {
            hmx.skipWhiteSpace();
            return;
        }
        hmx.nextChar();
    }
}

#endif // SAQUT_TOKENIZER
