# ADR-034 — FFI Declaration Modeli: Gömülü root.sqt + Sayısal Host Dispatch + Import-Gated Stdlib

**Durum:** Kabul edildi (tasarım kilitli) — issue #107  
**Tarih:** 2026-07-13

## Bağlam

Stdlib dalgasından (fs/math/sys/date/caps) önce, host-destekli (gömülü) fonksiyonların
derleyiciye tanıtımı, dispatch'i ve kullanıcı koduna import edilmesi için birleşik bir
mekanizma gerekiyor. İlk taslaklarda `fs::readFile` / `math::sqrt` gibi `::` ad-alanı
çağrı sözdizimi düşünülmüştü; bu karar onu **elemektedir**.

Ayrıca `print` bugün `Interpreter::executeHostFunction`'da string-eşleşmeli özel
durumdur (`if (name == "print")`); bu ölçeklenmez.

## Karar

**1. `ffi` declaration formu (gömülü root.sqt).** Host fonksiyonları, derleyici
binary'sine gömülü tek bir `root.sqt` içinde gövdesiz `ffi` bildirimleriyle tanıtılır:

```
ffi <dönüşTipi> <ad>(<parametreler>) : <HOST_ID> from <modül> [requires <cap>] [unstable];
```

Örnekler:
```
ffi void   print(string s)             : PRINT       from core;
ffi float  sqrt(float x)               : MATH_SQRT   from math;
ffi string readFile(string path)       : FS_READFILE from fs   requires fs;
ffi date   now()                       : DATE_NOW    from date requires sys;
```

Bir `ffi` bildirimi şunları tek yerde verir: **imza** (tip denetimi + LSP/DAP),
**modül üyeliği** (`from <ad>`), **sembolik host bağı** (`: HOST_ID`), opsiyonel
**capability** (`requires <cap>`) ve **kararlılık** (`unstable`/`deprecated`).
Geriye kalan tek "ikinci kod" C++ gövdesidir (irreducible). String-registry tekrarı
yoktur.

**2. Import-gated kullanım (qualified çağrı yok).** root.sqt yalnızca DECLARATION
içerir (variable yok); global bilinir ama **kullanmak için `import` ZORUNLU:**

```
import {readFile} from fs;      // tırnaksız = modül
string c = readFile("cfg.txt"); // düz fonksiyon çağrısı
```

- Import edilmeden çağrı → derleme hatası ("declared in module X, not imported";
  LSP "import this?" önerebilir).
- Qualified `modül::fonksiyon` formu **yoktur** → import zorunlu.
- `array::`/`string::`/`struct::` (ADR-033 UFCS değer metodları) bundan ETKİLENMEZ —
  onlar import edilmez, her zaman vardır.

**3. Tırnak = dosya, tırnaksız = modül.** Import ve `from` sözdiziminde:
- **Tırnaklı** ad = dosya yolu (veri/string): `import {V} from "./util.sqt"`.
- **Tırnaksız** ad = çözümlenen modül (tanımlayıcı): `import {x} from fs`.
Bu ayrım host-import vs dosya-import belirsizliğini tek bakışta çözer ve paket
yöneticisine ölçeklenir (tırnaksız = gömülü VEYA kurulu paket; resolver çözer).

**4. Sayısal host dispatch.** Tek kaynak: C++ `enum HostFnId { PRINT, MATH_SQRT, ... }`.
root.sqt sembolik id (`: MATH_SQRT`) kullanır; derleyici build'inde `HOST_ID → index`
tutarlılığı doğrulanır (ham sayı YAZILMAZ → drift önlenir). IR `CALLHOST` sayısal id
taşır (intValue = HostFnId); VM flat `hostTable[id](args)` → O(1). `print` bu tabloya
taşınır; `executeHostFunction` string özel-durumu kalkar.

**5. root.sqt tek dosya, bir kez parse.** Modül başına parça DEĞİL. Derleyici başlangıcında
bir kez parse edilip önbelleğe alınır, tüm derlemelerde yeniden kullanılır (bir binary
binlerce derleme yapar; modül-modül FFI araması istenmez). Modül üyeliği `from <ad>` ile
ayrışır.

**6. Unstable/deprecated bir bayrağın ardında.** `unstable`/`deprecated` işaretli
bildirimler `--unstable` (veya `--allow-experimental`) olmadan import edilince hata.
Varsayılan build curated/deterministik kalır; güç kullanıcısı görünür kapıdan opt-in
yapar — "cam kutu" kimliğini korur, esnekliği verir.

## İleri genişlemeler (şimdi YAPILMAZ, iskele buna kapalı kurulmaz)

- **Opsiyonel gövde:** ileride `ffi` bir gövde alabilir; gövde, capability yokken
  runtime davranışını belirler (ör. `fs.readFile` desteklenmiyorsa tempfs'e yönlendir
  ya da hata). Fallback kararı derleyicide GİZLİ değil, gövdede görünür/incelenebilir →
  determinizm + izolasyon güçlenir.

## Sonuçlar

- `fs::`/`math::`/`sys::` ad-alanı çağrı sözdizimi hiç gelmez; #87/#88/#89/#90/#91
  stdlib issue'larındaki `::` yazımı geçersizdir.
- LSP/DAP gömülü fonksiyonlarda özel kod olmadan çalışır (gerçek AST sembolleri).
- Yeni host fonksiyonu = C++ gövde + 1 `ffi` satırı + 1 enum girdisi.
- Capability analizi (#76) `requires`'ı parse'tan okur.

## İlgili

- #107 (bu ADR'nin issue'su), #76 (capability), #85/ADR-033 (UFCS/ad alanı),
  ADR-016 (FFI seam — `callhost`), #89 (ilk modül `math`).
