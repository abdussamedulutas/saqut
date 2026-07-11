# ADR-029 — Nested Struct Tahsis Kuralı

**Durum:** Kabul  
**Tarih:** 2026-06-25

## Karar

Struct-tipli alana sahip bir struct değişkeni (`Rect r;`) bildirildiğinde, iç struct
alanları VarDecl anında özyinelemeli olarak tahsis edilir. Bu tahsis IR seviyesinde
gerçekleşir: `emitStructNew` + `FIELD_SET` zinciri üretilir; iç struct'ın da struct-tipli
alanları varsa aynı işlem özyinelemeli uygulanır.

```
STRUCT_NEW  s0 = struct<Rect>[2]
STRUCT_NEW  s1 = struct<Point>[2]   ← topLeft için
FIELD_SET   s0.0 = s1
STRUCT_NEW  s2 = struct<Point>[2]   ← bottomRight için
FIELD_SET   s0.1 = s2
```

## Referans paylaşımı (ADR-020 tutarlılığı)

`Rect b = a;` sonrası b ve a AYNI Rect nesnesini paylaşır; `b.topLeft` ve `a.topLeft`
da AYNI Point nesnesini paylaşır. `b.topLeft.x = 99` → `a.topLeft.x` de 99 olur.
Bu ADR-020'nin referans semantiği kuralının nested duruma doğal uzantısıdır.

## Reddedilen alternatifler

**Tembel tahsis (first-access):** Her field erişiminde "slot başlatıldı mı?" kontrolü
interpreter'ı karmaşıklaştırır ve "frontend kesin çözer" felsefesiyle çelişir.

**Interpreter seviyesinde tahsis:** STRUCT_NEW opcode'u işlenirken layout bilgisine
bakarak iç struct oluşturmak runtime'a compile-time bilgisi taşımayı gerektirir.
IR'de görünmez, projenin cam kutu vizyonuyla çelişir.

## Uygulama

`src/ir/ir_generator.cpp`: `IRGenerator::initNestedStructFields()` private metodu,
`VarDecl` bloğunda `emitStructNew()` çağrısı sonrasında çalışır.
