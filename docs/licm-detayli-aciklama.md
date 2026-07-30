# LICM — Loop Invariant Code Motion (Detaylı Açıklama)

> **Ne işe yarar?** Loop'un her iterasyonunda aynı sonucu veren
> hesaplamaları loop'un önüne (pre-header) çıkarır.
>
> Örnek:
> ```c
> while (i < N) {
>     int x = a * b + c;      // ← a,b,c loop içinde değişmiyor!
>     arr[i] = x * data[i];
> }
> ```
> ↓ LICM
> ```c
> int x = a * b + c;           // ← bir kere hesapla
> while (i < N) {
>     arr[i] = x * data[i];
> }
> ```

---

## 1. Sağlaması Gereken Koşullar

Bir ifadenin loop invariant sayılması için **3 koşul**:

1. **Tanım erişimi** — İfadenin tüm operandları loop dışında tanımlanmış
   ve loop içinde **hiçbir yerde değiştirilmiyor** olmalı.
2. **Yan etki yok** — İfade okuma amaçlı olmalı (print, push, alloc, throw
   gibi yan etkileri olmamalı).
3. **Kontrol akışından bağımsız** — İfade loop'un her yürütülüşünde aynı
   değeri vermeli (condition'a bağlı değil).

## 2. saQut AST'inde LICM Uygulaması

### Adım adım algoritma:

```
Phase 1: Loop tespiti
  AST'yi tara → WhileStatement düğümlerini bul

Phase 2: Invariant tespiti (her statement için)
  Statement'in kullandığı tüm değişkenleri listele
  Loop içinde bu değişkenlerin hiçbiri atanmıyorsa → INVARIANT
  (Recursive: children'ı da kontrol et)

Phase 3: Loop pre-header'a taşı
  Invariant statement'ı loop'un hemen öncesine kopyala
  Loop gövdesinden sil
```

### Kod taslağı:

```cpp
class LoopInvariantCodeMotionPass : public OptimizationPass {
    // Her loop'u işle
    bool run(ASTNode* root, SymbolTable*) override {
        bool changed = false;
        visitLoops(root, [&](WhileStatementNode* loop) {
            if (hoistInvariants(loop)) changed = true;
        });
        return changed;
    }

    bool hoistInvariants(WhileStatementNode* loop) {
        // 1. Loop condition'ının kullandığı değişkenleri bul
        auto loopVars = collectWrittenVars(loop->body);

        // 2. Loop body'sindeki her statement için kontrol et
        std::vector<ASTNode*> invariants;
        for (auto* stmt : loop->body->children) {
            if (isInvariant(stmt, loopVars)) {
                invariants.push_back(stmt);
            }
        }

        // 3. Invariant'ları loop öncesine taşı
        if (invariants.empty()) return false;
        for (auto* inv : invariants) {
            // loop->parent->children içinde, loop'dan önceye ekle
            insertBefore(loop, inv);
            remove(loop->body, inv);
        }
        return true;
    }

    bool isInvariant(ASTNode* node, const VarSet& writtenInLoop) {
        // VariableDecl: initExpr invariant mı? → evetse taşı
        // ExpressionStatement: tüm değişkenleri writtenInLoop'da yok mu? → taşı
        // Return/Break/Continue: ASLA taşıma
        // If/While: children'ı kontrol et
        // Builtin call (push/print vb.): yan etkili → taşıma
        // CALL/CALLHOST: yan etkili olabilir → GÜVENLİ taşıma (varsayılan: hayır)
    }
};
```

### Kısıtlar / Zorluklar:

| Sorun | Çözüm |
|-------|-------|
| **Döngü condition'ı da invariant olabilir** | Condition invariant ⇒ infinite loop ⇒ **taşıma** (sonsuz loop'u kırar) |
| **Function call yan etkili olabilir** | Safe: pure fonksiyon (çıktısı yalnızca girdiye bağlı). Şu an tüm fonksiyonlar impure kabul edilir. `[pure]` attribute eklenebilir. |
| **Array/struct mutation** | `a[i] = x` → array referansı değişmez ama ELEMAN değişir. `arr.length()` invariant olabilir ama `arr[i]` olamaz (çünkü i indis olabilir). |
| **Nested loop** | İç loop'un invariant'ı dış loop'a taşınabilir. Algoritma recursive olmalı: önce en iç loop'u işle, sonra dışarı çık. |

## 3. Performans Beklentisi

Benchmark sonuçları (tests/bench/c_loop_invariant_bad.sqt):

```
KOTU (icerde):  0.035s    ← a*b+c her iterasyonda
IYI  (disarda): 0.014s    ← bir kere hesapla
Hizlanma:       2.5x (%60)
```

LICM bu farkı **otomatik** kapatır. Kullanıcı hiçbir şey yapmaz.

### Daha agresif senaryo (nested loop):

```c
while (i < 1000) {
    int c = a * b;           // ← LICM bunu disari alir
    while (j < 1000) {
        arr[i*1000+j] = c * data[j];  // c sabit
    }
}
```

Bu örnekte (a×b) 1M kez değil, 1 kez hesaplanır. **1000×** hızlanma
teorik olarak mümkün (pratikte loop overhead + diğer faktörler).

## 4. Diğer Derleyicilerde LICM

| Derleyici | Seviye | Implementasyon |
|-----------|--------|---------------|
| **GCC** | GIMPLE (IR) | `tree-ssa-loop-im.c` — SSA form + dominance |
| **LLVM** | LLVM IR | `LICM.cpp` — LoopPass, AliasAnalysis |
| **V8 (TurboFan)** | Sea of Nodes | Loop peeling + LICM combined |
| **saQut (öneri)** | **AST** | Yukarıdaki gibi, top-down traversal |

### Neden AST'te LICM?

saQut'un şu anki optimizer altyapısı AST seviyesinde. IR seviyesinde LICM
daha güçlüdür (3-adresli kod + SSA), ama AST'te de:

- Değişken kapsamı net (writtenVars kümesi)
- Kontrol akışı basit (while dışında loop yok)
- Fonksiyon çağrıları az (yan etki analizi basit)

İleride IR optimizer eklenirse LICM oraya da taşınabilir. İkisini birden
tutmak anlamsız — bir seviyede yapmak yeterli.

## 5. Uygulama Planı

```
Adım 1: Loop tespiti + writtenVars toplama (2 saat)
Adım 2: isInvariant kontrolü (2 saat)
Adım 3: Taşıma / silme işlemi (2 saat)
Adım 4: Nested loop + edge cases (2 saat)
Adım 5: Test + benchmark doğrulama (2 saat)
        → tests/bench/08_licm_verify.sqt
```

Toplam: **~1 iş günü** (kesintisiz)

## 6. Açık Sorular

1. **Guarded invariant'lar**: `if (cond) { int x = a*b; }` — x yalnızca
   cond=true iken tanımlı. LICM bunu loop öncesine taşıyamaz (cond
   bilinmiyor). Çözüm: loop invariant'ı condition'sız hesapla, sadece
   cond altında kullan. Guarded LICM daha karmaşık.

2. **Pure function annotation**: `[pure] fn compute(x) { return x*x; }`
   — LICM bu çağrıyı invariant sayabilir. Şu an yok, ek planlanmadı.

3. **Loop versioning**: LICM ile birlikte loop'un 2 versiyonunu üret
   (invariant taşınmış / taşınmamış) ve runtime'da seç. Çok karmaşık,
   plan dışı.
