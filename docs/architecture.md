# saQut — Derleyici Mimarisi (Katman Modeli)

> **Statü uyarısı (2026-07-25):** Bu belge mevcut kaynak kodun eksiksiz tarifi
> veya v1 release sözleşmesi değildir. ADR-041'deki self-hosted stdlib/thin
> runtime hedef hipotezini anlatır. ADR-042 ile bu mimari v1 için kilitli olmaktan
> çıkarılmıştır. v1'in bağlayıcı kapsamı `v1.0-kapsam-bildirgesi.md` dosyasındadır.
>
> Bu belge saQut derleyicisinin **katman mimarisini** tanımlar. Kilitli kararlar
> `docs/adr/`'de yaşar; bu belge onların bütünsel resmini verir. Anlık durum →
> GitHub issue #101 (pinli). Çakışma olursa ADR'ler esastır.
>
> Temel referanslar: **ADR-041** (self-hosted stdlib + thin runtime — bu belgenin
> omurgası), ADR-032 (backend stratejisi), ADR-037 (Value ABI), ADR-038 (determinizm),
> ADR-034 (FFI modeli), ADR-017 (batteries = FFI sınırı).

---

## Temel tasarım ilkesi: backend bağımsızlığı

> **Yeni bir backend eklemenin maliyeti, builtin/stdlib fonksiyon sayısından
> bağımsız olmalıdır.**

Bu, saQut mimarisinin en üst kısıtıdır. Derleyici birden çok backend hedefliyor
(VM referans + MIR JIT + gömülü-runtime AOT; 2.0 ufkunda WASM ve olası LLVM).
Eğer her yeni özellik (bir string fonksiyonu, bir stdlib modülü) her backend'de
ayrı yazılmak zorunda olsaydı, maliyet `N_backend × N_özellik` olurdu ve proje
belli bir noktada genişleyemez hale gelirdi.

Mimari bu maliyeti **toplama** indirir (`N_backend + N_özellik`) ve bunu dört
katmanla sağlar: fonksiyon davranışının büyük çoğunluğu backend'lerin **altında
değil, üstünde** — dilin kendisinde (IR olarak) yaşar. Backend yalnızca küçük,
kapalı bir çekirdeği bilir; gerisi ona görünmez.

Ayrıntılı gerekçe, karar ağacı ve fonksiyon sınıflandırması: **ADR-041**.

---

## Dört katman

```
┌─ Katman 3 — Self-hosted stdlib ────────────────────────────────────────────┐
│  saQut KAYNAĞIYLA yazılır, derleyiciye gömülü (std/*.sqt).                  │
│  substring · split · trim · replace · base64 · crc32 · json · utf8 · path · │
│  uuid · seeded-PRNG · regex …                                              │
│                                                                            │
│  Normal pipeline'dan geçer: parse → tip → IR.                              │
│  BACKEND'E GÖRÜNMEZ — yalnızca Katman-1 opcode + Katman-2 CALLHOST üretir.  │
│  ► Yeni backend maliyeti: SIFIR.                                           │
└──────────────────────────────────┬─────────────────────────────────────────┘
                                   │ yalnızca kullanabilir ↓
┌─ Katman 2 — Host FFI seam ─────────────────────────────────────────────────┐
│  Dünyaya dokunan atomlar (C++ ZORUNLU): syscall, entropi, sistem saati,    │
│  vendored kripto/sıkıştırma.                                               │
│  fs.readFile · net.connect · sys.random · date.now · console.write · crypto│
│                                                                            │
│  TEK jenerik host-çağrı ABI: CALLHOST + sayısal HostFnId (ADR-034).        │
│  Capability TEK kapıda enforce (instr.requiredCap, ADR-035).               │
│  ► Yeni backend maliyeti: SABİT (FFI sayısından bağımsız — tek köprü).     │
└──────────────────────────────────┬─────────────────────────────────────────┘
                                   │ üstünde durur ↓
┌─ Katman 1 — Çekirdek intrinsic opcode'lar ─────────────────────────────────┐
│  Dilin primitifleriyle İFADE EDİLEMEYEN atomlar (kapalı, küçük ~50 opcode):│
│  byte-get/byte-len/bytes↔string · array new/get/set/len · struct field ·   │
│  aritmetik · karşılaştırma · kontrol akışı.                               │
│                                                                            │
│  ► Yeni backend maliyeti: SABİT (küçük) — her backend bunları codegen eder.│
└──────────────────────────────────┬─────────────────────────────────────────┘
                                   ▼
┌─ Katman 0 — Backend ───────────────────────────────────────────────────────┐
│  VM (referans) · MIR JIT · gömülü-runtime AOT · [WASM] · [LLVM?]           │
│  Register allocation, calling convention — tamamen backend-özel.           │
└────────────────────────────────────────────────────────────────────────────┘
```

**Okuma:** Bir backend'in bilmesi gereken **yalnızca Katman 1 + Katman 2**'dir.
Katman 3'ün yüzlerce fonksiyonu backend için görünmez — o sadece Katman-1'e
derlenmiş IR'dir. Backend IR çalıştırmayı zaten Katman-1 için biliyor; stdlib
"önceden yazılmış IR" olduğundan bedava gelir.

---

## Katmanların sorumlulukları

| Katman | Ne | Nerede yaşar | Yeni backend maliyeti | Yeni fonksiyon maliyeti |
|---|---|---|---|---|
| **3 — stdlib** | saf, ifade-edilebilir işlemler | `std/*.sqt` (gömülü kaynak) | **0** | 1 saQut fonksiyonu, backend'e 0 dokunuş |
| **2 — FFI** | dünya-teması, vendored kütüphane | C++ host + tek `ffi` bildirimi (`root_sqt.hpp`) | sabit (tek köprü) | 1 C++ gövde + 1 `ffi` satırı + 1 HostFnId |
| **1 — intrinsic** | ifade-edilemez atomlar | her backend'de codegen | sabit (~50 opcode) | nadir — yalnızca yeni atomik yetenek |
| **0 — backend** | RA, calling convention | backend-özel | tam (yeni backend'in kendisi) | — |

---

## Fonksiyon nereye gider? (özet karar ağacı)

Ayrıntı ve gerekçeler ADR-041 §3'te. Özeti:

```
1. Syscall / entropi / sistem-saati mı?          → Host FFI (Katman 2)
2. Güvenlik-kritik (kripto) mi?                  → Host FFI (vendored, ADR-017)
3. Olgun-kütüphane + doğruluk-kritik (sıkıştırma)? → Host FFI (vendored, ADR-017)
4. Dilin primitifleriyle ifade edilemez mi?      → Intrinsic (Katman 1)
5. Kalan her şey (saf + ifade-edilebilir)        → Self-hosted stdlib (Katman 3)
   5a. Büyük statik veri mi?  → stdlib + veri-seam (ertelenebilir)
   5b. Ölçülmüş hot-path mı?  → intrinsic-terfi adayı (önce stdlib yaz, ölç)
```

"Hissettirdiği için opcode" yasak — sınıf bu ağaçtan mekanik çıkar.

---

## Backend-bağımsızlık invariant'ı (mekanik garanti)

Temel ilke bir CI kontrolüyle **mekanik** olarak korunur:

> **stdlib'in ürettiği IR yalnızca Katman-1 opcode + Katman-2 CALLHOST içerir.**

Biri Katman 3'e backend-özel bir sembol/varsayım sızdırırsa CI patlar. Bu,
backend-bağımsızlığın kağıt-üstü vaadi değil, `saqut ir` çıktısı üstünde bir
kontrol kadar somut bir derleme-zamanı garantisidir. (ADR-041 §9.)

---

## Mevcut pipeline (Katman 0-1 bugün nasıl çalışıyor)

Katman modeli hedef mimaridir; migrasyon dikey dilimlerle ilerler (ADR-041 §8).
Bugün çalışan uçtan uca hat:

```
kaynak → lexer/tokenizer → Pratt parser → AST (+ JSON serileştirme)
       → sembol tablosu (iki-geçiş, döngü tespiti)
       → tip denetleyici + yapısal doğrulayıcı (+ diagnostic motoru)
       → optimizasyon (constant folding + DCE, klon üstünde — ADR-007)
       → IR üreteci (3-adresli, slot-tabanlı)
       → Katman 0:  VM (bytecode yorumlayıcı, referans backend)
                    MIR JIT (int/float/string/decimal skaler dilimleri)
```

CLI: `tokens / ast / symbols / check / ir / run / exec / bench / lsp / dap`.
Her ara aşama dışarıdan incelenebilir (cam-kutu ilkesi) — self-hosted stdlib bu
incelenebilirliği stdlib'in **kendisine** de taşır (ADR-041 §7).

Ayrıntılı "yapılan vs planlanan" envanteri: `CLAUDE.md` → "Mevcut durum" ve
issue #101.

---

## İlgili ADR haritası (mimari kararlar)

- **ADR-041** — self-hosted stdlib + thin runtime (bu belgenin omurgası)
- ADR-032 — backend stratejisi: MIR JIT + gömülü-runtime AOT; LLVM fiilen kapalı
- ADR-034 — FFI declaration modeli + sayısal host dispatch (Katman 2 mekanizması)
- ADR-035 — capability modeli (Katman 2 tek-kapı enforce)
- ADR-037 — JIT Value ABI (tip temsili backend başına — açık borç, bkz. §7)
- ADR-038 — determinizm + sürüm uyumluluğu (gözlemlenen davranış = sözleşme)
- ADR-017 — batteries = FFI sınırı; kripto/sıkıştırma elle yazılmaz (Katman 2)
- ADR-030 — heavyIR / lightIR ayrımı
- ADR-007 — analiz orijinal AST'te, optimizasyon klon üstünde
