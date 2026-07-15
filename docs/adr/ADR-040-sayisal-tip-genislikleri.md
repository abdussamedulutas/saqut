# ADR-040 — Sayısal Tip Genişlikleri Sözleşmesi (#113)

İlgili: #113 (bulgu + karar), ADR-032 (VM referans + JIT diferansiyel), ADR-037
(JIT Value ABI), ADR-038 (determinizm + sürüm uyumluluğu), ADR-039 (`Instruction::
valueType`), ADR-026 (`as` / cast, byte).

## Bağlam

Sayısal tip genişlikleri katmanlar arası **tutarsızdı** (#113): tip sistemi bir
genişlik bildiriyor, runtime (Value/VM) başka, JIT (MIR) üçüncü bir genişlik
işliyordu. En ciddi sonuç kanıtlanmış bir **VM ≢ JIT** ayrışmasıydı:

```
int main() { print(2147483647 + 1); return 0; }
saqut run          =>  -2147483648   (VM: C++ int, 32-bit wrap)
saqut run --jit    =>   2147483648   (JIT: MIR_ADD, 64-bit)
```

`int` genişliği VM'de 32, JIT'te 64 → ADR-032/038'in "gözlemlenen davranış birebir"
sözleşmesi ihlali. Diğer bulgular: `float`/`double` runtime'da çakışık (ikisi de
C++ `double`, E003 uyardığı veri kaybı hiç gerçekleşmiyor); `byte` fiziksel 8-bit
değil (0-255 aralık-denetimli int); genel amaçlı 64-bit tamsayı tipi yok.

Kök neden: genişlikler runtime'da host C++ tiplerine sabitlenmiş, iki backend
bunu **bağımsız** seçmiş, tip sistemi bildirdiğini storage/işlemede enforce
etmemişti.

## Karar

Genişlikler bir **sözleşme** olarak sabitlenir; her katman (tip sistemi, Value
depolama, VM, JIT) bunu onurlandırır.

1. **`int` = 32-bit signed.** Taşma politikası: **tanımlı 2's-complement wrap**
   (checked/throw değil). C++ signed overflow UB olduğundan VM aritmetiği uint32
   üzerinden yapılır; `INT_MIN / -1` ve `INT_MIN % -1` donanımda tuzak (x86 #DE)
   olduğundan iki backend'de de elle 2's-complement sonucu (`INT_MIN`, `0`)
   verilir. JIT `S`-suffix MIR op'ları (ADDS/SUBS/MULS/DIVS/MODS/LSHS/RSHS) +
   her sonuçta `EXT32` ile register'ı normalize tutar (S-op sonucunun üst yarısı
   MIR'de tanımsızdır). Bitwise (AND/OR/XOR/NOT) ve karşılaştırmalar sign-extend
   invariant'ını koruduğundan 64-bit op olarak kalır.

2. **`longint` = 64-bit signed** (genel amaçlı 64-bit tamsayı) **eklenecek**.
   `int` ile gizli dönüşüm yok; açık `as`. JIT `MIR_T_I64` + 64-bit op (EXT32'siz).
   *(Faz 2 — henüz uygulanmadı.)*

3. **`byte`** — 0-255. Aritmetik semantiği (gerçek 8-bit wrap mı, int'e terfi eden
   aralık-denetimli değer mi) **Faz 4'te son karara bağlanacak**. Not: `byte`'ın
   int içinde taşınması serileştirmeye dezavantaj DEĞİL — serializer değeri
   (0-255) okur, in-memory slot genişliğini değil; `byte[]` packed bayt bloğu
   olarak yazılabilir (ADR-038: iç temsil serbest). Karar ekseni serialization
   değil, "byte sarmalı makine-tipi mi, aralık-kısıtlı sayı mı" kimliği.

4. **`float` = 32-bit (IEEE single), `double` = 64-bit (IEEE double)** — ikisi de
   **gerçek** genişlikte olacak (şu an ikisi de 64). E003 `double → float` veri
   kaybı gerçekleşir. VM'de ayrı temsil, JIT'te `MIR_T_F` + `MIR_T_D`.
   *(Faz 3 — henüz uygulanmadı.)*

## Uygulama fazları

| Faz | Kapsam | Durum |
|---|---|---|
| 1 | `int` 32-bit iki backend'de hizalı + wrap tanımlı | ✅ uygulandı |
| 2 | `longint` (64-bit) tipi | ⏳ |
| 3 | `float32` / `double64` gerçek ayrım | ⏳ |
| 4 | `byte` kararı + docs (saqut.com genişlikler) + #113 kapat | ⏳ |

**Faz 1 dokunulan yerler:** `src/vm/interpreter.cpp` (wrapAddI32/…/wrapShrI32
helper'ları + ADD/SUB/MUL/DIV/MOD/SHL/SHR), `src/mir/mir_backend.cpp` (S-op +
EXT32; DIV/MOD'a INT_MIN/-1 tuzak dalları), `tests/golden/arithmetic/overflow.sqt`.

## Sonuç

Kanıtlanmış divergence kapandı: taşma senaryolarında (2^31, INT_MIN/-1, `1<<31`,
çarpım taşması) VM ≡ JIT birebir. Determinizm 1.0 öncesi düzeltildi — bu davranış
(int wrap, ileride float precision) gözlemlenebilir olduğundan 1.0'dan sonra
donacak (ADR-038); bu yüzden şimdi bağlanması kritik. Kalan genişlik kararları
(longint, float32/double64, byte) yukarıdaki fazlarda uygulanacak.
