# saQut Kullanıcı Kılavuzu

**saQut** — programlanabilir ve incelenebilir bir derleyici.
Bu kılavuz, dile sıfırdan başlamak isteyenler için hazırlandı.
Her sayfa birbirine bağlantı verir, istediğin yerden başlayabilirsin.

---

## Başlarken

| Sayfa | Açıklama |
|-------|----------|
| [Başlarken](getting-started.md) | Derleyiciyi derleme, ilk program, print() fonksiyonu |
| [CLI Komutları](cli-commands.md) | `run`, `tokens`, `ast`, `symbols`, `check`, `ir` — tüm komutlar |

---

## Dil Temelleri

| Sayfa | Açıklama |
|-------|----------|
| [Değişkenler ve Veri Tipleri](variables-types.md) | `int`, `float`, `bool`, `string`, değer/referans semantiği |
| [Literal'lar](literals.md) | Sayılar (`0xFF`, `0b1010`, `0777`, `1e-5`), string kaçışları, `true`/`false`/`null` |
| [Operatörler](operators.md) | Öncelik tablosu (18 seviye), Pratt parser, `-2 + -5` çözümlemesi, short-circuit |

---

## Kontrol Akışı

| Sayfa | Açıklama |
|-------|----------|
| [Kontrol Akışı](control-flow.md) | `if`/`else`, `while`, `do`/`while`, `for`, `break`/`continue` (`switch` henüz yok) |

---

## Veri Yapıları

| Sayfa | Açıklama |
|-------|----------|
| [Fonksiyonlar](functions.md) | Tanım, parametreler, `return`, recursion (fibonacci), `print()` |
| [Struct'lar](structs.md) | `struct` tanımı, alan erişimi, referans semantiği |
| [Diziler (Array)](arrays.md) | `int[]`, indeksleme, referans semantiği |
| [String'ler](strings.md) | Immutable, `+` ile birleştirme, `==` içerik karşılaştırması |

---

## Değişken Kapsamı

| Sayfa | Açıklama |
|-------|----------|
| [Global Değişkenler](globals.md) | Fonksiyon dışında tanımlanan değişkenler |

---

## Operatörler

| Sayfa | Açıklama |
|-------|----------|
| [Bileşik Atama Operatörleri](compound-assignment.md) | `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `<<=`, `>>=` |

---

## İleri Düzey

| Sayfa | Açıklama |
|-------|----------|
| [Optimizasyon](optimization.md) | Constant folding, DCE, `--optimized` bayrağı |
| [Hata Yönetimi (Try/Catch/Throw)](error-handling.md) | Unchecked hatalar, runtime hataları (deneysel) |
| [Derleyici Pipeline'ı](pipeline.md) | Token → AST → Sembol → Tip → Optimizasyon → IR → VM |

---

## Örnek Programlar

`examples/` ve `tests/golden/` dizinlerinde çalışan programları bulabilirsin:

| Örnek | Dosya |
|-------|-------|
| Merhaba Dünya | `examples/merhaba.sqt` |
| Fibonacci (recursive + iterative) | `examples/fibonacci.sqt` |
| Operatör öncelik | `tests/golden/arithmetic/precedence.sqt` |
| Döngüler (for/while/do) | `tests/golden/loops/basic.sqt` |
| İç içe break | `tests/golden/loops/nested_break.sqt` |
| Kısa devre mantıksal | `tests/golden/logic/short_circuit.sqt` |
| String birleştirme | `tests/golden/string/concat.sqt` |
| Float aritmetik | `tests/golden/float/basic.sqt` |
| Struct alan erişimi | `tests/golden/struct/basic.sqt` |
| Array referans semantiği | `tests/golden/array/ref_semantics.sqt` |
| Global değişkenler | `tests/golden/global/basic.sqt` |
| Bitsel operatörler | `tests/golden/bitwise/basic.sqt` |
| Constant folding | `tests/golden/opt/folding.sqt` |
| Dead code elimination | `tests/golden/opt/dce.sqt` |
| Tüm bileşik atamalar | `tests/golden/arithmetic/compound_mod.sqt` |

---

*saQut — her aşaması incelenebilir derleyici.*
