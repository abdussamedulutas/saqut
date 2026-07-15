# saQut — MIR Entegrasyon Planı (#80, ADR-032)

> **ADR kısaltmaları bu belgede geçtikçe açıklanıyor** (parantez içi kısa
> tanım) — numarayı ezbere bilmek gerekmiyor, her geçtiği yerde ne olduğu
> yazılı.

> Durum: **tasarım belgesi** — bu belgede kod yazılmaz. Amaç: saQut IR'inin
> [MIR](https://github.com/vnmakarov/mir) (vnmakarov/mir, `../Workspace/mir`
> altında incelendi) üstünden makine koduna nasıl derileceğini, hangi
> opcode'un hangi MIR mekanizmasına eştüşeceğini, GC shadow-stack ve ileride
> gelecek multithread ile nasıl bir arada çalışacağını somutlaştırmak.
> Uygulama sırası `PLAN.md`'nin 0.8.0 bölümünde; **koda başlamadan önce DUR
> ve sor** kısıtı hâlâ geçerli — bu belge yalnızca o konuşmayı somut zemine
> oturtur.

## 0. İki düzeltme (önceki taslaktan)

- **Determinizm ≠ optimizasyon seviyesini kısıtlamak.** MIR'in işi performans;
  bırakılacak. **ADR-032** (backend kararı: VM referans, MIR JIT + linker'sız
  AOT ikinci yol, LLVM fiilen kapalı) — bu ADR'nin "determinizm > performans" ilkesi, MIR'in içindeki
  register allocator/DCE gibi klasik codegen optimizasyonlarını sınırlamakla
  ilgili değil — LLVM'de reddedilen şey UB-sömüren agresif dönüşümlerdi
  (kaynak-seviyesi anlam değişikliği riski). MIR'in optimizasyonları (RA,
  dead-code, instruction combining) klasik derleyici teorisi, her çalıştırmada
  **aynı girdi → aynı çıktı** (deterministik) — VM ile bit-bit eşleşme
  gereksinimini (ADR-032 §4, diferansiyel test) tehdit etmez. Varsayılan
  seviye: **`MIR_gen_set_optimize_level(ctx, 2)`** (MIR'in kendi
  benchmark'larında gcc -O2'ye kıyaslandığı seviye). Gerekirse ölçülüp
  değiştirilir — önsel kısıtlama yok.
- **Determinizmin gerçek tehdidi kapsam kayması.** Bir modülün kendi işi
  dışına taşması (örn. IR generator'ın MIR detaylarını bilmesi, ya da MIR
  header'larının proje geneline sızması) — asıl risk bu, aşağıdaki §1 bunu
  ele alıyor.

## 1. İzolasyon ilkesi — tek giriş noktası

```
src/backend/mir/
  mir_backend.hpp        ← DIŞARIYA açılan TEK arayüz: IRProgram → çalıştırılabilir fonksiyon işaretçisi
  mir_codegen.cpp         ← IRFunction gezici: her Opcode için MIR_new_insn çağrısı (bu belgenin §4'ü)
  mir_value_abi.hpp       ← MirValue POD struct + VM Value ↔ MirValue dönüştürücüler (§3)
  mir_runtime_calls.cpp   ← MIR'den çağrılan C-linkage trampoline'lar (struct/array/string/decimal/
                             CALLHOST/try-catch runtime yardımcıları — §4, §7)
  mir_shadow_stack.hpp    ← GC kök yığını (§8)
```

**Kural:** `<mir.h>`/`<mir-gen.h>` YALNIZCA bu klasördeki `.cpp` dosyalarına
include edilir. `src/ir/*`, `src/vm/*`, `src/cli/*` MIR'i hiç bilmez —
`mir_backend.hpp`'nin dışa açtığı tek fonksiyon imzası dışında:

```cpp
// src/backend/mir/mir_backend.hpp — projenin geri kalanının gördüğü TEK şey
using CompiledFn = int (*)(Value* slots, Heap* heap, /* ... */);
CompiledFn compileFunction(const IRFunction& fn, MirCompileContext& ctx);
```

**Vendoring:** `../Workspace/mir` kaynağı repoya kopyalanır (`third_party/mir/`
ya da CMake `FetchContent` — ikisi de kabul edilebilir, tercih CMake
`FetchContent` çünkü güncelleme/pin sürüm numarasıyla iz sürülebilir olur).
Saf C, ~20K satır, bağımlılıksız — statik link, `.gitignore`'a girmez (build
çıktısı hariç).

**Aktivasyon noktası — TEK dispatch:** `src/cli/commands/run.hpp` (ve
`exec.hpp`) zaten VM'yi kurduğu tek yer; MIR yolu da SADECE oradan, tek bir
`if` ile devreye girer:

```cpp
if (args.backend == Backend::Jit) {
    auto compiled = mir_backend::compileFunction(mainFn, mirCtx);
    return compiled(...);
} else {
    Interpreter vm(...);   // bugünkü yol, değişmez
    return vm.run(...);
}
```

`Backend` enum'ı bugün `{Vm, Jit}` — **0.8.0'da varsayılan `Vm`, `--jit`
ile `Jit` seçilir** (JIT stabilize olana kadar). **1.0.0'da varsayılan
`Jit`/`Aot` olur, VM'e erişim `--interpret` (adı tartışılır) bayrağıyla
debug/adım-adım çalıştırma amaçlı kalır** — bu, ADR-032'nin kendi yol
haritasında zaten yazılıydı ("stabilize olunca varsayılan"); burada netleşen
şey yalnızca mekanizma: **bayrak yön değiştirir, dispatch noktası
değişmez** — `run.hpp`'deki tek `if` her iki sürümde de aynı şekli korur.

### §1.1 Arayüz şekli — bugünkü hâl ve ileride açılması (not)

Dilim 1'de inşa edilen gerçek arayüz, yukarıdaki taslak `compileFunction(...)`
DEĞİL; tek çağrıda init→derle→çalıştır→finish yapan bütün-program biçimidir:

```cpp
bool tryCompileAndRunProgram(IRProgram&, int& outExitCode,
                             UnsupportedReason&, StageTimer* = nullptr);
```

Bu, "kısmi JIT yok — programın TAMAMI desteklenmeli" kuralı için DOĞRU şekil
(fonksiyon-fonksiyon derleyip ortada takılmaz). Ama şu üç iş **derle/çalıştır
ayrımını** zorunlu kılacak — ileride bu arayüz açılmalı:

- **AOT (`saqut build`, #81):** derlemeyi çalıştırmadan üretip diske
  gömmeli (init→derle→**serialize**, run yok).
- **Diferansiyel test (#92):** VM ile JIT'i AYNI süreçte, `exit()` olmadan
  çalıştırıp stdout'u kıyaslamalı. ⚠️ Bugün `rt_jit_div_zero` doğrudan
  `std::exit(1)` çağırıyor (Dilim 1 kanıtı için kabul edilir) — in-process
  diff'ten önce bu, yakalanabilir bir hata-yayma yoluna çevrilmeli, yoksa
  test süreci sonlanır. Bu, "cage" ilkesiyle de uyumlu (bir runtime yardımcısı
  tek taraflı süreç sonlandırmamalı).
- **Debuggable-mod (#109):** context canlı tutulup tek fonksiyon yeniden
  derlenmeli (§11).

**Şimdilik yapılacak bir şey yok** — yalnızca "bütün-program `tryCompileAndRun`
şekli Dilim 1 için doğru ama #81/#92/#109 geldiğinde derle/çalıştır ayrımına
evrilecek" notu düşülüyor ki o dilimlerde sürpriz olmasın.

## 2. Fonksiyon/modül eşlemesi

Her `IRFunction` → bir `MIR_item_t` (MIR fonksiyonu), hepsi tek bir
`MIR_module_t` içinde (saQut programı = tek MIR modülü, çok-modüllü saQut
derlemesi zaten IR üretiminde tek `IRProgram`'a düzleştiriliyor — MIR
tarafında ekstra bir kavram gerekmiyor).

- saQut slot'ları (`slots[0..N]`) → MIR local register'ları
  (`MIR_new_func_reg`). **Tip seçimi tek tip değil** — bkz. §3 (Value ABI):
  slot'un o anki statik "kind"ine göre `MIR_T_I64` (int/bool/byte/date/ref
  pointer) ya da `MIR_T_D` (float) local'i açılır. saQut'ta bir slot
  runtime'da tip değiştirmez (tip denetleyici bunu garanti eder — **ADR-020**:
  primitive=değer/struct-array=referans semantiği, **ADR-026**: `as` ile açık
  tip dönüşümü)
  → JIT-zamanı tip biliniyor, register tipi statik seçilebilir.
- `moduleSlots_` (modül-düzeyi değişkenler) → MIR `bss`/`data` item'ı
  (`MIR_new_bss`) ya da C tarafında ayrılmış sabit bir dizi + `MIR_new_import`
  ile referans (ikincisi tercih edilir: VM ile aynı `Heap`/`moduleSlots`
  belleğini paylaşmak GC kök taramasını basitleştirir, tek kaynak kalır).

## 3. Değer temsili — MirValue ABI (açık tasarım kararı)

**Sorun:** VM'nin `Value` sınıfı (`src/vm/value.hpp`) `std::string`,
`DecimalValue`, `Object*` alanlarını aynı struct'ta taşıyan "şişman" bir
C++ nesnesi — MIR local'leri yalnızca **64-bit int, float, double, long
double** olabilir (MIR.md, "Registers can contain only one type value").
`Value`'yu doğrudan MIR register'ına koyamayız.

**Karar — iki katmanlı temsil:**

1. **Register'da (JIT'lenmiş kod içinde çalışırken):** yalnızca skaler.
   - `Int`/`Bool`/`Byte` → `MIR_T_I64` (tek register, işaret genişletmeli)
   - `Float` → `MIR_T_D`
   - `Date` → `MIR_T_I64` (zaten `int64Value` epoch-ms, doğrudan taşınır)
   - `String`/`Decimal`/`Ref` (struct/array) → `MIR_T_I64` ama **pointer**
     olarak yorumlanır: heap'te VM'in bugünkü `Object*`'iyle aynı ailede bir
     nesneye işaret eder. **String bugün VM'de kutusuz/inline tutuluyor**
     (**ADR-024**: string immutable-değer-tipi kararı, iç temsil UTF-8 —
     `Value` sınıfının içine doğrudan gömülü, ayrı heap nesnesi değil) —
     **JIT sınırında bu geçerli değil**, JIT'in gördüğü her `String`
     değeri heap'e kutulanmış olmalı (`StringObject : Object` yeni bir tip,
     `object.hpp`'ye eklenecek — bugün yok). Bu, VM Value modeliyle JIT'in
     kutulama biçimi arasında **kasıtlı bir fark** — aşağıda not edildi.
2. **Sınırda (fonksiyon çağrısı JIT↔VM geçişi, CALLHOST, global slot okuma/
   yazma):** `MirValue` adlı POD, C-uyumlu ABI struct:
   ```c
   typedef struct {
       uint8_t kind;   // ValueKind ile bire bir
       union { int64_t i; double f; void* obj; } as;
   } MirValue;
   ```
   `mir_value_abi.hpp` içinde VM'in C++ `Value`'sundan `MirValue`'ya ve geri
   dönüştüren fonksiyonlar (`toMir(const Value&)`, `fromMir(const MirValue&)`)
   — string'i kutulama/kutu-açma da burada olur.

**Açık soru — yeni bir ADR gerekiyor, öneri: `ADR-037` (henüz yazılmadı;
kısaca: "JIT tarafında string neden kutulu tutulur" kararının kayıt altına
alınması):** String'in VM'de inline, JIT'te kutulu olması iki backend'in
bellek modelini farklılaştırıyor; **ADR-032**'nin (bkz. yukarı) "VM ve JIT
aynı IR'de bit-bit aynı çıktı vermek ZORUNDA" kuralı **gözlemlenen davranış**
(stdout, dönüş değerleri) için geçerli, iç temsil için değil — yani bu fark
kural ihlali değil, ama okuyucuya açıkça yazılmalı. MIR implementasyonuna
başlamadan önce `ADR-037` yazılmalı.

**Decimal notu:** `DecimalValue` (`src/core/decimal.hpp`) `{int64_t coeff;
/* scale */}` — teorik olarak iki `I64` register'a açılabilir (kutusuz), ama
ölçek hizalama mantığı (`DADD`'de farklı `scale`'li iki decimal'i toplamadan
önce hizalamak) MIR aritmetiğine elle yazmaya değecek kadar sık/kritik bir
yol değil → **v1: Decimal her zaman kutulu (heap'te `DecimalObject*` ya da
`MirValue.obj` içinde pointer), aritmetik runtime call'a gider** (§4). İnce
ayar/inlining ölçüm sonrası ayrı bir iyileştirme turu.

## 4. Opcode → MIR eşleme tablosu

| saQut Opcode | MIR karşılığı | Not |
|---|---|---|
| `LOAD_CONST` | `MIR_MOV reg, imm` | doğrudan |
| `LOAD_FLOAT` | `MIR_DMOV reg, imm` | double |
| `LOAD_NULL` | `MIR_MOV reg, 0` | null = `nullptr`/0 sentinel; tip etiketi register dışı statik bilgide (§3) |
| `LOAD_STRING` | runtime call `rt_intern_string(ptr,len)` | derleme zamanı sabit string `MIR_new_string_data` ile modüle gömülür, çağrı yalnızca `StringObject` kutusu üretir |
| `LOAD_DECIMAL` | runtime call `rt_decimal_new(coeff,scale)` | §3 |
| `LOAD_SLOT` | `MIR_MOV`/`MIR_DMOV reg, reg` | skalerse register kopya, ref/kutulu ise pointer kopya (aynı MOV, I64 pointer taşır) |
| `ADD`/`SUB`/`MUL` | `MIR_ADD`/`SUB`/`MUL` | int64 |
| `DIV`/`MOD` | `MIR_DIV`/`MOD` + önce `right==0` kontrolü → `MIR_BF`/`BT` ile runtime `rt_throw_div_zero()` çağrısına dallan | sıfıra bölme kontrolü VM'deki davranışla birebir |
| `FADD`/`FSUB`/`FMUL`/`FDIV` | `MIR_DADD`/`DSUB`/`DMUL`/`DDIV` | double; `FDIV` sıfır kontrolü `DIV` gibi |
| `FNEG` | `MIR_DNEG` | |
| `BAND`/`BOR`/`BXOR` | `MIR_AND`/`OR`/`XOR` | |
| `SHL`/`SHR` | `MIR_LSH`/`MIR_RSH` | saQut int işaretli — `RSH` (aritmetik kaydırma) kullanılır, `URSH` değil |
| `BNOT` | `MIR_XOR reg, src, -1` | MIR'de doğrudan NOT yok, -1 ile XOR |
| `LESS`/`LESS_EQUAL`/`GREATER`/`GREATER_EQUAL`/`EQUAL_EQUAL`/`NOT_EQUAL` | int/Date operand → `MIR_LT/LE/GT/GE/EQ/NE`; float operand → `MIR_DLT/DLE/DGT/DGE/DEQ/DNE`; **string/decimal operand → runtime call** (`rt_string_eq`, `rt_decimal_cmp`) çünkü içerik karşılaştırması (**ADR-023**: `==` primitive'de değer/referansta
kimlik ama string istisna içerik, **ADR-024**: string immutable) native MIR karşılaştırması değil | opcode aynı, hedef seçimi statik tipe göre codegen'de dallanır (VM'nin bugünkü `ValueKind` switch'iyle birebir aynı mantık, derleme zamanına taşınmış hali) |
| `JMP` | `MIR_JMP label` | |
| `JIF_FALSE` | `MIR_BF label, cond` | |
| `JIF_TRUE` | `MIR_BT label, cond` | |
| `CALL` | `MIR_CALL proto, target_func_ref, dest, args...` | hedef başka bir saQut `IRFunction`'ın MIR item'ı (§2); tip-imzalı `MIR_new_proto` bir kere fonksiyon başına üretilir |
| `RETURN` | `MIR_RET` | |
| `INT_TO_FLOAT` | `MIR_I2D` | |
| `FLOAT_TO_INT` | `MIR_D2I` | (checked değil — desugar zaten kesin dönüşüm gerektiren yerlerde kullanılıyor) |
| `CAST_FLOAT_TO_INT_CHECKED` | `MIR_D2I` + NaN/aralık kontrolü (`self != self` → NaN; `< INT_MIN`/`> INT_MAX` → taşma) + `MIR_BT` → runtime `rt_throw_or_null(...)` | fallible dönüşüm, VM'deki `left` bayrağı (Error/null) runtime call'a taşınır |
| `CAST_INT_TO_BYTE_CHECKED` | aralık `MIR_BLT`/`BGT` (0-255 dışı) → runtime throw/null | |
| `CAST_INT_TO_STR`/`CAST_FLOAT_TO_STR`/`CAST_BOOL_TO_STR` | runtime call (`rt_int_to_str` vb.) | biçimlendirme mantığı C++'ta kalır, MIR'e taşınmaz |
| `CAST_STR_TO_INT`/`CAST_STR_TO_FLOAT`/`CAST_STR_TO_DECIMAL` | runtime call, fallible sonucu `MirValue{kind, ...}` olarak döner, çağıran taraf `kind==Error` ise throw dallanır | |
| `DADD`/`DSUB`/`DMUL`/`DDIV`/`DMOD`/`DNEG` | runtime call (`rt_decimal_add` vb.) | §3 |
| `INT_TO_DECIMAL`/`FLOAT_TO_DECIMAL`/`CAST_DECIMAL_*` | runtime call | |
| `STRUCT_NEW` | runtime call `rt_struct_new(fieldCount, typeName)` → `Object*` | Heap alloc VM'deki `Heap::allocStruct` ile AYNI fonksiyon (kod paylaşımı — MIR yeniden yazmaz, `Heap`'i doğrudan çağırır) |
| `FIELD_GET`/`FIELD_SET` | runtime call ya da (performans turu sonrası) doğrudan `MIR_MOV` ile `StructObject::fields[idx]` bellek erişimi (`MIR_new_mem_op`, sabit offset) | v1: runtime call (basit, güvenli); optimizasyon: alan offset'i derleme zamanı sabit olduğundan doğrudan bellek erişimi MÜMKÜN — ama `Object` sınıfı virtual (`markChildren`) içerdiğinden vtable offset'i hesaba katılmalı, bu yüzden v1'de runtime call ile başlanması önerilir |
| `ARRAY_NEW`/`ARRAY_GET`/`ARRAY_SET`/`ARRAY_LEN` | runtime call (`rt_array_new/get/set/len`) — sınır kontrolü runtime call İÇİNDE (VM'deki mantık birebir) | |
| `LOAD_GLOBAL`/`STORE_GLOBAL` | `MIR_MOV` ile `moduleSlots` dizisine sabit offset erişim (§2) | skaler; ref ise pointer taşınır, GC kök taraması `moduleSlots`'u zaten tarıyor (Heap::markSlots) — JIT bunu değiştirmez |
| `STRING_CONCAT` | runtime call `rt_string_concat` | yeni string üretir (ADR-024 immutable) |
| `ENTER_TRY`/`LEAVE_TRY`/`THROW` | runtime call üçlüsü — §7, AÇIK TASARIM SORUSU | |
| `CALLHOST` | runtime call `rt_callhost(hostFnId, argsPtr, argc, &HostContext)` | mevcut `callHostFn`/`host_functions.cpp` **birebir aynı** çağrılır — FFI kısmı MIR'de yeniden yazılmaz (§7) |

## 5. Kontrol akışı ve backpatch

saQut IR zaten `jumpTarget`'ı instruction indeksi olarak taşıyor (VM
`Interpreter`'ın bugün yaptığı gibi). MIR codegen'i iki geçişte çalışır:

1. **Geçiş 1:** her IR instruction indeksi için bir `MIR_label_t` üret
   (`MIR_new_label`), henüz insn eklemeden (adres tablosu: `vector<MIR_label_t>
   labelAt`).
2. **Geçiş 2:** instruction'ları sırayla MIR'e çevir; `JMP`/`JIF_*` görülünce
   `labelAt[instr.jumpTarget]`'ı kullan — **backpatch'e gerek yok**, VM'in
   kendi backpatch mekanizması IR üretiminde zaten çözülmüş durumda (IR
   instruction'ları üretildiğinde `jumpTarget` kesin biliniyor), MIR tarafı
   yalnızca hazır hedefi tüketir.

## 6. Fonksiyon çağrısı ve CALLHOST — runtime köprüsü

`CALL`: hedef fonksiyon başka bir saQut fonksiyonuysa, o da aynı derleme
biriminde bir MIR `func` item'ı → doğrudan `MIR_CALL`. **saQut fonksiyonları
arası çağrı MIR içinde kalır, C++'a geri dönmez** (asıl performans kazancı
burada — VM'nin her çağrıda yeni `Frame` push/pop maliyeti yok).

`CALLHOST`: her zaman C++ tarafına geçer. Tek bir `MIR_new_import` ile
projede zaten var olan `callHostFn(int id, const std::vector<Value>&,
HostContext&)` imzasına **çok yakın**, C-linkage bir trampoline
(`mir_runtime_calls.cpp`'de):

```cpp
extern "C" MirValue rt_callhost(int hostFnId, MirValue* args, int argc,
                                 HostContext* ctx) {
    std::vector<Value> vArgs; vArgs.reserve(argc);
    for (int i = 0; i < argc; i++) vArgs.push_back(fromMir(args[i]));
    Value result = callHostFn(hostFnId, vArgs, *ctx);   // AYNI fonksiyon, değişmez
    return toMir(result);
}
```

**Kazanım:** `src/ffi/host_functions.cpp` (fs/sys/math/date/caps — tüm
0.7.0 stdlib dalgası) **hiç değişmez**, MIR'e taşınmaz, yeniden yazılmaz.
FFI seam zaten "host fonksiyonu çağır" olacak şekilde tasarlanmıştı
(**ADR-016**: `callhost` mekanizması, `print` ilk müşterisi)
— MIR bu seam'i olduğu gibi kullanıyor, genişletmiyor.

## 7. Hata yönetimi (ENTER_TRY/LEAVE_TRY/THROW) — AÇIK TASARIM SORUSU

Bu, planın en netleşmemiş köşesi — **koda geçmeden önce ayrı bir onay turu
gerektirir**, burada yalnızca aday yaklaşım kaydediliyor.

VM'nin bugünkü modeli: `TryFrame` yığını + `callDepth` (unwind sınırı) +
C++ `throw`/`catch` (`pendingThrow_`). MIR'in **native exception mekanizması
yok** (C'nin de yok) — üç aday:

1. **setjmp/longjmp tabanlı runtime köprü** (önerilen): `ENTER_TRY` →
   runtime call `rt_try_push(catchLabelAddr, shadowStackDepth)` (içeride
   `jmp_buf` diziye `setjmp` ile itilir); `THROW` → `rt_throw(errorValue)`
   (en üstteki `jmp_buf`'a `longjmp`); JIT'lenmiş kod `longjmp` sonrası
   doğrudan `catchLabelAddr`'a döner (MIR `JMPI` — indirect jump — ile,
   `LADDR`/`JMPI` insn çifti). Avantaj: VM'nin `TryFrame` modeliyle kavramsal
   olarak birebir (yalnızca C++ `throw` yerine C `longjmp`); dezavantaj:
   `setjmp`/`longjmp` C++ nesne yıkıcılarını (destructor) ATLAR — JIT'lenmiş
   kodun kendisi skaler/pointer register kullandığından bu ciddi bir sorun
   değil, ama shadow stack'in (§8) `longjmp` sırasında doğru derinliğe geri
   sarılması runtime call'ın sorumluluğu olmalı.
2. **Dönüş-kodu tabanlı yayılma** (her `CALL` sonrası örtük hata kontrolü):
   basit ama her çağrı sitesine ekstra dallanma ekler — VM'nin bugünkü
   "unchecked try/catch" felsefesiyle (**ADR-025**: Swift-tarzı struct-tabanlı
   hata + klasik unchecked try/catch, kullanıcı tarafında görünmez)
   çelişmez ama JIT kod boyutunu/kod-üretim karmaşıklığını arttırır.
3. **Native OS istisna mekanizması** (SEH/DWARF unwind tabloları MIR'e elle
   yazılır): en hızlı ama platform-bağımlı, MIR'in kendisi bunu sunmuyor —
   elenir (kısıt #1: sıfır harici toolchain + platform-bağımsızlık).

**Öneri:** (1) — ama bu belge onu KARARLAŞTIRMIYOR, yalnızca öneriyor.
Uygulamaya geçmeden önce kullanıcıyla ayrı doğrulanmalı (ADR-025'in
"deterministik stacktrace" gereksinimiyle `longjmp`'in stack unwind sırasında
satır/iz bilgisini nasıl koruyacağı da netleşmeli).

### §7.1 Performans profili — "try yoksa maliyet yok" (kullanıcı endişesi)

Kullanıcı endişesi: JIT'lenmiş kodun **runtime'da yavaşlaması/tıkanması**
istenmiyor. setjmp/longjmp modeli bu hedefi rakiplerinden DAHA İYİ karşılar:

- **try/catch KULLANMAYAN kod → sıfır maliyet.** setjmp/longjmp yalnızca
  `ENTER_TRY`'da kod üretir. İçinde hiç `try` olmayan bir fonksiyon (sıcak
  döngüler dahil) hata yönetimi adına **tek bir ekstra talimat bile**
  taşımaz. Bu, "normal yol bedava" güvencesidir — asıl istenen bu.
- **`throw` (longjmp) yavaş yoldur** — ama istisna zaten istisnai; sıcak
  yolda değil. Maliyet atış anında ödenir, her talimatta değil.
- **Bu bir "tıkanma" (per-instruction check) DEĞİL.** longjmp tüm programı
  yavaşlatmaz, non-try kod runtime'ına hiç dokunmaz. VM'in eski yorumlayıcı
  safepoint'i gibi her adımda çalışan bir kontrol yok.
- **Kıyas — neden aday (2) elendi bu açıdan da doğru:** dönüş-kodu yayılması
  HER `CALL` sonrası bir dallanma ekler → maliyeti tüm programa yayar. İşte
  "runtime yavaşlaması" tam olarak budur; setjmp/longjmp bundan kaçınır.

**Tek gerçek yerel maliyet — ve §11 ile birleştirilerek çözülür:** longjmp
sonrası MIR register'larının değeri **belirsizdir** (setjmp semantiği: yalnızca
setjmp ile longjmp arasında değişmemiş ya da bellek-destekli değerler güvenli).
JIT'te saQut slot'ları register'da yaşıyor → catch bloğunun okuyacağı, try
gövdesinde DEĞİŞTİRİLMİŞ slot'lar çöp olabilir. Çözüm: **try gövdesi içindeki
slot'ları serbest register'a değil, §11'in debuggable-mod için tarif ettiği
sabit-offsetli bellek çerçevesine (`slots[]`) koy** — catch bellekten okur,
register çöpünden değil. Bu maliyet YALNIZCA try gövdesi içine yazılan slot'lar
için bir bellek-store'dur (§8 shadow-stack'in ref-yazma modeliyle aynı desen,
"~%5-10 ek yazma"); try dışındaki sıcak döngü ETKİLENMEZ. Yani:

| Kod şekli | Ek maliyet |
|---|---|
| try/catch yok | **sıfır** (hiçbir şey üretilmez) |
| try var, sıcak döngü try DIŞINDA | sıfır (döngü serbest register'da) |
| sıcak döngü try İÇİNDE | değişen slot başına bir bellek-store (yalnız orada) |
| `throw` atış anı | longjmp (yavaş yol, istisnai) |

**Sonuç:** setjmp/longjmp önerisi kullanıcının "runtime tıkanması istemiyorum"
kısıtını KARŞILAR — try/catch mekanizması §11 bellek-çerçevesiyle birleştiği
sürece maliyet yalnızca try bloklarına yereldir, program geneline yayılmaz. Bu
birleştirme (Dilim 5 = Dilim 6/§11 mekanizmasını devralır) uygulama sırasına
yazıldı (§10).

## 8. GC — shadow stack somutlaştırma

ADR-032 §3'te taslak: "JIT'lenmiş fonksiyon girişte N slot açar, referans
atamalarını gölge slota da yazar, çıkışta kapatır." Somut hâli:

```c
typedef struct { Object** slots; int count; int capacity; } ShadowFrame;
extern ShadowFrame* g_shadowStack;   // thread-local (bkz §9)
```

- Her JIT'lenmiş `IRFunction` girişinde: `rt_shadow_push(frameSize)` —
  o fonksiyonun **ref-tipli slot sayısı kadar** (derleme zamanı statik
  olarak bilinir — tip denetleyici çıktısı) bir çerçeve açar.
- Her `Ref` kind'lı slot'a yazma (`STRUCT_NEW`/`ARRAY_NEW`/`FIELD_GET`/
  `ARRAY_GET`/`LOAD_SLOT` ref taşıyorsa/`CALL` dönüş değeri ref ise) MIR
  codegen'i **otomatik olarak** aynı pointer'ı gölge slotun ilgili
  index'ine de yazan bir `MIR_MOV` ekler (opcode başına elle değil,
  codegen'in "bu slot ref mi?" statik bilgisine göre otomatik enjekte
  ettiği tek bir kural — ADR-032'nin öngördüğü "~%5-10 ek yazma" maliyeti
  budur).
- Fonksiyon çıkışında (her `RETURN` öncesi, ve `rt_throw` unwind yolunda —
  §7 ile bağlı): `rt_shadow_pop()`.
- `Heap::markSlots` (bugün VM'nin `moduleSlots_`/frame slot'larını taradığı
  yer) MIR yolunda **aynı fonksiyon**, ek olarak `g_shadowStack`'i de tarar
  — GC'nin kendisi (**ADR-022**: basit taşımasız stop-the-world mark-sweep)
  değişmez, yalnızca kök kaynağı bir
  tane daha (VM frame'leri yerine/yanında shadow stack) ekleniyor. **Cam
  kutu kriteri korunur:** `saqut gc --dump-roots` gibi bir komut ileride
  shadow stack'i olduğu gibi dökebilir (sıradan bir dizi, konservatif tahmin
  yok — ADR-032'nin vaadi).

## 9. İleride: multithread ile birlikte çalışma

MIR'in kendi garantisi (README): **"Farklı thread'lerde farklı
`MIR_context_t` kullanılırsa senkronizasyon gerekmez."** Bu, saQut'un
ileride (v1.1.0+, bu planın kapsamı dışı — yalnızca uyumluluk notu) çoklu
iş parçacığı eklemesi durumunda şu şekli zorluyor:

- **Context-per-thread**: her worker thread kendi `MIR_context_t`'ini açar;
  derlenmiş modül (makine kodu) tüm thread'ler arasında paylaşılabilir
  (kod salt-okunur, sorun yok) ama **derleme sırasında** (henüz derlenmemiş
  bir fonksiyon JIT'leniyorken) iki thread aynı `MIR_context_t`'i kullanmaz.
- **GC kökleri thread başına**: `g_shadowStack` (§8) **thread-local** olmalı
  — her thread kendi shadow stack'ini taşır, stop-the-world mark-sweep
  (ADR-022, hâlâ tek-thread'li kalıyor — GC'nin kendisi paralelleşmiyor,
  yalnızca kök TARAMASI tüm thread-local shadow stack'leri gezer) tüm
  thread'leri bir safepoint'te durdurup hepsinin shadow stack'ini tek tek
  tarar.
- **Heap paylaşımı**: `Heap` tek, tüm thread'ler aynı nesnelere referans
  verebilir (saQut'un referans semantiği, ADR-020) — bu, mark-sweep'in
  stop-the-world olma gerekçesini GÜÇLENDİRİR (paralel thread'ler heap'i
  aynı anda değiştiriyorken concurrent GC çok daha karmaşık olurdu; ADR-022
  zaten stop-the-world seçmişti, bu seçim multithread'e de taşınabilir
  kalıyor — yeniden tasarım gerekmiyor).
- Bu bölüm **bilgilendirme amaçlı** — multithread saQut'ın kendi ayrı
  issue'su/ADR'si olmalı, burada yalnızca "MIR bunu destekler, mimari
  buna kapalı değil" güvencesi veriliyor.

## 10. Aşamalı uygulama sırası (dikey dilim)

CLAUDE.md ilkesi ("önce uçtan uca tek dikey dilim, sonra çerçeve") burada da
geçerli — tüm opcode tablosunu tek seferde uygulamak yerine:

1. **Dilim 0 — iskelet:** `src/backend/mir/` klasörü, CMake `FetchContent`,
   `mir_backend.hpp` boş arayüz, tek bir `int add(int,int)` saQut
   fonksiyonunu (yalnızca `LOAD_CONST`/`ADD`/`RETURN`) gerçekten JIT'leyip
   çalıştıran kanıt. GC/try-catch/string yok.
2. **Dilim 1 — int-skaler tam küme (TAMAMLANDI):** tüm int aritmetik+
   bitsel+karşılaştırma+kontrol akışı+fonksiyon çağrısı (CALL/RETURN) +
   print(int). `fibonacci.sqt` MIR ile çalışıyor, VM ile aynı çıktı.
   ⚠️ **float HENÜZ YOK** — Dilim 1'in kapsamı yalnızca int-skaler; float
   Dilim 1.5'e taşındı çünkü register-tip seçimini (I64↔D) zorlayan ilk
   özellik o (aşağı bkz).
2b. **Dilim 1.5 — slot-tip tablosu + float (TAMAMLANDI ✅):** Yapıldı:
   - `IRFunction::slotTypes` (`std::vector<SlotType>`) eklendi; `SlotType`
     IR katmanında tanımlı (`ir_function.hpp`), VM `ValueKind`'ına bağımlı
     değil. IRGenerator::finalizeSlotTypes doldurur — parametreler bildirilen
     tipten, geri kalan slotlar üreten opcode'dan (fixpoint tarama; LOAD_SLOT
     propagasyonu, CALL dönüş türü). Ekstra semantik analiz değil (§0 uyumlu).
   - MIR codegen register tipini bu tablodan seçiyor (Float → `MIR_T_D`,
     diğerleri → `MIR_T_I64`). Proto/func imzaları, CALL, print(int/float)
     hepsi türe göre.
   - float opcode'ları eklendi: LOAD_FLOAT, FADD/FSUB/FMUL/FDIV(sıfır guard'lı,
     `MIR_DBNE`)/FNEG, INT_TO_FLOAT/FLOAT_TO_INT, ve karşılaştırmaların
     D-varyantları (operand türüne göre codegen'de seçilir).
   - `rt_jit_print_float` VM'in `Value::toString()` Float dalıyla birebir
     biçim. Diferansiyel test: float aritmetik + fonksiyon çağrısı + döngü,
     VM ile **bit-bit aynı çıktı** (doğrulandı).
   - Slot türü Int/Float DIŞINDA (Ref/Str/Decimal/Date) ise program reddedilir
     — o türler sonraki dilimlerde (kutulama + shadow stack).
   - **Kalan (bu dilimin dışına düşen küçük işler):** `saqut ir --types` cam
     kutu dökümü (**#111**); FIELD_GET/ARRAY_GET/LOAD_GLOBAL sonuç türü
     `slotTypes`'ta Int varsayılıyor (JIT'te zaten reddedildikleri için
     zararsız; Dilim 2/3'te bu opcode'lara sonuç-türü alanı eklenecek).
3. **Dilim 2 — GC'li tipler:** STRUCT_NEW/ARRAY_*/FIELD_* + shadow stack
   (§8) gerçek uygulaması. Ref-slot tespiti Dilim 1.5'in `slotTypes`
   tablosundan gelir.
4. **Dilim 3 — string/decimal/date/cast:** runtime call katmanı (§4 tablosu,
   kutulama). ⚠️ **ÖN KOŞUL: ADR-037 (JIT string kutulama) yazılmış olmalı**
   (§3, açık sorular #1).
5. **Dilim 4 — CALLHOST köprüsü:** mevcut FFI'nin MIR'den çağrılması (§6).
6. **Dilim 5 — try/catch:** §7'deki açık sorunun (**#110**) kararlaştırılıp
   uygulanması (bu dilimden önce AYRI onay turu şart). ⚠️ **§7.1 gereği bu
   dilim, §11/Dilim 6'nın bellek-çerçeveli slot mekanizmasını DEVRALIR**
   (longjmp register-çöpü sorunu ancak böyle çözülür) — yani Dilim 6 hazır
   değilse try gövdesi için en azından o bellek-çerçeve alt-parçası bu
   dilimde inşa edilmeli.

Her dilim kendi commit'i/PR'ı, her dilimden sonra ilgili golden testler VM
ile JIT arasında (o dilimin kapsadığı kadarıyla) diferansiyel karşılaştırılır
— #92'nin gerçek içeriği bu aşamalarla birlikte büyür (önceki tartışmada
kararlaştırıldığı gibi, #92 MIR'den ÖNCE bağımsız bir iskelet olarak değil,
MIR'in dilimleriyle birlikte organik olarak büyüyecek).

## Açık sorular (kod yazımından önce netleşmeli)

0. **Slot-tip tablosu (Dilim 1.5, §10)** — ✅ ÇÖZÜLDÜ. `IRFunction::slotTypes`
   eklendi, IRGenerator dolduruyor, MIR register tiplemesi kullanıyor. Dilim
   2'nin shadow-stack ref-tespiti aynı tablodan gelecek.
1. **ADR-037 (JIT Value ABI)** — ✅ YAZILDI (`docs/adr/ADR-037-jit-value-abi.md`).
   String kutulama farkı VM/JIT arasında kayıt altına alındı. Dilim 3 ön koşulu
   kapandı (kod hâlâ Dilim 3'te yazılacak).
2. **§7 hata yönetimi** — setjmp/longjmp önerisi onay bekliyor. **Performans
   endişesi §7.1'de çözümlendi:** try/catch yoksa sıfır maliyet, maliyet
   yalnızca try bloklarına yerel; register-çöpü sorunu §11 bellek-çerçevesiyle
   giderilir. Kalan açık nokta — deterministik stacktrace ile longjmp'in
   uyumu — **#110'a taşındı** (Dilim 5 kodu bu issue kapanmadan yazılmamalı).
3. **FIELD_GET/FIELD_SET doğrudan bellek erişimi** (v1 runtime call, v2
   muhtemelen doğrudan MOV) — `Object`'in virtual metoduyla (`markChildren`)
   birlikte vtable offset'i nasıl atlatılacağı (örn. `Object`'i non-virtual
   yapıp `ObjectType` enum'una göre `switch` ile mark etmek — zaten `type`
   alanı var, virtual gereksiz olabilir) ayrı bir küçük refaktör önerisi
   olarak not düşülüyor, bu planın kapsamı dışı.
4. **1.0.0'da VM'e erişim bayrağının adı** (`--interpret`/`--debug`/`--vm`) —
   CLI.md'nin `--allow` deseniyle çakışmayan bir isim seçilmeli, küçük bir
   karar, MIR koduna başlamadan hemen önce kapatılabilir.

## 11. DAP/LSP ile ilişki

- **LSP:** JIT/AOT'tan tamamen bağımsız. Hover/definition/completion/
  diagnostics AST + sembol tablosu + tip denetleyici çıktısı üzerinde çalışan
  saf statik analiz — kodu hiç çalıştırmaz. Backend değişikliği LSP'ye
  dokunmaz.
- **DAP — hedef tasarım: Debuggable-mod JIT derlemesi (VM'e kaçış DEĞİL).**
  JIT'lenmiş kod bizim ürettiğimiz makine kodu — istediğimiz noktalara
  istediğimiz çağrıyı gömebiliriz. Fonksiyon başına **iki MIR derlemesi**:
  - **Fast (üretim):** §1-§10'daki tarif — register'lar serbest, hiçbir
    kontrol yok, tam hız.
  - **Debuggable (hata ayıklama):** aynı fonksiyonun ikinci bir derlemesi —
    her IR-satır sınırında (VM'nin bugün `lineToFirstIP` için kullandığı
    aynı granülerlik) bir `rt_debug_safepoint(frame, irLine)` çağrısı
    gömülü. MIR derlemesi mikrosaniyeler sürdüğü için (§0 — MIR'in kendi
    ölçümü ~249µs) bu ikinci derleme yalnızca **breakpoint konulduğu anda,
    yalnızca o fonksiyon için** üretilir — diğer fonksiyonlar Fast modda
    kalmaya devam eder (V8'in tier-up/tier-down mantığına benzer, tüm
    runtime yavaşlamaz).
  - **Slot adresleme:** Debuggable modda saQut slot'ları serbest register'a
    DEĞİL, VM'in `slots[]` dizisiyle aynı sabit-offsetli bir yığın alanına
    yerleştirilir. `IRFunction::slotNames` (bugün DAP'ta zaten kullanılan
    isim eşlemesi) ile birleşince "değişken A şu an bellekte X adresinde"
    sorusu doğrudan yanıtlanabilir — debugger `frame->slots[N]`'in adresini
    okur.
  - **Duraklama:** `rt_debug_safepoint` çağrıldığında bu satırda breakpoint
    var mı / step hedefine ulaşıldı mı kontrol eder; varsa çalışan thread'i
    bloklar (mutex/condvar) — VM'nin bugünkü `runUntilEvent` bütçeli-koşu/
    duraklama sözleşmesiyle aynı, yalnızca interpreter döngüsü yerine
    gerçek native kod içinde bekleyen bir thread var. Step over/into/out
    için gereken çağrı-derinliği sayacı, GC shadow stack'in (§8) zaten
    tuttuğu bookkeeping'le örtüşüyor — aynı veri iki amaca hizmet eder.
    Struct/array inceleme (expand) de aynı shadow stack'teki canlı pointer
    listesini kullanır — ek bir mekanizma gerekmez.
  - **Kademeli devreye alma:** Bu, dilim sıralamasında (§10) JIT'in temel
    aritmetik/kontrol-akışı/GC dilimlerinden SONRA gelen, ayrı bir dilim
    (öneri: Dilim 6 — Debuggable-mod derleme + `rt_debug_safepoint`).
    **0.8.0/erken 1.0.0'da bu dilim henüz yokken** `saqut dap` VM'i
    kullanmaya devam eder (basit, çalışan, geriye dönük uyumlu fallback) —
    bu bir mimari sınırlama değil, yalnızca inşa sırası; Debuggable-mod
    hazır olunca DAP JIT'lenmiş kod üstünde de tam teşekküllü çalışır.
    Ayrı issue olarak kaydedildi: **#109**.
