# `src/internal/` — saQut ile yazılmış çekirdek

Buradaki `.sqt` dosyaları **derleyici binary'sine gömülür** ve çalışma zamanında
dosya sisteminden okunmaz.

## Neden var

saQut'un sentetik veri tipleri (`decimal`, `date`, ...) için "ortam desteklemiyor"
diye bir durum olmamalıdır: sonucu dilin sözleşmesi belirler, işlemci değil.
Bu hesapları C++'ta yazmak, WASM'e veya bir transpiler'a gittiğimizde
ya tökezlemek ya da elle yeniden yazmak (= VM'den sapmak) demektir.

saQut'ta yazılan bir hesap her backend'de aynı IR'ye derlenir — taşınabilirlik
otomatiktir.

## Nasıl çalışır

```
src/internal/*.sqt
      ↓  cmake (derleme sırasında, otomatik)
build/generated/internal_sources.hpp     (üretilmiş; git'e girmez)
      ↓
ModuleLoader::overlay_  →  diskten değil bellekten okur
```

**Yeni modül eklemek = buraya bir `.sqt` dosyası koymak.** Başka hiçbir yere
dokunulmaz; CMake `CONFIGURE_DEPENDS` ile yeni dosyayı kendiliğinden görür.

Dosyalar normal saQut kaynağıdır: editörde açılır, LSP görür, birbirini
`import` edebilir.

## Buraya ne girer, ne girmez

**Girer** — sonucu dilin sözleşmesi belirleyen saf hesap:
takvim aritmetiği, ondalık işlemler, biçimleme, ayrıştırma.

**Girmez** — ortamdan gelen bilgi:
`date::now()` (sistem saati), `fs::*`, `sys::env`, `print`.
Bunlar FFI'da kalır ve capability gerektirir (ADR-035).

Ölçüt: *"sonucu dilin sözleşmesi mi belirliyor, yoksa ortam mı?"*
