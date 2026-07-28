# ADR-032 — İkinci Backend: MIR JIT + Gömülü-Runtime AOT (ADR-015 Revizyonu)

> **v1 kapsam notu (2026-07-25): ADR-042 bu ADR'nin v1 takvimini ve ürün
> iddialarını supersede eder.** VM v1'in tek stabil backend'idir; MIR JIT
> `[EXPERIMENTAL]`, executable paketleme ise v1 dışıdır. Burada “gömülü-runtime
> AOT” denen yöntem klasik AOT değildir ve bundan sonra
> **bundled-runtime executable packaging** olarak anılacaktır. Aşağıdaki metin
> tarihsel backend değerlendirmesi olarak korunmuştur.

**Durum:** Uzun vadeli yön olarak kabul; v1 ürün kapsamı ADR-042 ile supersede
edildi.
**Tarih:** 2026-07-11
**Revize eder:** ADR-015 ("makine-kodu JIT kapsam dışı" maddesi), ADR-001/005'teki
backend değerlendirmeleri.

## Bağlam

ADR-015 çalıştırma modelini IR + bytecode VM olarak kilitledi ve makine-kodu
JIT'i kapsam dışı ilan etti. Bu karar, fikirlerin erken aşamada genişlemesini
önlemek için doğruydu. Ancak VM tek başına kalıcı çözüm değil: kullanıcının
yazdığı kodun masaüstü ve sunucuda **kabul edilebilir normal hızda** çalışması
gerekiyor. Hedef ham hız DEĞİL — hedef, "VM yorumlayıcı yavaşlığı" nedeniyle
dilin gerçek işlerde kullanılamaz kalmaması.

Temel kısıtlar:

1. **Kullanıcı makinesinde sıfır harici toolchain.** saQut'u kullanan hiç kimse
   MinGW/GCC/binutils/linker kurmak zorunda kalmamalı. Her şey saqut
   binary'sinin içinden çıkmalı.
2. **Determinizm > performans.** Kullanıcı düzgün kod yazarsa hızlı çalışmalı;
   kötü yazılmış kodu agresif optimizasyonla kurtarmak görevimiz değil.
3. **Cam kutu tezi:** eklenen her katman incelenebilir kalmalı.

## Değerlendirilen Yaklaşımlar

- **C transpile:** ❌ ELENDİ. Kullanıcının makinesinde C derleyicisi ister —
  kısıt #1'i ihlal eder. (ADR-015'teki "geçerli ikinci backend" statüsü
  kaldırıldı.)
- **libgccjit:** ❌ ELENDİ. "JIT" adına rağmen içeride `.s` dosyası yazıp GNU
  assembler/linker'ı çağırır, sonucu dlopen eder → binutils'e gizli runtime
  bağımlılığı = C transpile ile aynı problem. Windows desteği zayıf,
  dokümantasyon yetersiz.
- **LLVM:** ❌ FİİLEN KAPANDI. Statik linklenirse kısıt #1'i sağlar ama binary
  ~100+ MB şişer, derleme süresi ve API kararsızlığı bakım yükünü başka lige
  taşır. Tek satış noktası agresif optimizasyon — ki istemiyoruz; agresif
  optimizasyon determinizmin doğal düşmanıdır. **Çok uzak gelecek, muhtemelen
  hiç yapılmayacak.**
- **Cranelift:** ❌ Rust; C++ projesine gömmek için elle FFI katmanı + build'e
  Rust toolchain girer.
- **QBE:** ❌ Harici süreç, assembly üretir → assembler/linker ister (kısıt #1).
- **Elle codegen (AsmJit vb.):** ⚠️ Maksimum kontrol ama mimari başına instruction
  selection'ı sıfırdan yazmak orantısız mühendislik yükü.
- **MIR (vnmakarov/mir):** ✅ SEÇİLDİ.

## Karar

### 1. JIT backend'i = MIR

[MIR](https://github.com/vnmakarov/mir) — saf C, ~20K satır, MIT lisanslı,
sıfır harici bağımlılıklı JIT kütüphanesi (Vladimir Makarov / Red Hat).

- Statik linklenir; kullanıcı hiçbir şey kurmaz (kısıt #1 ✅).
- Üretilen kod GCC `-O2`'nin ~%70-90'ı hızında; derleme GCC'den ~100x hızlı.
  "Kabul edilebilir normal hız" hedefi fazlasıyla karşılanır (kısıt #2 ✅).
- x86-64, aarch64, ppc64, riscv, s390x — masaüstü + sunucu kapsamı hazır.
- MIR'in kendisi okunabilir metinsel bir IR → `saqut mir` komutu saQut IR'inden
  makine koduna giden ara katmanı da incelenebilir yapar (kısıt #3 ✅).
- saQut IR (3-adresli, slot tabanlı) → MIR eşlemesi neredeyse mekanik; ikisi
  aynı soyutlama seviyesinde.

### 2. AOT = gömülü-runtime paketleme (linker'sız)

Klasik AOT (object file → sistem linker'ı → exe) kısıt #1'i deler. Bunun yerine
`deno compile` / `bun build --compile` modeli:

- `saqut build program.sqt -o program` → saqut kendi runtime'ının bir kopyasını
  alır, derlenmiş IR/bytecode'u binary'nin sonuna gömer, tek çalıştırılabilir
  dosya üretir.
- Başlangıçta gömülü IR MIR ile JIT'lenir (MIR derlemesi çok hızlı → startup
  maliyeti milisaniyeler). Kullanıcı açısından AOT'tan ayırt edilemez: tek exe,
  dağıtılabilir, bağımlılıksız.
- Object emission, relocation, linker entegrasyonu tamamen kapsam dışı kalır.

### 3. GC kök bulma = shadow stack

JIT'lenmiş kod yerelleri gerçek CPU stack'ine/registerlara koyar; mark-sweep GC
(ADR-022) oraya bakamaz. Çözüm: native stack'in yanında yalnızca referansları
tutan **gölge yığın**. JIT'lenmiş fonksiyon girişte N slot açar, referans
atamalarını gölge slota da yazar, çıkışta kapatır. GC yalnızca gölge yığını +
globalleri tarar.

- Deterministik: kökler her an kesin bilinir, konservatif tahmin yok.
- İncelenebilir: gölge yığın sıradan bir veri yapısıdır, dökümü alınabilir
  ("GC şu an şu kökleri görüyor" = cam kutu).
- Maliyet: referans atamalarında ~%5-10 ek yazma — kabul edilir.
- Aynı teknik ileride WASM backend'inde de kullanılır (Go'nun WASM portu gibi);
  yatırım taşınır.

### 4. VM'in kalıcı rolü = referans backend

VM silinmez. Yeni bir IR opcode'u önce VM'de çalıştırılıp doğrulanır, MIR
eşlemesi sonra yazılır. İki backend aynı IR'de aynı sonucu vermek **zorundadır**
→ test stratejisi diferansiyel karşılaştırmadır (aynı program VM ve JIT'te
çalıştırılır, çıktılar bire bir eşleşmelidir).

### 5. IR = dar bel (narrow waist) disiplini

Multi-backend'in sigortası IR'in küçük ve kararlı kalmasıdır. Yeni dil özelliği
geldiğinde önce "var olan opcodelara desugar edilebilir mi?" sorulur; ancak
cevap hayırsa IR'e opcode eklenir. IR değişmeyen özellik backend'lere dokunmaz.

## Backend yol haritası

1. **MIR JIT** — `saqut run --jit`, stabilize olunca varsayılan.
2. **Gömülü-runtime AOT** — `saqut build`.
3. **WASM** — multi-backend yapısında planlı; tarayıcı/playground **en son**.
4. **LLVM** — fiilen kapalı; muhtemelen hiç yapılmayacak.

## Optimizasyon felsefesi (bağlayıcı not)

saQut'un optimizasyon görevi: kullanıcının fark edemeyeceği ölü kodu kaldırmak,
açık yapıları katlamak (constant folding, DCE). Bunlar AST/IR seviyesinde,
backend-bağımsız geçitlerdir — dar belin üstünde kalır, backend eklendikçe
yeniden yazılmaz. Vektörleştirme/agresif inlining/loop dönüşümleri sınıfı
kalıcı olarak kapsam dışıdır.

## İlgili

- ADR-015 (revize edilen çalıştırma modeli), ADR-022 (GC — shadow stack bu
  ADR'de somutlaştı), ADR-001/005 (tarihsel backend değerlendirmeleri).
- Issue'lar: MIR JIT backend, gömülü-runtime AOT (`saqut build`).
