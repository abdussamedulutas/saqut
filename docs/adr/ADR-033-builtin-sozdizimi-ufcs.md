# ADR-033 — Builtin Sözdizimi Reformu: UFCS Nokta Çağrısı + Kategori Ad Alanları

**Durum:** Uygulandı (issue #85, 2026-07-13)  
**Tarih:** 2026-07-13

## Bağlam

Eski builtin sözdizimi `ElementTipi::method(receiver, args)` (ör. `int::push(arr, 12)`)
dört sorun taşıyordu: sol taraf bilgi taşımıyordu (derleyici receiver tipini zaten
biliyor), `int[]`→`Point[]` refactoring'inde her çağrı noktası değişiyordu, zihinsel
model tutarsızdı (array'de sol taraf *element* tipi, string'de *değerin kendisi*) ve
`::` sonrası LSP completion filtrelemesi tekrarlanan düzeltmeler gerektiriyordu.
Stdlib büyümeden (v0.7.0 fs/date dalgası) değişmesi gerekiyordu.

## Karar

**1. UFCS nokta çağrısı (birincil):** `arr.push(12)`, `s.upper()`, `p.toJson()`.

- Bu OOP **değildir**: `a.f(b)` yalnızca `f(a, b)` şekeridir. Metod tablosu, vtable,
  dinamik dispatch yok. Parser `expr.name(` gördüğünde `ScopeCallNode{dotCall=true}`
  üretir ve receiver'ı `arguments[0]` yapar; TypeChecker kategoriyi receiver
  TİPİNDEN çözer; IR eski `::` çağrısıyla **bire bir aynı** CALLHOST'a düşer
  (diff ile doğrulandı). Dil kimliği (prosedürel, OOP yok) korunur.
- **Çakışma kuralı:** struct alanı builtin'i gölgeler — receiver struct ve metod
  adı bir alansa builtin'e bakılmaz; alanlar çağrılabilir olmadığından bu açık
  bir derleme hatasıdır (`'length' is a field of struct 'Kutu' and is not callable`).

**2. `::` ad alanına döner (ikincil):** sol taraf sabit kategori adı olur:
`array::push(arr, 12)`, `string::upper(s)`, `struct::toJson(p)`. Böylece `::` =
ad alanı erişimi (ileriki `fs::readFile`, `math::sqrt` ile aynı model), `.` =
değer üzerinde işlem. `array::`'de element tipi receiver argümanından türetilir;
`struct::`'ta struct adı receiver tipinden gelir.

**3. Eski sözdizimi W006 ile bir sürüm yaşar:** `int::push(arr,12)` ve
`Person::toJson(p)` W006 uyarısıyla (yeni sözdizimini öneren hint ile) çalışmaya
devam eder, **v0.7.0'da kaldırılır**. `string::upper(s)` eski ve yeni modelde aynı
yazım olduğundan uyarı almaz; `string::push(sarr, x)` (element-tipi kullanımı)
uyarı alır.

**4. Registry değişmez:** `BuiltinMethodRegistry`, `ParamRule`, `ReturnRule`,
`runtimeId` mimarisi aynen kaldı; yalnızca lookup'a giden yüzey sözdizimi değişti.
`lookup()`'un sv/st/ar sıralı arama düzeni ad alanlarıyla doğal örtüşüyor:
`"array"` sol adı sv/st dallarına düşmez ve `ar:`'da bulunur.

## Uygulama notları

- `ScopeCallNode.dotCall` bayrağı eklendi; clone/toJson günceller (AST JSON'da
  yalnız yeni sözdiziminde `"dotCall": true` görünür — eski çıktılar değişmez).
- Parser: `parseLeftDenotation` DOT dalında member adından sonra `(` görülürse
  dot-call üretilir; `isScopeCallPattern`'e `KW_STRUCT` eklendi (`struct::`).
- LSP: `arr.` completion'ı artık receiver tipine göre builtin metodları önerir
  (array/string → metodlar; struct → alanlar + toJson/dump, gölgelenen metod
  önerilmez). signatureHelp UFCS'te receiver'sız, `::` biçimlerinde receiver'lı
  imza gösterir (activeParameter hizası).

## Testler

- `tests/golden/builtin/` yeni sözdizimine taşındı (array_float.sqt `array::`
  ad alanı biçiminde bırakıldı; string_part2 `string::repeat` ad alanı örneği).
- `tests/semantic/legacy_builtin.sqt` — W006 + exit 0; `field_shadow.sqt` —
  gölgeleme hatası (run.sh "builtin sözdizimi" bölümü).
- `tests/lsp/21_completion_ufcs` — `dizi.` → array metodları, `ad.` → string
  metodları; 14/19 senaryoları yeni davranışa göre güncellendi.
- IR eşdeğerliği: `a.push(2)` ve `int::push(a, 2)` aynı IR'yi üretir (diff temiz).
