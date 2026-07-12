# Sözcüksel Analiz (`src/tokenizer/` + `src/lexer/`)

## Sorumluluk

Kaynak kodu karakter karakter okuyarak Parser'ın tüketeceği token dizisine dönüştürür.
İki alt katmandan oluşur: **Lexer** (karakter seviyesi — konum takibi, backtracking,
sayı okuma) ve **Tokenizer** (sözcük seviyesi — karakterleri token'lara ayırma,
keyword/identifier ayrımı, yorum atlama). Pipeline'da Katman 1 (Lexer) ve Katman 2
(Tokenizer) olarak yer alır.

## Dosya envanteri

| Dosya | Rol |
|-------|-----|
| `lexer/lexer.hpp` | Lexer sınıfı bildirimi + INumber yapısı. Karakter tarama, backtracking, sayı okuma API'si. |
| `lexer/lexer.cpp` | Lexer metodlarının gerçeklemesi (konum yönetimi, karakter okuma, `readNumeric`). |
| `tokenizer/token.hpp` | Token sınıf hiyerarşisi: `Token` → 6 türemiş sınıf (String, Number, Operator, Delimiter, Keyword, Identifier). |
| `tokenizer/tokenizer.hpp` | Tokenizer sınıfı + operatör/delimiter/keyword sabit tabloları (`constexpr string_view[]`). |
| `tokenizer/tokenizer.cpp` | Tokenizer metodları (`scan`, `scope`, `readIdentifier`, `readString`, yorum atlama) + KW_MAP (O(1) keyword lookup). |

## Ana tipler ve ilişkileri

```
Lexer
  ├─ input       : string    — kaynak kodun tamamı
  ├─ offset      : int       — mevcut okuma konumu
  ├─ offsetMap   : vector<int> — backtracking yığını
  └─ sourceFile  : SourceFile — offset → (line, column) dönüşümü
       │
       ├─ beginPosition() / acceptPosition() / rejectPosition()
       ├─ getchar() / nextChar() / toChar()
       ├─ include(word) — desen eşleme
       ├─ readNumeric() → INumber {start, end, token, isFloat, hasEpsilon, base, positive}
       └─ skipWhiteSpace()

Tokenizer
  ├─ hmx : Lexer   — composition (Lexer'ı kapsüller)
  ├─ scan(input, filePath) → vector<Token*>
  ├─ scope() → Token*  — ana dispatch (her çağrıda bir token)
  ├─ readIdentifier() → IdentifierToken*
  └─ readString() → StringToken*

Token (abstract base)
  ├─ StringToken     — "...", context'te tırnaksız içerik
  ├─ NumberToken     — 42, 0xFF, 3.14; isFloat, hasEpsilon, base
  ├─ OperatorToken   — +, ==, +=, <<= (token metni operatörü tanımlar)
  ├─ DelimiterToken  — (, ), {, }, ;, ->, ::
  ├─ KeywordToken    — if, else, int, struct, return (50+ keyword)
  └─ IdentifierToken — değişken/fonksiyon adları

Tablolar (tokenizer.hpp):
  ├─ operators[]   — constexpr string_view — çok karakterliler (==, <<=) önce
  ├─ delimiters[]  — constexpr string_view — çok karakterliler (::, ->) önce
  └─ keywords[]    — constexpr string_view — ~50 adet (kontrol + tip + OOP + diğer)
```

## Veri akışı

```
Kaynak kod (string)
       ↓
  Lexer::setSourceText(path, text)
       ↓  karakter karakter
  Tokenizer::scope()
       │   ├─ skipWhiteSpace()
       │   ├─ include("//") → skipOneLineComment()
       │   ├─ include("/*") → skipMultiLineComment()
       │   ├─ getchar() '"' → readString()
       │   ├─ isNumeric()   → readNumeric() → NumberToken
       │   ├─ switch-case   → OperatorToken / DelimiterToken
       │   └─ readIdentifier() → KW_MAP lookup → KeywordToken veya IdentifierToken
       ↓
  vector<Token*> — heap'te new'lenmiş, polimorfik token listesi
       ↓
  Parser tüketir, token'ları delete eder
```

## Diğer modüllerle temas

| Modül | İlişki |
|-------|--------|
| Parser | `vector<Token*>` tüketir; token tiplerine göre AST düğümleri kurar. |
| Core (location, sourcefile) | Lexer, `SourceFile::offsetToLocation()` ile konum bilgisi alır. |
| CLI (`saqut tokens`) | Doğrudan `Tokenizer::scan()` çağırır, token listesini gösterir. |

## Tasarım kararları

- **Lexer + Tokenizer ayrımı**: Lexer yalnızca karakter okuma/konumlandırma/backtracking
  yapar; Tokenizer anlamlı token'ları üretir. Bu ayrım, karmaşık sayı okuma ve desen
  eşlemenin tek bir karmaşık sınıfta birikmesini önler.
- **Backtracking yığını** (`offsetMap`): İç içe `beginPosition()` çağrılarına izin verir
  (örn. include() iç içe). `positionRange()` heap tahsisi yapar — TODO: `std::pair<int,int>`
  kullanılmalı.
- **Operatör/delimiter sıralaması**: Çok karakterliler (`==`, `<<=`, `::`) önce listelenir;
  `scope()` içinde önce bunlar kontrol edilir (açgözlü eşleme).
- **Keyword dönüşümü**: `scope()` önce `readIdentifier()` ile IdentifierToken okur,
  sonra KW_MAP'te arar. Eşleşme varsa KeywordToken'a çevirir, IdentifierToken'ı siler.
  Bu sayede keyword listesi değişirse yalnızca tablo güncellenir.
- **Heap'te new'lenen token'lar**: Polimorfik kullanım için tüm token'lar `new` ile
  tahsis edilir; çağıran (Parser/CLI) `delete` ile temizler.
- **INumber (ara yapı)**: Lexer ile Tokenizer arasında sayı verisi taşır; değer tipidir
  (heap tahsisi yok). Tokenizer bu yapıyı NumberToken'a dönüştürür.
- **Header'daki constexpr tablolar + .cpp'deki unordered_map**: keywords[] derleme
  zamanı sabitidir; KW_MAP ise çalışma zamanında O(1) arama sağlar. İkisi de aynı
  veriyi tutar, küçük farklar olabilir (ör: tokenizer.cpp'de "export" var).

## Bilinen sınırlar / TODO

- `positionRange()` `new int[2]` ile heap tahsisi yapar; `std::pair<int,int>` veya
  küçük struct kullanılmalı (`positionRange.cpp`'de TODO notu var).
- KW_MAP ile header'daki keywords[] arasında tutarsızlık olabilir (tokenizer.cpp'de
  KW_MAP'te "export" var ama header'daki keywords[]'de yok).
- Token'lar heap'te new'lenir ve elle yönetilir; `unique_ptr` veya `shared_ptr`
  kullanılmaz — belleği sızdırmamak için çağıranın dikkatli olması gerekir.
- Lexer::offsetMap tipi `std::vector<int>`; backtracking işlemi O(1) amortized
  ama küçük token'lar için sabit maliyet tahsisi vardır.
