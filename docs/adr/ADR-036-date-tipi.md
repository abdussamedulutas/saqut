# ADR-036 — date Tipi ve Modülü (UTC epoch-ms değer tipi)

İlgili: #7, #76, #88, ADR-028 (decimal emsali), ADR-035 (capability modeli).

## Bağlam

saQut'ta native tarih/saat tipi yoktu (#7). decimal'de (ADR-028) kurulan
model izlenerek — yeni primitive değer tipi + bağlı fonksiyon ailesi — date
eklendi.

## Karar

- **Tip:** `PrimitiveKind::Date` + `Type::Date()`. Değer tipi (ADR-020),
  immutable. İç temsil: **UTC epoch-milisaniye**, `Value::int64Value`
  (`long long`) alanında taşınır — `Value`'nun mevcut `int` alanı (32-bit)
  bunun için yetersizdi, bu yüzden yeni bir alan eklendi (decimal'in kendi
  `DecimalValue` alanını aldığı gibi).
- **Timezone (kilitli):** v1'de IANA tzdata YOK — sıfır harici bağımlılık
  kısıtını (ADR-032) deler. Yalnızca UTC. Tam timezone desteği 1.x sonrasına
  açık madde.
- **Capability ayrımı:** yalnızca `date::now()` `sys` capability'si ister
  (ADR-035). Geri kalan tüm fonksiyonlar (parse/format/fromEpochMillis/
  toEpochMillis/addX/year..second/diffMillis) saf hesaptır, capability'siz.
- **Operatörler:** `==`,`!=`,`<`,`<=`,`>`,`>=` int64 karşılaştırması olarak
  çalışır. **Aritmetik YOK** (`date + int` gibi) — yalnızca açık `addDays`/
  `addHours`/`addMinutes`/`addSeconds` (birim belirsizliği önlenir).
  `Type::isDate()` bilerek `isNumeric()`'e dahil edilmedi; TypeChecker'ın
  karşılaştırma dalına `isDate() && isDate()` ayrı bir kolla eklendi —
  aritmetik/bitwise dalına hiç girmez.
- **IR/VM:** date **mevcut karşılaştırma opcode'larını paylaşır**
  (`LESS`/`LESS_EQUAL`/`GREATER`/`GREATER_EQUAL`/`EQUAL_EQUAL`/`NOT_EQUAL`) —
  bunlar zaten çalışma zamanında `ValueKind`'e göre dallanıyordu (Decimal/
  Float/Int için de aynı desen); `Date` koluna `int64Value` karşılaştırması
  eklemek yeterliydi. Yeni `DATE_*` opcode'u GEREKMEDİ (dar-bel ilkesi:
  önce var olan mekanizmaya desugar).
- **Takvim matematiği:** year/month/day/hour/minute/second ayrıştırması ve
  addDays'in ay/yıl sınır aşımı (artık yıl dahil) `src/ffi/date_calc.hpp`'de,
  Howard Hinnant'ın kamu malı (public domain) `civil_from_days`/
  `days_from_civil` algoritmasının doğrudan uyarlamasıyla — host tarafında
  saf C++, harici kütüphane yok.
- **`fromEpochMillis`/`toEpochMillis` bilinen v1 kısıtı:** saQut'ta 64-bit
  tamsayı tipi YOK; bu iki fonksiyon `int` (32-bit) taşır. Günümüz
  epoch-ms değerleri int32'yi aşar, yani bu iki fonksiyon **yalnızca test/
  debug amaçlı küçük ofset değerleri için güvenlidir** — `date::now()` ile
  üretilen gerçek bir date'i `toEpochMillis` ile int'e çevirmek taşar.
  `date` DEĞERİNİN kendisi (`int64Value`) bu sınırdan etkilenmez — yalnızca
  bu iki dönüşüm fonksiyonu int32 sınırına tabidir. year/month/day/addX/
  diffMillis (makul aralıklarda) etkilenmez. 64-bit tamsayı tipi eklenirse
  bu iki fonksiyon o tipe geçirilir.
- **`diffMillis` da `int` döner** — aynı sebeple yalnızca ~24 günlük farklara
  kadar güvenli (`int32_max / 86400000 ≈ 24.8 gün`). Daha büyük farklar için
  `year`/`month`/`day` üzerinden hesaplama önerilir.

## Sonuç

`tests/golden/date/` — parse/format gidiş-dönüşü, artık yıl + yıl sınırı
aşan `addDays`, karşılaştırma operatörleri, accessor'lar + `diffMillis`,
capability enforcement (`now()` onsuz derleme hatası, izinle çalışır) test
edildi.
