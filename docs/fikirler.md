# saQut Derleyici — Mimari Fikirler ve Karar Kaydı (ADR)

> Bu belge, saQut derleyicisinin backend stratejisi, mimari kararları ve
> gelecek yol haritası hakkında kapsamlı analizleri içerir.
> Her kararın **neden** alındığı, alternatiflerin neden elendiği ve
> gelecekte hangi koşullarda tekrar değerlendirileceği belirtilmiştir.
>
> ⚠️ **Hizalama notu (sonraki oturum):** Aşağıdaki ADR-001 ve ADR-005, backend
> kod-üretimi seçeneklerini (LLVM/QBE/custom/C-transpile) ve "JIT" terimini
> tartışır. Sonradan **çalıştırma modeli netleşti** ve bazı ifadeler güncellendi:
>
> - **Birincil çalıştırma modeli = IR + bytecode VM** (yorumlayıcı döngü).
>   Bkz. `docs/adr-frontend-analiz.md` **ADR-015**.
> - **Gerçek makine-kodu JIT kapsam DIŞIDIR** (tek faydası ham hız; öncelik
>   determinizm + incelenebilirlik). Buradaki "JIT" geçişleri bu ışıkta okunmalı.
> - **C-transpile**, ileride geçerli bir **ikinci backend**'tir; QBE/custom/LLVM
>   ise "makine kodu gerçekten gerekirse libgccjit/LLVM'e bağlan" çerçevesinde
>   **çok uzak gelecektir**. ADR-001'deki karşılaştırmalar o gün için geçerli.
> - "HeavyIR/LightIR" ikiliği (ADR-005) bir **gelecek fikri** olarak durur; v0'ın
>   IR+VM hedefi tek, basit bir IR'dir + **FFI seam** (ADR-016).
> - **Yapılan vs planlanan:** ADR-001'deki "mevcut durum" listesi hâlâ doğrudur
>   (lexer/tokenizer/parser/AST + minimal IR deneyi çalışır); kod üretimi ve VM
>   **henüz yoktur.**

---

## ADR-001: Backend Stratejisi

### Bağlam

saQut derleyicisi şu anda:
- **Lexer**: Karakter seviyesinde tarama (src/lexer/lexer.hpp)
- **Tokenizer**: Token üretimi, 6 token tipi, yorum satırı desteği (src/tokenizer/tokenizer.hpp)
- **Parser**: Pratt parser ile ifade ayrıştırma + recursive descent ile statement ayrıştırma (src/parser/parser.hpp)
- **AST**: Program, FunctionDecl, Block, değişken tanımlama, if/for/while/do-while/return, expression node'ları (src/parser/ast.hpp)
- **IR**: Sadece temel matematik opcode'ları (mathadd/sub/mul/div) ve declare (src/ir/ir.hpp)

Henüz çalışan bir backend yok. Kod üretimi (code generation) aşaması boş.

### Değerlendirilen Seçenekler

#### 1. LLVM (Low Level Virtual Machine)

**Nedir**: Derleyici altyapısı. C/C++/Rust/Swift gibi dillerin kullandığı endüstri standardı.

**Artıları**:
- Agresif optimizasyonlar (loop unrolling, inlining, vectorization, LTO)
- Çok platformlu kod üretimi (x86, ARM, RISC-V, WebAssembly, GPU)
- JIT ve AOT (Ahead-of-Time) derleme desteği
- Olgun hata ayıklama bilgisi üretimi (DWARF, PDB)
- Geniş araç zinciri (llc, opt, lld, clang)
- GC (Garbage Collection) desteği için statepoint mekanizması

**Eksileri**:
- **Bağımlılık boyutu**: LLVM kütüphaneleri ~1GB+ disk alanı kaplar
- **Derleme hızı**: LLVM'nin kendi derlenmesi dakikalar alır, link zamanı yavaştır
- **Öğrenme eğrisi**: LLVM IR karmaşıktır, C++ API'si ağırdır
- **Hata ayıklama zorluğu**: LLVM IR seviyesinde hata bulmak zordur
- **Hafif projeler için aşırı**: saQut gibi deneysel bir derleyici için "sineği top ile vurmak" olur
- **Build sistemi karmaşası**: LLVM'nin kendi build sistemi CMake ile entegre olur, proje yapısını domine eder

**Karar**: ❌ Şimdilik kullanılmamalı. Deneysel aşamada çok ağır. Dil olgunlaştığında ve optimizasyon ihtiyacı somutlaştığında tekrar değerlendirilebilir.

---

#### 2. GNU Lightning (JIT)

**Nedir**: Anında makine kodu üreten hafif kütüphane. Register tabanlı, hedef mimariye göre kod üretir.

**Artıları**:
- Çok hafif (birkaç yüz KB)
- Anında kod üretimi ve çalıştırma (JIT)
- x86, ARM, MIPS, PowerPC gibi mimarilere kod üretebilir
- Kod üretimi hızlıdır (optimizasyon yapmaz, direkt çeviri)
- C API'si basit ve temiz

**Eksileri**:
- **Optimizasyon yok**: Constant folding, dead code elimination gibi temel optimizasyonlar bile yok
- **Bakım durumu belirsiz**: Proje uzun süredir aktif geliştirilmiyor
- **Sınırlı tip desteği**: Karmaşık veri tipleri ve struct'lar için manuel işlem gerekir
- **Hata toleransı düşük**: Yanlış register kullanımı sessizce yanlış kod üretir
- **Portability sorunları**: Her platformda aynı performansı vermez
- **GC ve exception handling desteği yok**

**Karar**: ⚠️ Prototip aşamasında kullanılabilir ancak üretim için uygun değil.

---

#### 3. Sıfırdan Custom Backend (Go yaklaşımı)

**Nedir**: Go dilinin yaptığı gibi, kendi kod üreticini yazmak.

**Go'nun yaklaşımı**:
- Go başlangıçta Plan 9 assembler'dan kendi assembler'ına geçti
- Kendi register allocator, instruction selector ve optimizer'ını yazdı
- Sonuç: LLVM bağımlılığı yok, hızlı derleme, tam kontrol
- Go 1.21+ ile PGO (Profile-Guided Optimization) bile eklendi

**Artıları**:
- **Tam kontrol**: Her şeyi istediğin gibi tasarlayabilirsin
- **Bağımlılık yok**: Dış kütüphane gerektirmez
- **Hızlı derleme**: Optimizasyon seviyesini sen belirlersin
- **Dil ile entegrasyon**: saQut diline özel optimizasyonlar yapabilirsin
- **Öğrenme değeri**: Derleyicinin her katmanını anlarsın

**Eksileri**:
- **Çok iş**: Register allocation, instruction selection, calling convention, stack frame yönetimi, peephole optimization... hepsini sıfırdan yazmak aylar sürer
- **Platform bağımlılığı**: Her hedef mimari için ayrı kod üretici gerekir
- **Optimizasyon kalitesi**: LLVM seviyesinde optimizasyon yapmak yıllar alır
- **Bakım yükü**: Tüm backend hataları senin sorumluluğunda

**Karar**: ✅ **Önerilen uzun vadeli strateji**. Aşamalı olarak inşa edilmeli:
1. Aşama: C koduna transpile et (hızlı prototip, hemen çalışır)
2. Aşama: Basit bir register allocator + x86-64 kod üretici
3. Aşama: Orta seviye optimizasyonlar ekle
4. Aşama: ARM64 desteği ekle

---

#### 4. QBE (Quick Backend)

**Nedir**: LLVM'den 10 kat daha hızlı, hafif bir derleyici backend'i. cproc, harecc gibi C derleyicileri tarafından kullanılır.

**Artıları**:
- LLVM'den çok daha hafif (birkaç MB)
- Hızlı kod üretimi (LLVM'den ~10x)
- Makul optimizasyonlar (register allocation, copy propagation, memory folding)
- x86-64 ve ARM64 desteği
- Basit SSA-tabanlı IR

**Eksileri**:
- C'de yazılmış, FFI gerektirir
- Optimizasyonlar LLVM kadar agresif değil
- Dokümantasyon İngilizce, küçük topluluk
- 32-bit ve RISC-V desteği deneysel
- Hata ayıklama bilgisi (DWARF) desteği yok

**Karar**: ✅ **Orta vadede en iyi seçenek**. Custom backend yazılana kadar QBE ideal bir ara çözüm.

---

#### 5. Cranelift (WebAssembly odaklı)

**Nedir**: Bytecode Alliance tarafından geliştirilen, Rust'ta yazılmış JIT/AOT derleyici backend'i. Wasmtime'ın JIT motoru.

**Artıları**:
- Hızlı JIT derlemesi
- x86-64, ARM64, RISC-V64 desteği
- Güvenlik odaklı (memory safety, sandboxing)
- Modern mimari (SSA, e-graphs)

**Eksileri**:
- Rust'ta yazılmış, C++ projesine entegrasyon zor
- WebAssembly odaklı, native diller için ikincil öncelik
- Dokümantasyon sınırlı, hızlı değişiyor
- Optimizasyonlar LLVM kadar agresif değil

**Karar**: ❌ saQut gibi C++ tabanlı bir proje için uygun değil.

---

#### 6. C Koduna Transpile Etme

**Nedir**: AST'yi doğrudan C kaynak koduna çevirip GCC/Clang ile derlemek.

**Artıları**:
- **En hızlı prototip yolu**: Hemen çalışan bir sistem
- GCC/Clang optimizasyonlarından bedava faydalanma
- Hata ayıklama kolay (üretilen C kodunu okuyabilirsin)
- Her platformda çalışır (C derleyicisi olan her yerde)

**Eksileri**:
- İki aşamalı derleme (yavaş)
- saQut'a özgü optimizasyonlar kaybolabilir
- Debug bilgisi orijinal kaynak koda değil, üretilen C koduna işaret eder
- Dil özellikleri C'nin sınırları içinde kalır
- Hata mesajları C derleyicisinden gelir, anlaşılması zor

**Karar**: ✅ **Birinci aşama için ideal**. Hemen çalışan bir sistem kurup, sonra native backend'e geçiş yapılabilir.

---

### Nihai Karar ve Yol Haritası

```
┌─────────────┐     ┌──────────────────┐     ┌─────────────────┐
│  Aşama 1    │────▶│  Aşama 2         │────▶│  Aşama 3        │
│  C Transpile│     │  QBE Backend     │     │  Custom Backend │
│  (hemen)    │     │  (orta vade)     │     │  (uzun vade)    │
└─────────────┘     └──────────────────┘     └─────────────────┘
    1-2 hafta            2-4 hafta              2-6 ay
```

**Aşama 1 — C Transpile**: Hemen başlanabilir. Mevcut AST ve IR'yi C koduna çevirip GCC ile derlemek. Bu sayede:
- Dilin semantiği test edilebilir
- Gerçek programlar çalıştırılabilir
- Backend baskısı olmadan dil geliştirmeye devam edilebilir

**Aşama 2 — QBE**: Dil yeterince olgunlaştığında, QBE ile native kod üretimi:
- C derleyicisi bağımlılığı kalkar
- Derleme hızı artar
- Temel optimizasyonlar QBE tarafından yapılır

**Aşama 3 — Custom Backend**: Dil tamamen stabilize olduğunda:
- Tam kontrol
- saQut'a özgü optimizasyonlar
- Minimum bağımlılık

---

## ADR-002: Parser Mimarisi — Neden Pratt?

### Bağlam

C/C++/Java gibi diller genellikle **recursive descent** veya **LALR(1) parser** (yacc/bison) ile ifade ayrıştırması yapar. saQut için hangi yaklaşım seçilmeli?

### Değerlendirilen Seçenekler

#### Recursive Descent (elle yazılmış)
- **+** Basit, okunabilir, hata mesajları kontrol edilebilir
- **+** Java/C# benzeri dillerde yaygın
- **−** Operatör önceliğini yönetmek için çok sayıda fonksiyon gerekir (parseAddExpr, parseMulExpr, parseUnaryExpr...)
- **−** Yeni operatör eklemek zor

#### Pratt Parser (Top-Down Operator Precedence)
- **+** Operatör önceliğini merkezi bir tabloda yönetir
- **+** Yeni operatör eklemek tek satır (tabloya ekle + NUD/LED yaz)
- **+** Kod tekrarı yok, single source of truth
- **+** Hem prefix hem infix hem postfix operatörleri aynı çerçevede işler
- **−** Recursive descent kadar yaygın bilinmez
- **−** İlk bakışta anlaşılması zor gelebilir

#### LALR(1) / Parser Generator
- **+** Gramer tanımı net
- **−** Hata mesajları anlaşılmaz
- **−** Shift/reduce conflict'leri ile uğraşmak zaman kaybı
- **−** Generated code okunamaz, debug zor

### Karar

✅ **Pratt Parser** seçildi çünkü:
1. saQut'un operatör seti geniş ve büyüyebilir (ileride `|>`, `?.`, `??` gibi özel operatörler eklenebilir)
2. Operatör önceliğini merkezi bir tabloda (TokenPrecedence) yönetmek, kod tekrarını önler
3. Hem ifadeler hem prefix/postfix operatörler aynı çerçevede işlenir
4. Recursive descent'in statement tarafı için kullanılması, Pratt'in ifade tarafı için kullanılması hibrit bir yaklaşım sunar (en iyi iki dünya)

---

## ADR-003: Neden Header-Only?

### Bağlam

saQut derleyicisi tüm `.hpp` dosyalarında hem tanım (declaration) hem gerçekleme (implementation) içerir. Geleneksel C++ projelerinde `.hpp` + `.cpp` ayrımı yapılır.

### Değerlendirme

**Header-only avantajları**:
- Tek dosya = tek gerçeklik. Tanım ve gerçekleme arasında senkronizasyon sorunu olmaz
- `inline` anahtar kelimesi ile ODR (One Definition Rule) ihlali önlenir
- Derleme süreci basit: tek bir `.cpp` dosyası (main.cpp) her şeyi include eder
- Dağıtım kolay: Tüm derleyici tek bir header koleksiyonu

**Header-only dezavantajları**:
- Tüm kod her yerde görünür (ama zaten açık kaynak)
- Büyük projelerde derleme süresi uzayabilir
- Circular dependency riski (ama include guard'lar ile yönetiliyor)

### Karar

✅ **Header-only** devam ediyor. saQut şu anda küçük bir proje ve bu yaklaşım:
1. Kodun anlaşılmasını kolaylaştırır (dosyalar arası atlama yok)
2. Build sistemini basitleştirir
3. Hızlı iterasyon sağlar

Gelecekte proje çok büyürse (100K+ satır), `.hpp` + `.cpp` ayrımına geçilebilir.

---

## ADR-004: Token Sistemi — Neden Polymorphic Token Sınıfları?

### Bağlam

Tokenizer farklı token tipleri için farklı veri alanlarına ihtiyaç duyar:
- NumberToken: `isFloat`, `hasEpsilon`, `base`
- StringToken: `context`, `size`
- IdentifierToken: `context`, `size`

İki yaklaşım var:
1. **Tagged union**: Tek bir Token struct'ı, içinde `union` veya `std::variant`
2. **Class hierarchy**: Base Token sınıfı, her tip için alt sınıf

### Karar

✅ **Class hierarchy** seçildi çünkü:
1. C++'ta doğal ve yaygın bir pattern
2. Yeni token tipi eklemek kolay (yeni sınıf türet)
3. Tip güvenliği: `dynamic_cast` veya `gettype()` string karşılaştırması ile tip kontrolü
4. Bellek yönetimi açık: heap'te `new` ile oluşturulup pointer olarak saklanıyor

⚠️ **Bilinen sorun**: `ParserToken` yapısı eskiden `Token token` (değer kopyası) tutuyordu, bu object slicing'e neden oluyordu (alt sınıf verileri kayboluyordu). `commit 40579ca` ile `Token* token` pointer'a geçildi.

---

## ADR-005: IR Tasarımı

### Bağlam

Mevcut IR (src/ir/ir.hpp) sadece 5 opcode içeriyor: `declare`, `mathadd`, `mathsub`, `mathmul`, `mathdiv`. Bu bir "virtual register" IR'si — her işlem yeni bir sanal register'a yazılır.

### Mevcut Durum

```
OPCode: declare, mathadd, mathsub, mathmul, mathdiv
Param:  {isRegister: bool, value: variant<int,float>}
IROpData: {op: OPCode, targetReg: int, arg1-3: Param}
```

### Eksikler (TODO)

- [ ] Kontrol akışı: `branch`, `jump`, `compare`
- [ ] Fonksiyon çağrısı: `call`, `ret`
- [ ] Bellek: `load`, `store`, `alloc`
- [ ] Tip bilgisi: IR opcode'ları tipleri taşımıyor
- [ ] Debug bilgisi: Kaynak satır eşlemesi yok

### Gelecek Yön

IR'nin iki katmanlı olması planlanıyor:
- **HeavyIR**: Debug bilgisi, tip bilgisi, değişken isimleri içeren zengin IR (interpreter/debug için)
- **LightIR**: Sadece çalıştırma için gerekli minimum IR (JIT/compiler için)

---

## Performans Karşılaştırması: JIT vs AOT

| Kriter | JIT (Lightning/Custom) | AOT (LLVM/QBE/Custom) | Transpile (C) |
|---|---|---|---|
| İlk derleme hızı | ⚡ Çok hızlı (mikrosaniye) | 🐢 Yavaş (saniye) | 🐢 Orta |
| Çalışma hızı | 🐢 Optimizasyonsuz | ⚡ Yüksek optimizasyon | ⚡ GCC/Clang seviyesi |
| Bellek kullanımı | ✅ Düşük | ⚠️ Yüksek (LLVM) | ✅ Derleme anında yok |
| Debug kolaylığı | ⚠️ Makine kodu seviyesi | ✅ Kaynak eşlemesi var | ⚠️ C kodu üzerinden |
| Platform bağımsızlığı | ⚠️ Her mimariye özel | ✅ LLVM her yerde | ✅ C her yerde |
| Geliştirme süresi | ⚡ Kısa (Lightning ile) | 🐢 Uzun (LLVM öğrenme) | ⚡ En kısa |

### Sonuç

**Prototip için**: C transpile > QBE > JIT
**Üretim için**: Custom backend > QBE > LLVM
**Dinamik kod (REPL) için**: JIT (Lightning veya custom)

---

## Gelecek Özellikler (Roadmap)

### Kısa Vade (1-4 hafta)
- [ ] C koduna transpile (Aşama 1 backend)
- [ ] Tip kontrolü (symbol table)
- [ ] Fonksiyon parametreleri
- [ ] else-if zincirleri
- [ ] Mantıksal operatörler (&&, ||) kısa devre değerlendirmesi

### Orta Vade (1-3 ay)
- [ ] QBE backend entegrasyonu
- [ ] Array/dizi desteği
- [ ] Struct/record tipleri
- [ ] Import/include sistemi
- [ ] Hata mesajlarında kaynak satır gösterimi
- [ ] Basit optimizasyonlar (constant folding, dead code elimination)

### Uzun Vade (3-12 ay)
- [ ] Custom native backend
- [ ] Interpreter modu (REPL)
- [ ] Debugger desteği (DWARF)
- [ ] Package yöneticisi
- [ ] LSP sunucusu (IDE desteği)
- [ ] Kendi kendini derleyebilme (self-hosting)

---

## Mimari Prensipler

1. **Tek sorumluluk**: Her dosya/class tek bir iş yapar
   - Lexer: Karakter → sayı/konum
   - Tokenizer: Lexer → Token
   - Parser: Token → AST
   - IR Generator: AST → IR
   - (Gelecek) Code Generator: IR → Makine kodu / C kodu

2. **Bağımlılık yönü**: Tek yönlü
   ```
   Lexer ← Tokenizer ← ParserToken ← AST ← Parser ← IR
   ```

3. **Test edilebilirlik**: Her katman bağımsız test edilebilir
   - Lexer: `scan("42")` → `INumber{42, base=10}`
   - Tokenizer: `scan("1+2")` → `[NumberToken, OperatorToken, NumberToken]`
   - Parser: `parse(tokens)` → `ASTNode*`
   - IR: `parse(ast)` → `vector<IROpData>`

4. **Hata toleransı**: Parser mümkün olduğunca ilerlemeye çalışır, ilk hatada durmaz (ileride panic mode eklenecek)

5. **Kademeli geliştirme**: Her aşamada çalışan bir sistem. "Big bang" entegrasyon yok.
