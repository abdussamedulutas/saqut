# Sonnet Uygulama Promptu — Bileşik Tipler Runtime'ı (ADR-020…024)

> Bu dosya bir **devir teslim promptu**. Opus ile yapılan mimari oturumda 5 karar
> alındı (ADR-020…024). Bu plan o kararları **koda döker.** Kararları yeniden
> sorma — uygula. İletişim Türkçe; commit sonu `Co-Authored-By: Claude Opus 4.8
> <noreply@anthropic.com>`; dal `0.1.0`.

---

## 0. Nerede kaldık (bağlam)

Bu oturumda kilitlenen mimari kararlar (tümü `docs/adr-frontend-analiz.md`):

| ADR | Karar (özet) |
|---|---|
| **020** | Primitive (`int`/`float`/`bool`) = **değer**; bileşik (`struct`/`array`/`string`) = **referans** (JS/Java/C# modeli). "Pointer yok" = kullanıcıya `&`/`*` *sözdizimi* yok; runtime referansı kullanır. |
| **021** | Null güvenliği: varsayılan **non-null**; nullable açıkça `Type?`. Akış-duyarlı null analizi (compile-time, runtime sıfır). `a!` = runtime-kontrollü iddia. |
| **022** | GC: basit **taşımasız, stop-the-world, deterministik mark-sweep**. Nesne modeli **baştan GC-hazır** (header + kök sayımı + çocuk sayımı). v1: toplama YOK. **`shared_ptr`'ı kalıcı sahiplik modeli YAPMA.** |
| **023** | `==`: primitive değer; referans (struct/array) **kimlik** (aynı nesne); string **içerik**. Derin eşitlik asla `==`'e bağlanmaz → ayrı `deepEquals()`. |
| **024** | String = **immutable değer-tipi, iç temsil UTF-8**. `s+"x"` yeni string üretir; `==` içerik. |

Açık mimari borç: **#56** (döngüsel referans → mark-sweep GC v2). Şimdilik toplama yok, bilinçli.

**Mevcut kod durumu:**
- int-only pipeline uçtan uca çalışıyor (`examples/fibonacci.sqt`).
- `src/vm/value.hpp` → `Value` = `{ ValueKind kind; int intValue; std::string stringValue; }`. String **inline** (immutable olduğu için yeterli — taşıma zorunlu değil).
- `src/ir/` → 3-adresli IR, slot tabanlı. `instruction.hpp`, `ir_generator.cpp`, `ir_program.hpp`.
- `src/vm/interpreter.cpp` → bytecode yorumlayıcı döngü. `call_frame.hpp` çağrı çerçevesi.
- struct/array: parser + semantik **var**, IR/VM codegen **YOK**.

---

## 1. İlk görev — GC-hazır nesne modeli + ARRAY runtime (dikey dilim)

**Neden array, string değil:** string zaten immutable-değer olarak inline çalışıyor;
asıl kararları (referans semantiği + GC-hazır nesne modeli + kimlik eşitliği)
**doğrulayan** ilk gerçek referans-tipi array'dir. Bu görev, struct'ın da oturacağı
temeli atar.

### KAPSAM DIŞI (bu görevde YAPMA — sonraki görevler)
- ❌ Gerçek çöp toplama (mark-sweep). ADR-022 v1 = toplama yok. Sadece header +
  all-objects listesi + kök-sayımı *kancasını* kur.
- ❌ Tam null akış-analizi (ADR-021). `int[]?` parse edilebilir ama flow-narrowing
  bu görevde değil.
- ❌ struct codegen, string UTF-8 cilası, builder. Ayrı görevler (Bölüm 2).
- ❌ `shared_ptr` ile nesne sahipliği. Düz `Object*` + intrusive liste.

### Adım 1.1 — Nesne modeli temeli (`src/vm/object.hpp` veya benzeri)
```cpp
enum class ObjectType { Array /*, Struct, String (ileride) */ };

struct Object {
    ObjectType type;
    bool       marked = false;   // mark-sweep için (v2); şimdilik kullanılmaz
    Object*    next   = nullptr;  // intrusive "tüm nesneler" listesi (mark-sweep tarayışı için)
};
```
- Bir **Heap/allocator**: `Object* allocate(...)` her yeni nesneyi `next` zincirine
  ekler. **Free yok** (v1). Yıkıcıda/process exit'te toptan bırak.
- **Kök sayımı kancası:** VM'in operand stack + frame local'leri + global slot'lar
  üstünden referansları gezebileceği bir yol bırak (şimdilik imza/TODO yeterli; içini
  doldurmak v2). `// TODO(#56): mark-sweep kök taraması buradan` ile işaretle.

### Adım 1.2 — `Value`'yu referans taşıyacak şekilde genişlet (`src/vm/value.hpp`)
- `ValueKind`'a `Ref` ekle (array/struct nesnelerine `Object*`).
- `Value`'ya `Object* ref = nullptr;` alanı + `static Value fromRef(Object*)`.
- `Nil` kind ekle (ADR-021 null: `Type?` null değeri ve referans varsayılanı).
- `isTruthy`/`toString`/`typeName`'i yeni kind'lar için genişlet.
- String inline kalsın (`ValueKind::String`), dokunma.

### Adım 1.3 — ArrayObject
```cpp
struct ArrayObject : Object {           // type = ObjectType::Array
    std::vector<Value> elements;        // homojen (int[] → hepsi Int)
};
```
- Array literali (`[1,2,3]` — **parser'ın mevcut sözdizimini doğrula**, Bölüm 4'e bak)
  → ArrayObject tahsis, `elements` doldur, `Value::fromRef` ile slot'a koy.

### Adım 1.4 — IR opcode'ları + IR üreteci
- `ARRAY_NEW` (literal/boyuttan array oluştur), `ARRAY_GET dest, arr, idx`,
  `ARRAY_SET arr, idx, val`, `ARRAY_LEN dest, arr`. (`instruction.hpp` + üreteç.)
- **Referans semantiği:** array bir fonksiyona geçince/atanınca `Value` (içindeki
  `Object*`) **kopyalanır ama nesne paylaşılır** → `func(arr)` içindeki `ARRAY_SET`
  çağıranı etkiler. (Bu, kararın doğrulandığı kritik testtir.)
- **`==` (ADR-023):** array'de **kimlik** → iki `Value`'nun `ref`'i aynı `Object*` mı?
  (İçerik DEĞİL.) Mevcut EQ opcode'unu tip-bazlı dallandır veya `REF_EQ` ekle.

### Adım 1.5 — Sınır kontrolü (kafes felsefesi)
- `ARRAY_GET`/`ARRAY_SET` index sınır dışıysa → **temiz runtime hatası** (sessiz UB
  değil). saQut "cage" — koruma şart.

### Doğrulama (golden test — `tests/` veya `examples/`)
```c
func degistir(int[] a) {
    a[0] = 99;
}
func main() {
    int[] x = [1, 2, 3];
    degistir(x);
    print(x[0]);          // 99  ← referans semantiği (ADR-020)
    int[] y = x;
    print(y == x);        // true ← kimlik (aynı nesne, ADR-023)
    int[] z = [1, 2, 3];
    print(z == x);        // false ← içerikçe aynı ama farklı nesne
    print(x[1]);          // 2
    // x[5] → runtime sınır hatası
}
```
`saqut run` ile beklenen çıktı doğrulanmalı. `saqut ir` çıktısı yeni opcode'ları göstermeli.

---

## 2. Sonraki görevler (sıralı — ilk görev bitince)

1. **Struct runtime.** Alanlar (isimli), `StructObject : Object { std::vector<Value> fields; }`.
   `struct Node { Node next; }` artık **meşru** (referans → sonlu boyut). **E010
   revizyonu:** referansla tutulan struct alanı için döngü artık hata DEĞİL (ADR-020).
   Bu görev #56'yı *canlı* hale getirir (döngü kurulabilir, henüz toplanmaz).
2. **String cilası (ADR-024).** İçerik `==`'i doğrula; UTF-8 çok-baytlı pass-through
   (`"şğü"` concat+print bozulmasın); bayt-uzunluğu vs karakter ayrımını açık API
   olarak işaretle. (İstege bağlı: string'i de Object modeline taşıyıp intern et —
   zorunlu değil.)
3. **Null akış-analizi (ADR-021 — REVİZE).** Ayrı **frontend** görevi: `Type?` tip
   sistemi + `T <: T?` atama kuralı + katı operand kuralı + akış-duyarlı narrowing
   (nested `if` + sıralı guard + `&&`). ⚠️ **`a!`/`??`/`?.` YASAK** — null yalnızca
   görünür `if` ile aklanır. `T?` üstünde doğrudan erişim → derleme hatası. Frontend
   kesin çözer (backend yeniden analiz etmez). Detay: TODO Bölüm "SIRADAKİ İŞ" + ADR-021.
4. **Hata yönetimi (ADR-025, #57).** Struct-tabanlı yakalanabilir hata (Swift-tarzı):
   - **Önkoşul:** IR'a **satır tablosu** (komut index → kaynak konum) — stacktrace için. Şu an taşıyor mu doğrula.
   - Standart built-in `struct Error { int line; int char; string message; string trace; string code; }`.
   - Klasik **`try { ... } catch (e) { ... }` bloğu** (unwind + en yakın handler'a zıpla); `catch (e)` → `e : Error`. `throw` ile kullanıcı da kaldırır.
   - Runtime null-deref (NPE analoğu), array OOB, /0, `a!` patlaması → **yakalanabilir hata**; `message`/`code` = derleyicinin W/E kataloğu.
   - **UNCHECKED (Java/C#/JS usulü):** fonksiyon **işaretlenmez** (`noexcept`/`constexpr` tarzı YOK), çağrıda **`try f()` YOK**. İmza-işaretli Swift/Zig modeli KULLANMA.
   - Stacktrace = frame stack'ten en içten dışa; insan + JSON; deterministik (adım indeksi/replay handle).
5. **float/double (#44).** Bağımsız; `Value::Float` + FADD… opcode + tip denetleyici.
6. **mark-sweep GC v2 (#56).** Adım 1.1'de bırakılan header+kök kancası üstünde aç.

---

## 3. Uyman gereken kararlar (sorma, uygula)
- Referans semantiği: bileşik = paylaşılan nesne; primitive = kopya (ADR-020).
- Array/struct `==` = **kimlik**; string `==` = **içerik** (ADR-023).
- Nesne modeli GC-hazır ama v1 toplama YOK (ADR-022); `shared_ptr` sahiplik modeli kurma.
- Sınır kontrolü + temiz runtime hataları (kafes felsefesi).
- Erken soyutlama yok: önce çalışan dikey dilim, framework sonra (CLAUDE.md ilkesi).

## 4. Bloklamayan açık noktalar (ilk göreve engel değil — gerekirse Opus'a sor)
- **Array literal sözdizimi:** parser'da `[1,2,3]` mi `{0,1,2}` mi kabul ediliyor?
  Mevcut parser'ı **doğrula**; tutarsızsa `[...]` tercih (tip `int[]`). Karar gerekirse işaretle.
- Array sabit-boyut mu büyüyebilir mi: readme "dinamik" der; ilk görevde literal+indeks
  yeter, büyütme API'si (`push`/`len`) sonra.
- `int[]` non-null varsayılan (ADR-021) → `int[] a;` init zorunlu; nullable `int[]?`.
- #42 (cast modeli) ve #41 (stdlib politikası) henüz karara bağlanmadı — ilk görevi
  bloklamaz; sıraları gelince Opus ile.

---

## 5. Özet — tek cümle
GC-hazır basit nesne modelini kur (header + all-objects listesi, toplama yok), `Value`'ya
referans (`Ref`) + `Null` ekle, **array**'i referans semantiği + kimlik `==` + sınır
kontrolüyle uçtan uca çalıştır; struct ve null-analizi sonraki görevler.

---

## 6. ⚠️ TERMİNOLOJİ KİLİDİ — isimleri değiştirme/icat etme

Bu oturumda `null` yerine `nil` yazıldı; bu tür sapmalar **olmamalı.** Anlaşılan
isimler **aynen** kullanılır. İsmi belirsiz bir şeyle karşılaşırsan **icat etme —
Opus'a sor.**

| Kavram | DOĞRU | YANLIŞ (kullanma) |
|---|---|---|
| Null anahtar sözcüğü / literal | **`null`** | ~~`nil`~~, ~~`none`~~, ~~`void`~~ |
| Nullable tip işareti | **`Type?`** (ör. `int?`, `Node?`) | ~~`Optional<T>`~~, ~~`Type \| null`~~ |
| Non-null iddiası / elvis / safe-call | **YASAK** — `if` narrowing kullan | ~~`a!`~~, ~~`a ?? x`~~, ~~`a?.f`~~ (ADR-021 revize) |
| Array literal | **`[1, 2, 3]`** | ~~`{1,2,3}`~~ |
| Array tip | **`int[]`** | ~~`array<int>`~~, ~~`[]int`~~ |
| Hata tipi | **`Error`** (`{line,char,message,trace,code}`) | ~~`Exception`~~, ~~`Err`~~ |
| Hata kaldırma / yakalama | **`throw` / `try` / `catch`** | ~~`raise`~~, ~~`rescue`~~ |
| Fonksiyon | **`func`** | ~~`fn`~~, ~~`function`~~, ~~`def`~~ |
| C++ ValueKind null'u | **`ValueKind::Null`** | ~~`Nil`~~ |

**Genel kural:** ADR'lerde/handoff'ta yazan tam ismi kullan. Yeni bir isim gerekiyorsa
ve ADR'de yoksa, **kendin karar verme** — TODO'ya not düş veya Opus'a sor. Kod
identifier'ları İngilizce (dil sözdizimi), yorum/commit Türkçe.
