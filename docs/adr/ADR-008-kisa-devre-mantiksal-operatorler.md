# ADR-008: && ve || kısa devre değerlendirmesi

## Durum
Kabul edildi — **2026-08-28'de revize edildi** (bkz. "Revizyon: sonuç tipi").
Kısa devre kararı DEĞİŞMEDİ; değişen, operatörün ürettiği DEĞERDİR.

## Bağlam
Mevcut kodda `&&` ve `||` çalışma anında bozuk: IR üretici bu operatörler için
`case` içermiyor, `default` koluna düşüp `LOAD_CONST 0` üretiyor — yani değişken
operandlarda her zaman `false` dönüyor. Yalnızca her iki operand sabit olduğunda
sabit katlama doğru sonucu veriyor. (Kanıt: `ir_generator.cpp:395-413`,
`constant_folding.hpp:107-112`; davranış referansı bölüm B.)

## Karar
`&&` ve `||` **KISA DEVRE** değerlendirilir:

- `a && b`: `a` false ise `b` **HİÇ değerlendirilmez**, sonuç `false`.
- `a || b`: `a` true ise `b` **HİÇ değerlendirilmez**, sonuç `true`.

Bu, bu operatörlerin sıradan ikili işlem (iki tarafı hesapla sonra birleştir)
**DEĞİL**, bir dallanma olarak üretilmesi gerektiği anlamına gelir.

IR şeması:

```
a && b:
  slot_a  = [a değerlendir]
  result  = freshSlot()
  LOAD_CONST result, 0        ; varsayılan: false
  JIF_FALSE slot_a → DONE     ; a false? b'yi atla, result=0 kalsın
  slot_b  = [b değerlendir]
  LOAD_SLOT result, slot_b    ; result = b'nin değeri
DONE:

a || b:
  slot_a  = [a değerlendir]
  result  = freshSlot()
  LOAD_CONST result, 1        ; varsayılan: true
  JIF_TRUE  slot_a → DONE     ; a true? b'yi atla, result=1 kalsın
  slot_b  = [b değerlendir]
  LOAD_SLOT result, slot_b    ; result = b'nin değeri
DONE:
```

`||` için `JIF_TRUE` opcode'u gerekir. `JIF_FALSE`'un simetriği olarak
`instruction.hpp`'e ve `interpreter.cpp`'e eklendi. `do-while` döngüsünün
mevcut `EQUAL_EQUAL(cond, 1)` geçici çözümü bu opcode'dan faydalanabilir
(ayrı düzeltme — bu ADR kapsamı dışı).

## Gerekçe
- C-ailesi dillerin (C, Go, Java, JS, C#) tamamı kısa devre yapar; hedef kitle
  bunu bekler.
- Performans artısı: gereksiz sağ-taraf değerlendirmesi atlanır; kısa devre
  OLMAYAN versiyon her iki tarafı da her zaman hesaplayacağı için daha yavaştır.
- Niş ile uyumlu: derleyici "şu çağrı şu durumda atlandı" diye gösterebilir.

## Sonuçlar
- Tek dezavantaj: mantıksal operatörün sağındaki yan etki koşullu çalışır.
  Bu kabul edilir; yan etkiyi koşula gömmek zaten kötü kalıptır.
- Sabit katlama yolu (`constant_folding.hpp`) zaten doğru çalışıyordu ve
  değiştirilmedi; bu düzeltme yalnızca değişken-operand (IR üretim) yolunu etkiler.


---

## Revizyon (2026-08-28): sonuç tipi 1/0'dır — operandın değeri değil

### Ne değişti

Bu ADR'nin ilk hali iki ayrı sözleşmeyi tek başlık altında topluyordu ve
ikincisini örtük bırakmıştı:

1. **Kısa devre (değerlendirme):** `a` sonucu belirliyorsa `b` hiç
   çalıştırılmaz. **DEĞİŞMEDİ, aynen geçerlidir.**
2. **Sonuç (değer):** ilk şemadaki `LOAD_SLOT result, slot_b` adımı,
   sonucu `b`'nin KENDİSİ yapıyordu (`5 && 3` → 3, `0 || 33` → 33) —
   Python/JS tarzı. **Bu değişti: sonuç artık 1/0'dır** (C/Java/Go modeli).

Ürün sahibi kararı: `&&` ve `||` mantıksal operatörlerdir; sonuçları bir
doğruluk değeridir. `5 && 2` sayısal bir birleştirme değil bir bool
ifadesidir — sayısal bir sonuç isteniyorsa `5 && 2 == 0` gibi açıkça
yazılır. İkisi yüzeysel olarak benzer görünür ama semantikleri farklıdır ve
statik tipli bir dilde operandı döndürmek tip belirsizliği üretir.

### Revize edilmiş IR şeması

```
a && b:
  slot_a = [a değerlendir]
  result = freshSlot()
  LOAD_CONST result, 0        ; varsayılan: false
  JIF_FALSE slot_a → DONE     ; a falsy → b ATLANIR (kısa devre)
  slot_b = [b değerlendir]
  JIF_FALSE slot_b → DONE     ; b falsy → result 0 kalır
  LOAD_CONST result, 1        ; ikisi de truthy → 1
DONE:

a || b:
  slot_a = [a değerlendir]
  result = freshSlot()
  LOAD_CONST result, 1        ; varsayılan: true
  JIF_TRUE  slot_a → DONE     ; a truthy → b ATLANIR (kısa devre)
  slot_b = [b değerlendir]
  JIF_TRUE  slot_b → DONE     ; b truthy → result 1 kalır
  LOAD_CONST result, 0        ; ikisi de falsy → 0
DONE:
```

İkinci `JIF`'in tek işlevi 1/0'a indirgemedir. Yeni opcode gerekmez.

### Neden acil: optimizasyon sonucu değiştiriyordu

Bu revizyon bir ergonomi tercihi değil, bir **doğruluk hatasının**
düzeltilmesidir. İlk halin son satırı şöyle diyordu:

> "Sabit katlama yolu (`constant_folding.hpp`) zaten doğru çalışıyordu ve
> değiştirilmedi; bu düzeltme yalnızca değişken-operand (IR üretim) yolunu
> etkiler."

Sorun tam buydu: sabit katlama `(l && r) ? 1 : 0` (yani 1/0) üretirken IR
üretimi operandın değerini üretiyordu. İki yol o anda **sessizce ayrıştı**:

| ifade | `saqut run` | `saqut run --optimized` |
|---|---|---|
| `5 && 3` | 3 | 1 |
| `0 \|\| 33` | 33 | 1 |
| `-1000000 && 7` | 7 | 1 |

Yani aynı program, optimizasyon bayrağına göre farklı sonuç veriyordu —
ADR-038'in "aynı program, backend/optimizasyon ne olursa olsun aynı sonuç"
sözleşmesinin doğrudan ihlali. Production'da `--optimized` açık
çalışacağından bu, dağıtılan davranış ile geliştirmede görülen davranışın
ayrışması demekti.

Revizyondan sonra dört mod da aynı sonucu veriyor:
`run`, `run --optimized`, `run --jit`, `run --jit --optimized`.

### Kısa devre neden korundu

Kaldırılması ayrı bir kayıptır: `x != null && x.alan > 0` kalıbını kırar
(sağ taraf `x` null iken de değerlendirilirdi) ve TypeChecker'ın `&&` sağ
tarafında yaptığı null daraltmasını (`type_checker.cpp`) anlamsız kılardı.
Yan etkili sağ tarafta gereksiz iş de yaptırırdı.

### Kanıt

- Regresyon fixture'ı: `tests/golden/opt/mantiksal_semantik.sqt` — her iki
  sözleşmeyi de sınar (1/0 sonucu + yan etkili çağrıyla kısa devre gözlemi).
- Kalıcı gate: `tests/run.sh` → "optimizasyon sonucu degistirmiyor" —
  her golden fixture'ı `--optimized` ile ve onsuz, iki backend'de karşılaştırır
  (290 kombinasyon). Ayrışmayı bu gate yakaladı; kalıcı korumadır.
- Gate'in ve fixture'ın gerçekten yakaladığı, düzeltme geçici geri alınarak
  doğrulandı (`31017...` vs `11011...`).
- `bash tests/run.sh` — hepsi geçti (golden 128, diferansiyel VM≡JIT 110).
