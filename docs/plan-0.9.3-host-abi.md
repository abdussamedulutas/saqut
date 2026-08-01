# 0.9.3 Planı — Merkezi Host Çağrı ABI'si

**Durum:** Taslak, ürün sahibi onayı bekliyor.
**Bağlam:** `OPCODE-IR.md` §4 ve §3.

---

## 1. Sorun

`CALLHOST` bugün tek opcode altında **üç ayrı çağrı sözleşmesi** taşıyor ve
bunları `functionName` string karşılaştırmasıyla ayırıyor
(`src/vm/interpreter.cpp:1323`):

| `functionName` | Dispatch | İmza | JIT |
|---|---|---|---|
| `"__ffi__"` | `callHostFn(id, args, ctx)` | `HostContext&` (caps + args + heap) | hayır |
| `"__builtin_method__"` | `dispatchBuiltinMethod(id, args, heap)` | `Heap&` | hayır |
| `"print"` | `executeHostFunction(name, …)` | hiçbiri | yalnız `print/1` |

JIT tarafında karşılığı tek satır (`mir_backend.cpp:343`):

```cpp
bool isSupportedCallhost(const Instruction& instr) {
    return instr.functionName == "print" && instr.argSlots.size() == 1;
}
```

JIT'in yalnız `print`'i bilmesi teknik bir sınır değil — her aile için ayrı
trampoline yazmak gerekmesi. Bugünkü yapıda 44 host fonksiyonu + 26 built-in
metodu JIT'e açmak, **70 elle yazılmış trampoline** demek. Bu istenmeyen iş.

### Ölçüm: performans darboğazı değil

300.000 iterasyonda `abs()` (FFI) ile eşdeğer saQut fonksiyonu, aynı makinede
dönüşümlü üç ölçüm:

```
ffi  0,874s  |  pure 1,340s
ffi  0,886s  |  pure 1,359s
ffi  0,896s  |  pure 1,355s
```

FFI yolu saf saQut çağrısından **1,5x hızlı** — çünkü frame açmıyor. Yani
`std::vector<Value>` inşası (Value = 80 bayt) ölçülebilir bir darboğaz
oluşturmuyor. **Bu iş performans işi değil, kapsam ve tekrar-yazım işidir.**

Buna rağmen tasarım, hızı bozmamak zorunda: yeni ABI'nin VM tarafında bugünkü
maliyetin üstüne çıkmaması ölçülerek doğrulanır.

---

## 2. Hedef

Host fonksiyonlarını ve built-in metotları **tek bir ABI** altında tanımlamak;
VM ve her backend bu ABI'yi tek bir köprüden tüketsin. Yeni bir host fonksiyonu
eklemek, hiçbir backend'e dokunmayı gerektirmesin.

Gelecekteki backend'ler (LLVM, gccjit) için de tek bariyer: backend, ABI'nin
**tek** C çağrı konvansiyonunu bilir; 70 fonksiyonu ayrı ayrı bilmez.

---

## 3. Tasarım

### 3.1 Tek çağrı konvansiyonu

Bütün host çağrıları tek bir C imzasına indirgenir:

```c
// Her host fonksiyonu ve her built-in metod bu imzayı taşır.
// Dönüş: 0 = başarılı, sıfır-dışı = hata (err doldurulmuş).
typedef int (*HostThunk)(HostCallFrame* f);
```

```cpp
struct HostCallFrame {
    // Argümanlar ve dönüş — çağıran tarafından sahiplenilen, ÖNCEDEN AYRILMIŞ
    // bölge. Tahsis yok: VM frame slot'larına, JIT ise stack'teki sabit bir
    // tampona işaret eder.
    HostSlot*   args;
    int         argc;
    HostSlot    ret;

    // VM durumuna erişim — yalnızca ihtiyaç duyan thunk'lar okur.
    HostEnv*    env;      // caps, programArgs, heap, outputSink

    // Hata kanalı — throw yerine. Backend'ler C++ exception'ı geçemez.
    HostError   err;
};
```

`HostSlot`, `Value`'nun **backend-nötr** biçimidir:

```cpp
struct HostSlot {          // 16 bayt hedef
    HostKind kind;         // Int/LongInt/Float/Float32/Str/Decimal/Ref/Null/Date
    union {
        int64_t   i;       // Int, LongInt, Date, bool
        double    d;       // Float, Float32
        void*     p;       // Str (StringObject*), Ref (Object*), Decimal (DecimalValue*)
    };
};
```

Kritik nokta: `HostSlot` **string'i değer olarak taşımaz**, pointer taşır.
Bugünkü `Value` 80 bayt çünkü içinde `std::string` (32) + `DecimalValue` (16)
gömülü. `HostSlot` 16 bayt olur ve trivially copyable'dır — JIT'in stack'te
doğrudan inşa edebileceği tek şey budur.

### 3.2 Hata kanalı: exception yok

Bugün hem `callHostFn` hem `dispatchBuiltinMethod` `std::runtime_error`
fırlatıyor, VM `catch` edip `makeErrorValue` ile saQut Error'a çeviriyor. JIT
tarafından C++ exception geçmek taşınabilir değil ve her backend'e ayrı unwind
bariyeri demek.

Yerine: thunk sıfır-dışı döner, `f->err` doldurur. Çağıran (VM veya JIT
trampolini) bunu saQut Error'a çevirir. **Tek dönüş yolu, her backend'de aynı.**

Bu, `ENTER_TRY`'ın JIT'e girmesinin de önkoşulu; ama try/catch bu planın
kapsamında değil.

### 3.3 Tek kayıt tablosu

`hostFnTable()` (44 kayıt) ve `BuiltinMethodRegistry` (26 kayıt) tek bir
`HostRegistry`'de birleşir:

```cpp
struct HostEntry {
    const char*  symbolicId;    // "MATH_SQRT" | "ARRAY_PUSH"
    uint8_t      arity;
    uint8_t      flags;         // NeedsHeap | NeedsCaps | Mutating | Pure
    HostKind     retKind;
    const HostKind* paramKinds;
    HostThunk    thunk;
};
const std::vector<HostEntry>& hostRegistry();
```

`CALLHOST` artık `functionName` string'ine bakmaz; `intValue` **tek** bir
registry indeksidir. Sıcak yoldaki string karşılaştırması tümüyle kalkar.

`flags` alanı backend'in karar vermesini sağlar: `Pure` işaretli bir thunk
heap'e dokunmaz, GC güvenli noktası gerektirmez, JIT onu koşulsuz çağırabilir.

### 3.4 Backend köprüsü — tek trampoline

JIT tarafında **tek** import:

```c
extern "C" int rt_host_call(int32_t entryId, HostCallFrame* f);
```

Bir backend'i host çağrılarına açmak = bu tek fonksiyonu import etmek +
argümanları `HostSlot` dizisine yazmak. 70 trampoline yerine 1.

`print` bugünkü özel-durumundan çıkar, registry'de sıradan bir kayıt olur.
Mevcut `rt_jit_print_*` trampolinleri silinir.

---

## 4. Kapsam ve etkisi

### Bu plan neyi açar

| Aile | Fixture | Ek gereksinim |
|---|---|---|
| `__ffi__` (math/date/sys/caps) | 10 | yok — bu plan yeter |
| `builtin::` string metodları | 5 | `SlotType::Str` zaten JIT'te var |

**Beklenen: 73/119 → ~88/119 (%61 → %74).** Tek satırlık gate kalkar.

### Bu plan neyi AÇMAZ

- **array/struct (11 + 5 fixture)** — `SlotType::Ref` ve GC shadow stack
  gerektirir. `HostSlot` `Ref`'i taşıyabilir, ama JIT register'da GC'nin
  göreceği bir kök tutamaz. **Ayrı iş, ürün sahibi onayı gerekir.**
- **try/catch (5)**, **global (5)**, **`LOAD_NULL` (3)** — bağımsız işler.

Bu ayrım kasıtlıdır: host ABI'si GC kararına bağımlı olmadan tamamlanabilir ve
GC işi geldiğinde ABI hazır olur (`HostSlot::Ref` + `flags & NeedsHeap` zaten
tanımlı).

---

## 5. Uygulama sırası

Her adım kendi başına yeşil test bırakır.

1. **`HostSlot` + `HostCallFrame` + `HostError`** tanımlanır; `Value` ↔ `HostSlot`
   dönüşümü yazılır. Hiçbir çağıran değişmez. *(davranış değişikliği yok)*
2. **`HostRegistry`** kurulur; `hostFnTable()` ve builtin tablo ona taşınır.
   Sembolik id çözümü tek yerden. `root.sqt` değişmez. *(davranış yok)*
3. **44 host fonksiyonu** yeni imzaya çevrilir; `throw` → `err`. VM `callHostFn`
   yerine `rt_host_call` kullanır. *(VM davranışı birebir korunur — golden testler
   kanıt)*
4. **26 built-in metod** aynı şekilde. `dispatchBuiltinMethod`'un 82 case'lik
   `switch`'i registry'ye dağılır.
5. **IR üretimi**: `CALLHOST` `functionName` yerine registry indeksi taşır.
   `print` sıradan kayıt olur.
6. **JIT köprüsü**: `rt_host_call` import edilir, `isSupportedCallhost` kalkar,
   `CALLHOST` `OP_JIT` alır. `rt_jit_print_*` silinir.
7. **Doğrulama**: golden testler + `#207` aşama-doğrulama harness'i ile VM≡JIT
   birebir; §1'deki FFI mikro-benchmark'ı ile hız regresyonu olmadığı gösterilir.

Adım 1–2 geri alınabilir ve risksiz. Adım 3–4 mekanik ama geniş. Adım 5 IR
biçimini değiştirir — `saqut ir` çıktısı ve `OPCODE-IR.md` güncellenir.

---

## 6. Karar gereken noktalar

1. **`Instruction::functionName`** `CALL` için de kullanılıyor. Registry indeksi
   yalnız `CALLHOST`'a mı uygulanır, yoksa `CALL` de indekslenir mi?
   *Öneri: yalnız `CALLHOST` — `CALL` ayrı iş.*
2. **`HostError`** saQut `Error` struct'ıyla aynı alanlara mı sahip olsun?
   *Öneri: evet, çeviri maliyeti sıfır olsun.*
3. **`print`'in `outputSink_`'i** (DAP modu) `HostEnv`'e taşınır — JIT'te DAP
   çıktısı bugün zaten çalışmıyor. Bu planla çalışır hale gelir mi, yoksa
   kapsam dışı mı?
   *Öneri: `HostEnv`'e taşı, JIT'te bağlamayı sonraya bırak.*
