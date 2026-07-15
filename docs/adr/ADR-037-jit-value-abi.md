# ADR-037 — JIT Value ABI (register-skaler vs kutulu; string kutulama farkı)

İlgili: #80 (MIR JIT), ADR-032 (backend kararı: VM referans + MIR JIT),
ADR-020 (değer/referans semantiği), ADR-024 (string immutable-değer-tipi),
ADR-023 (eşitlik), MIRPLAN.md §3.

## Bağlam

VM'in `Value` sınıfı (`src/vm/value.hpp`) `int`, `double`, `DecimalValue`,
`std::string`, `Object*` alanlarını aynı struct'ta taşıyan "şişman" bir C++
nesnesi — çalışma zamanında `kind` etiketiyle ayrımlanır. MIR register'ları ise
yalnızca **tek tip 64-bit değer** tutabilir (int64, float, double, long double;
MIR.md: "Registers can contain only one type value"). Bu yüzden JIT'lenmiş kodun
içinde `Value`'nun tamamı bir register'da taşınamaz.

Ek olarak string, VM'de **kutusuz/inline** tutuluyor (ADR-024: immutable
değer-tipi, `Value` içine gömülü, ayrı heap nesnesi değil). JIT sınırında bu
geçerli değil — register bir string'in bütününü taşıyamaz.

## Karar

**İki katmanlı değer temsili:**

1. **Register'da (JIT'lenmiş kodun içinde):** yalnızca skaler, statik türüne
   göre tek register:
   - `Int`/`Bool`/`Byte`/`Date` → `MIR_T_I64` (Date epoch-ms doğrudan taşınır).
   - `Float` → `MIR_T_D`.
   - `String`/`Decimal`/`Ref` (struct/array) → `MIR_T_I64` ama **pointer**
     olarak yorumlanır (heap'teki `Object` ailesinden bir nesneye işaret eder).
   - Register tipi seçimi **derleme zamanı statiktir** — bir slot çalışma
     zamanında tip değiştirmez (ADR-020, tip denetleyici garanti eder). Bu
     bilgi IR'de `IRFunction::slotTypes` olarak taşınır (Dilim 1.5, MIRPLAN §3).

2. **Sınırda (JIT↔VM geçişi, CALLHOST, global slot okuma/yazma):** `MirValue`
   adlı POD, C-uyumlu ABI struct:
   ```c
   typedef struct { uint8_t kind; union { int64_t i; double f; void* obj; } as; } MirValue;
   ```
   `mir_value_abi.hpp` (henüz yok, Dilim 3'te) VM `Value` ↔ `MirValue`
   dönüştürücülerini (`toMir`/`fromMir`) barındırır; string kutulama/kutu-açma
   burada olur.

**String kutulama farkı — bilinçli ve kayıtlı:** JIT'in gördüğü her `String`
değeri heap'e kutulanır (`StringObject : Object`, Dilim 3'te `object.hpp`'ye
eklenecek); VM'de string inline kalır. **Bu iki backend'in iç bellek modelini
farklılaştırır ama kural ihlali DEĞİL:** ADR-032'nin "VM ve JIT bit-bit aynı
çıktı vermek ZORUNDA" kuralı **gözlemlenen davranışa** (stdout, dönüş değerleri)
uygulanır, **iç temsile değil.** İçerik-eşitliği (ADR-023 string istisnası:
`==` içerik) ve immutability (ADR-024) her iki temsilde de korunduğu sürece
gözlemlenen davranış aynıdır — diferansiyel test (#92) bunu doğrular.

**Decimal:** v1'de her zaman kutulu (`MirValue.obj` içinde pointer), aritmetik
runtime call'a gider (ölçek hizalama MIR'e elle yazılmaya değmeyecek kadar
seyrek/kritik-olmayan bir yol — MIRPLAN §3). İnce ayar/inlining ölçüm sonrası
ayrı bir tur.

## Sonuç / Kapsam

- **Bu ADR yalnızca ABI kararını kaydeder**, kodu Dilim 3 (string/decimal/date/
  cast) getirir. Dilim 1.5 (int+float skaler) `MirValue` sınırına henüz
  ihtiyaç duymaz — tüm değerler register-skaler.
- Kutulama farkı `readme`/kullanıcı belgelerinde "iç temsil backend'e göre
  değişebilir, gözlemlenen davranış aynıdır" olarak bir cümleyle anılmalı.
- Açık kalan tek nokta yok — bu karar Dilim 3'ün ön koşulunu (MIRPLAN açık
  soru #1) kapatır.
