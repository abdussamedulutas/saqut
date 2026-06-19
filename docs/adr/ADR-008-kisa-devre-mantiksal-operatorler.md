# ADR-008: && ve || kısa devre değerlendirmesi

## Durum
Kabul edildi.

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
