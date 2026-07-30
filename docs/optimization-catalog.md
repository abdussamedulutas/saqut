# saQut Optimizasyon Kataloğu

> Her optimizasyonun uygulanabilirliği saQut'un dil özelliklerine, AST/IR
> yapısına ve hedef platforma (VM/JIT) göre değerlendirilmiştir.
>
> Seviyeler:
>   AS = AST seviyesi (mevcut optimizer altyapısı)
>   IR = IR seviyesi (instruction dizisi)
>   VM = VM seviyesi (bytecode interpreter)
>   JIT = JIT codegen seviyesi (MIR)
>
> Etki: tahmini hızlanma (cold code / hot loop bazında)

---

## A. AST Seviyesi (Mevcut Altyapı: `OptimizationPass`)

| # | Optimizasyon | Etki | Zorluk | Açıklama |
|---|-------------|------|--------|----------|
| A01 | **Constant Folding** ✅ | %5-15 | Düşük | `3 + 5 * 2` → `13` (mevcut) |
| A02 | **Dead Code Elimination** ✅ | %2-10 | Düşük | Kullanılmayan değişken/ifade sil (mevcut) |
| A03 | **Constant Propagation** | %5-20 | Düşük | `int x = 5; ... x + 3` → `8` |
| A04 | **Copy Propagation** | %1-5 | Düşük | `int y = x; ... y + 1` → `x + 1` |
| A05 | **Unreachable Code Elimination** | %1-10 | Düşük | `return; ...` sonrası kod sil |
| A06 | **If Simplify / Constant Branch** | %1-5 | Düşük | `if (true) {X}` → `X`; `if (false){}` → sil |
| A07 | **Switch Simplify** | %1-5 | Düşük | Tek kollu switch → direkt dal |
| A08 | **Redundant Assignment Elimination** | %1-3 | Düşük | `x = x;` sil; `int x; x = 5; x = 6;` → `int x = 6;` |
| A09 | **Loop Unswitching** | %5-30 | Orta | Loop invariant condition'ı dışarı al |
| A10 | **Loop Invariant Code Motion (LICM)** | %10-60 | Orta | Sabit hesapları loop dışına çıkar |
| A11 | **Function Inlining** | %10-50 | Yüksek | Küçük fonksiyon çağrılarını inline et |
| A12 | **Tail Call Optimization** | %1-5 | Orta | `return f(x)` → jump |
| A13 | **Expression Simplification** | %1-10 | Düşük | `x * 0` → `0`; `x + 0` → `x`; `x * 1` → `x` |
| A14 | **Strength Reduction (AST)** | %5-20 | Düşük | `x * 2` → `x << 1`; `x * 15` → `(x << 4) - x` |
| A15 | **Null Check Elimination** | %1-5 | Orta | `if (x != null) { use(x) }` → redundant check sil |
| A16 | **Common Subexpression Elim. (CSE)** | %5-25 | Yüksek | Aynı alt-ifadeyi tekrar hesaplama |
| A17 | **Algebraic Simplification** | %1-10 | Düşük | `(a + b) - a` → `b`; `(x / y) * y` → `x` |
| A18 | **Boolean Expression Simplification** | %1-5 | Düşük | `true && x` → `x`; `false || x` → `x` |
| A19 | **Array Length Propagation** | %1-5 | Düşük | `int n = a.length();` → loop'da a.length() yerine n kullan |
| A20 | **Strength Reduction (loop)** | %5-30 | Orta | `i * 5` (loop) → `i5 += 5` (indüksiyon değişkeni) |
| A21 | **Induction Variable Elimination** | %5-20 | Yüksek | Gereksiz indüksiyon değişkenlerini sil |
| A22 | **Loop Unrolling** | %5-50 | Orta | Küçük loop'ları aç (kendi maliyeti var) |
| A23 | **Peephole (AST desen eşleme)** | %1-5 | Düşük | `(a as byte) as int` → `a` (gereksiz cast zinciri) |
| A24 | **Array Bounds Check Elimination** | %5-30 | Yüksek | `a[i]` loop'unda bounds check'i kaldır (güvenlik × hız tradeoff) |

## B. IR Seviyesi (Instruction Dizisi)

| # | Optimizasyon | Etki | Zorluk | Açıklama |
|---|-------------|------|--------|----------|
| B01 | **IR Dead Instruction Elimination** | %2-10 | Düşük | Sonucu kullanılmayan instruction sil |
| B02 | **IR Constant Folding** | %3-15 | Düşük | LOAD_CONST + ADD gibi instruction çiftlerini katla |
| B03 | **IR Copy Propagation** | %1-5 | Düşük | LOAD_SLOT zincirini kısalt |
| B04 | **IR Algebraic Simplification** | %1-10 | Düşük | `ADD(s, 0)` → LOAD_SLOT; `MUL(s, 1)` → LOAD_SLOT |
| B05 | **IR Strength Reduction** | %5-20 | Orta | MUL → SHL; DIV → SHR |
| B06 | **IR CSE** | %5-25 | Yüksek | Aynı IR instruction tekrarını kaldır |
| B07 | **IR Dead Loop Elimination** | %1-10 | Orta | Yan etkisiz loop'u sil |
| B08 | **IR Jump Threading** | %1-5 | Orta | JMP → JMP zincirini kır |
| B09 | **IR Branch Folding** | %1-5 | Düşük | JIF_TRUE(1) → JMP; JIF_FALSE(1) → sil |
| B10 | **IR Instruction Combining** | %1-10 | Orta | LOAD_CONST + ARRAY_NEW(0) → ARRAY_NEW(sabit, 0) |
| B11 | **IR Slot Reuse** | %1-5 | Düşük | Kullanılmayan slot'ları yeniden kullan |
| B12 | **IR Function Argument Cleanup** | %1-3 | Düşük | Kullanılmayan parametreleri işaretle |

## C. VM Seviyesi (Interpreter)

| # | Optimizasyon | Etki | Zorluk | Açıklama |
|---|-------------|------|--------|----------|
| C01 | **Instruction Cache Locality** | %5-15 | Orta | Sık kullanılan opcode'ları branch predictor dostu sırala |
| C02 | **Fast Path Inline** | %10-30 | Orta | Sık opcode'lar (LOAD_CONST, ADD, JMP) için özel hızlı yol |
| C03 | **Threaded Code Interpreter** | %20-40 | Yüksek | Switch yerine computed goto (label pointers) |
| C04 | **GC Threshold Tuning** | %5-20 | Düşük | Dinamik GC eşiği (heap boyutuna göre) |
| C05 | **Stack Frame Pre-allocation** | %1-5 | Düşük | callFrame vector growth azalt |
| C06 | **Direct Threading** | %30-60 | Yüksek | Her instruction dispatch'inde switch'ten kaçın |
| C07 | **Inline Caching** | %10-30 | Yüksek | Aynı builtin metodu tekrar çağırırken dispatch bypass |

## D. JIT Seviyesi (MIR Backend)

| # | Optimizasyon | Etki | Zorluk | Açıklama |
|---|-------------|------|--------|----------|
| D01 | **MIR Register Allocation** | %10-30 | Orta | (MIR'in kendi rega'sı) — ek iş gerekmez |
| D02 | **MIR Instruction Selection** | %5-15 | Düşük | (MIR'in kendi pattern matching'i) |
| D03 | **JIT Inline Caching** | %10-40 | Yüksek | CALLHOST dispatch'ini native call'a çevir |
| D04 | **JIT Shadow Stack Elimination** | %5-20 | Yüksek | VM frame'ini native stack'e taşı |
| D05 | **JIT Deoptimization** | %1-10 | Çok yüksek | JIT'ten VM'e güvenli dönüş (speculative optimization için) |
| D06 | **JIT On-Stack Replacement** | %5-30 | Çok yüksek | Uzun loop'u JIT derlemesi biter bitmez değiştir |

## E. Bellek / Runtime

| # | Optimizasyon | Etki | Zorluk | Açıklama |
|---|-------------|------|--------|----------|
| E01 | **Small String Optimization** | %5-20 | Düşük | Kısa string'leri heap yerine Value içinde tut |
| E02 | **String Builder** | %10-50 | Orta | String concat'i buffer push'a çevir (emir kuralı) |
| E03 | **GC Generational** | %20-60 | Çok yüksek | Genç nesne havuzu, yaşlıları az tara |
| E04 | **GC Parallel Sweep** | %10-30 | Yüksek | Sweep fazını thread'e yay |
| E05 | **Array Pre-populate** | %1-5 | Düşük | `ARRAY_NEW` + `ARRAY_SET` zincirini tek alloc'a |
| E06 | **Struct Field Packing** | %5-15 | Orta | alignment doldurmayı en aza indir |
| E07 | **Lazy Initialization** | %1-10 | Düşük | Kullanılmayan global'i başlatma |
| E08 | **Int/Byte Array Buffer Coalescing** | %1-3 | Düşük | Push-only pattern → tek resize + index write |

---

## Öncelik Sırası (Etki × Uygulanabilirlik)

```
YÜKSEK ÖNCELİK (Hemen yapılabilir, büyük etki):
  A10 LICM          — Loop invariant code motion
  A09 Loop unswitching
  A14 Strength reduction (AST)
  E02 String builder
  A03 Constant propagation

ORTA ÖNCELİK (Yapılabilir, ölçülü etki):
  A11 Inlining       — Küçük fonksiyonlar
  A16 CSE            — Common subexpression elimination
  A13 Expression simplification
  A20 Loop strength reduction
  C04 GC threshold tuning

DÜŞÜK ÖNCELİK (Karmaşık veya büyük yatırım):
  C06 Direct threading
  D05 Deoptimization
  E03 Generational GC
  A24 Array bounds check elimination
```

---

## Mevcut Durum

```
AST (2 pass)  →  IR (0 pass)  →  VM (0 optimization)  →  JIT (MIR built-in)
  ├ ConstantFolding ✅            Boş                         MIR kendi yapar
  └ DeadCodeElim   ✅
```

Boşluk: IR seviyesinde hiç optimizasyon yok. Oysa saQut'un IR'i 3-adresli ve
SSA benzeri — CSE, copy propagation, dead instruction elimination için
çok uygun. Bir IR pass ekosistemi, AS seviyesinden daha güçlü sonuç verir.
