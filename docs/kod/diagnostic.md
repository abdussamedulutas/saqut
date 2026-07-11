# Tanılama (`src/diagnostic/`)

## Sorumluluk

Derleme boyunca bulunan hata/uyarıları yapısal veri olarak temsil etme ve
biriktirme. "Veri birincil, metin bir görünümdür" ilkesiyle: aynı tanı hem
terminal (insan-okur) hem LSP/JSON (makine-okur) olarak dışa verilir.
Diagnostic'ler ilk hatada durmaz (ADR-013) — tüm tanılar toplanır, pipeline
sonunda topluca raporlanır; durdurma kararını `hasErrors()` ile pipeline verir.

## Dosya envanteri

| Dosya | Rol |
|-------|-----|
| `diagnostic.hpp` | `Diagnostic` yapısı, `DiagLevel` enum'ı, hata kataloğu (E001–E011, W001–W004, E901–E904), `makeDiagnostic`, `suggestName`/`diagEditDistance` (yazım hatası önerisi). |
| `diagnostic_engine.hpp` | `DiagnosticEngine` sınıfı — biriktirme (`report`), sorgu (`hasErrors`, `count`), insan-okur (`printAll`) ve makine-okur (`toJson`, `toLspDiagnostics`) çıktı. |

## Ana tipler ve ilişkileri

```
DiagLevel : Error | Warning | Note | Hint

Diagnostic
  ├─ level       : DiagLevel
  ├─ code        : string  ("E003", "W001" vb.)
  ├─ loc         : SourceLocation  (nerede?)
  ├─ message     : string  (bağlama özel açıklama)
  ├─ hint        : string  (opsiyonel çözüm önerisi)
  └─ tokenLength : int     (LSP range genişliği)

DiagnosticEngine
  ├─ diagnostics_ : vector<Diagnostic>  — ekleme sırasıyla biriktirir
  ├─ report()     : 3 overload — Diagnostic doğrudan / kod+loc+mesaj / seviye+kod+loc+mesaj
  ├─ hasErrors()  → bool
  ├─ printAll(os) : insan-okur terminal çıktısı (satır: seviye [kod]: mesaj)
  └─ toLspDiagnostics() : LPS publishDiagnostics formatında JSON

diagnosticCatalog()
  └─ vector<DiagInfo>  — kod → (seviye, kanonik başlık) sabit listesi
```

## Veri akışı

```
Lexer/Parser/Semantic/Optimizer
       ↓  (hata bulunca)
  diag.report(Diagnostic{...})
       ↓
  DiagnosticEngine::diagnostics_ (birikir)
       ↓  (faz sonunda)
  pipeline → hasErrors()? → varsa printAll(cerr) veya toLspDiagnostics()
```

## Diğer modüllerle temas

| Modül | İlişki |
|-------|--------|
| Tüm analiz katmanları | `DiagnosticEngine::report()` ile hata/uyarı ekler. |
| LSP | `toLspDiagnostics()` ile `publishDiagnostics` bildirimi üretilir. |
| CLI (`saqut check`) | `hasErrors()` + `printAll()` ile terminal çıktısı. |
| Parser | E901–E904 sözdizimi hatalarını `DiagnosticEngine`'e bağlı olarak üretir (opsiyonel `DiagnosticEngine*` parametresi). |

## Tasarım kararları

- **İlk hatada durma YOK** (ADR-013): Tüm tanılar toplanır, pipeline durdurma
  kararını verir. Kullanıcı tek seferde tüm hataları görür.
- **Hata kataloğu sabitlenmiştir** `diagnostic.hpp`'de: her kodun kanonik anlamı
  tektir; bağlama özel mesaj `report` sırasında verilir. İleride `saqut explain E003`
  katalog başlığını kullanabilir.
- **Üç report overload'u**: doğrudan `Diagnostic` nesnesi, kod+konum+mesaj (katalogdan
  seviye çözülür), seviye+kod+konum+mesaj (açık seviye). Esneklik için.
- **`Diagnostic` JSON serileştirmesi**: `toJsonObj()` / `toJson()` — LSP, AI araçları
  ve CLI için ortak format.
- **`tokenLength` alanı**: LSP `range.end.character` hesaplamasında hatanın altını
  çizmek için kullanılır.
- **Yazım-hatası önerisi**: `suggestName()` Levenshtein mesafesi (en fazla 32 karakter,
  eşik 3) ile en yakın sembol adını önerir.
- **LSP uyumu**: `toLspDiagnostics()` 1-severity error, 2-warning; hint mesaja
  eklenir (LSP ayrı bir "relatedInformation" alanı taşımaz).

## Bilinen sınırlar / TODO

- `diagnosticCatalog()` içindeki `DiagInfo` listesi elle güncellenir; yeni hata
  kodu eklenirken katalog da güncellenmelidir (zorunlu değil ama tavsiye edilir).
- LSP hint gösterimi sınırlıdır — hint mesaja `\n` ile eklenir; LSP'nin ayrı
  `relatedInformation` alanı kullanılmaz.
- `diagEditDistance` yalnızca ≤32 karakter girdilerde çalışır; daha uzun
  string'lerde 99 döner (kırpma yapılmaz).
