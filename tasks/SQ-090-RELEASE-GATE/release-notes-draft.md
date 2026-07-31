# saQut 0.9.2 — Release Notu Taslağı (RG-9)

> Bu, #157 SQ-090-RELEASE-GATE'in RG-9 kriteri ("bilinen eksikler ve kapsam
> dışı maddeler release notunda dürüstçe listelenir") için hazırlanan
> taslaktır. Ürün sahibi onayı olmadan resmi release notu sayılmaz.
> Sürüm: 0.9.2 iş dalı — resmi GitHub release sayfası 0.9.5 için açılacak.

## Bu sürümde

- **VM normatif çalıştırma yolu.** `run/check/ast/ir/symbols` temel CLI
  yüzeyi, syntax/semantic/runtime hata sınıflarını merkezi bir sözleşmeyle
  (`exit 0/64/65/70`) ayırt eder (RG-7).
- **VM dogfood kanıtı.** Elle yazılmış, gerçek JSON parser, XML parser ve
  CPU-ağırlıklı (Eratosthenes kalburu + rekürsif Fibonacci + sort) programlar
  `tests/golden/dogfood/` altında tracked (RG-4).
- **Basit VM GC** varsayılan açık; çoklu collection ve canlı-kök korunumu
  `tests/golden/gc/liveness.sqt` ile ölçülmüş (RG-5).
- **Dört bilinen hata iddiası** (#134, #135, #136, #114) güncel binary ile
  yeniden doğrulandı: üçü (#134 exec parser hatası yutma, #135 statik
  string-indeksleme denetimi, #136 switch return-completeness) gerçek bug'lardı
  ve düzeltildi; #114 mimari borç olarak dispose edildi (aktif hata değil).
- **#170 düzeltildi.** Parser'daki `(`/`[`/`{` kapanış delimiter'ı bekleyen
  site'lar artık eksik delimiter'da `E905` diagnostic'i basıyor — eskiden
  sessizce farklı bir programa yeniden yorumlanıp exit 0 ile "başarıyla"
  çalışıyordu (ADR-038 ihlali). Aynı turda, `import { ... }` listesinde
  beklenmeyen bir token'da parser'ı sonsuz döngüye sokan bağımsız bir
  hang/DoS hatası da bulunup düzeltildi (PR #173). 0.9.2'de struct ve enum
  gövde kapanışı da aynı sınıfa eklendi (eksik `}` artık E905 + exit 65).
- **#165 düzeltildi.** MIR JIT, `switch` bir fonksiyonun SON ifadesi
  olduğunda segfault veriyordu; label tablosu ve fonksiyon-sonu etiketi
  düzeltildi, `.jit_known_broken` marker kaldırıldı.
- **SQ-100 JSONL ailesi (ürün sahibi kararıyla bu dala alındı):**
  - **#144 — `saqut check`** stdout'u canonical JSONL: `check.header`
    (schemaVersion) → `check.diagnostic`* (file + exact line/column/offset) →
    `check.end` (errors/warnings sayaçları).
  - **#145 — `saqut symbols --jsonl`**: insan-okur metin varsayılan kalır;
    `--jsonl` makine yüzeyi (header → symbol/diagnostic* → end). Eski `--json`
    preview kaldırıldı.
  - **#146 — `saqut tokens`**: her token file + line + column (1-tabanlı
    UTF-8 byte-tabanlı) + byteOffset (0-tabanlı) + byteLength taşır; Unicode
    çok baytlı lexemelerde byte ve görüntü kolonu karıştırılmaz.
- **#132 — Opcode spec tablosu:** `OPCODE_LIST(X)` X-macro tek kaynak —
  enum, `opcodeName()`, arite ve backend bayrakları tablodan türetilir;
  yeni opcode eklemek tek satır.
- **#130 — Tip temsili sözleşme tablosu:** `src/core/value_rep_contract.hpp`
  ValueKind → VM inline / JIT register / DAP temsilini tek tabloda gösterir;
  backend patlaması borcunu görünür kılar (karar değil).
- **#218 — IR CFG altyapısı + `saqut ir --cfg`:** flat talimat listesi →
  BasicBlock + kenar (fall-through dahil) → linearize round-trip birebir.
  `--cfg` görüntüleyici TTY'de renkli, redirect'te ANSI'sız; literal
  operandlar flat dump ile aynı ortak renderer'dan gelir.
- **#76 — Runtime capability backstop canlandı:** `drop("fs")` sonrası
  `readFile` artık `E_CAP_MISSING` ("requires --allow-fs") veriyor (ADR-035
  B modeli; #218 metadata taşımasında map population'ı eksik kalmıştı).
- **#141 — IR TTY renk temizliği:** `saqut ir` yalnız gerçek TTY'de renkli;
  redirect/pipe'ta sıfır ANSI (ir_function.cpp dahil tüm dump katmanı).
- **Test kapısı:** 229/229 geçiyor (Debug + Release).

## Bilinen eksikler (dürüst liste)

### Orta — bilinen, izole, ayrı issue'larda

- **#160**: `saqut ast --output <dosya>` JSON olmayan (varsayılan) modda
  dosyaya hiçbir şey yazmıyor (`ASTNode::log()` `std::cout`'a hardcode).
  `--json --output` ile çalışıyor.
- **#163**: `saqut exec` argümansız çağrıldığında `args.hpp`'nin global
  "source.sqt" fallback'i yüzünden anlamsız bir semantic hata veriyor,
  kendi usage mesajını hiç göstermiyor.
- **#168**: saQut fonksiyonları array tipini DÖNÜŞ TİPİ olarak alamıyor
  (`int[] f() {...}` parse hatası veriyor) — parametre/değişken tipi olarak
  sorunsuz.
- **CFG/IR görünümünde longint/float32 opcode operand'ları boş** (LOAD_LONG,
  LADD, F32ADD, CAST_*_LONG...): flat dump golden spec'i bunu sabitliyor
  (`tests/golden/ir/*.ir_plain.expected`); kapatmak golden güncellemesi
  gerektirir (ayrı iş).

### Mimari borç — 1.0.0/backlog'a önerilen

- **#114**: Sayısal biçimlendirme (float/double/float32 → string) VM ve
  JIT'te iki ayrı yerde elle senkron tutuluyor; VM'in sıcak yol
  karşılaştırmaları switch-exhaustiveness kalkanı olmayan if/else-if
  zinciri kullanıyor; constant folding pass'i tip genişliğini kendi başına
  varsayıyor. Şu an yanlış sonuç üretmiyor, yalnız yapısal risk.

### LSP/DAP preview matrisi — RG-8

0.9.x için yeni LSP/DAP capability eklenmedi ve tam 1.0 protokol sözleşmesi
ilan edilmedi. Mevcut tracked preview alt kümesi ayrı matrise bağlandı:
`docs/v0.9-lsp-dap-preview-matrix.md`.

- LSP: 21 tracked senaryo.
- DAP: 10 tracked senaryo.
- Matrix, desteklenen/testlenen preview satırlarını, explicit unsupported
  yüzeyleri ve advertised-unverified boşlukları ayrı listeler.

Bilinen sınır korunur: Bu release note taslağı editor smoke, tam LSP/DAP v1
contractı veya yeni protocol capability vaadi değildir.

## Kapsam dışı (AGENTS.md §9, hatırlatma)

Stabil JIT, AOT, public concurrency, sandbox, WASM, record/replay,
production Redis/SQLite uyumluluğu, generic native library loading,
kapsamlı optimizer, 500+ self-hosted stdlib migrasyonu — bunların hiçbiri
0.9.x veya 1.0.0'a gizlice eklenemez.

## MIR JIT durumu

Her yerde `[EXPERIMENTAL]`. #165 (switch-son-ifade segfault) düzeltildi;
kalan JIT sınırları (array/struct/global/try-catch opcode'larında programın
tamamı reddedilir — kısmi JIT yok) bu release'in başarı kriteri değildir —
yalnız VM normatif backend'dir.
