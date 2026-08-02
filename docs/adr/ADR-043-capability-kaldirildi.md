# ADR-043 — Capability Modeli Kaldırıldı (ADR-035 supersede)

İlgili: ADR-035, #76, #87, #90, #91.

## Durum

**Yerini alır:** ADR-035 (Capability Modeli — `--allow-fs/net/sys`, A+B enforcement).
ADR-035 silinmedi; artık yürürlükte değil. Bu kayıt neden kaldırıldığını ve
kodun yeni durumunu anlatır.

## Karar

**Capability modeli tamamen kaldırıldı.** `fs`/`net`/`sys` izin kategorileri,
`requires <cap>` bildirimi, `--allow` CLI bayrağı, `caps::drop`/`caps::has`
modülü ve A+B runtime backstop'u artık yok. Host fonksiyonları (dosya I/O, sistem
çağrıları, zaman `now()`) capability denetimi olmadan, doğrudan çalışır.

Gerekçe: capability modeli (varsayılan kapalı + açık izin) bir güvenlik katmanı
olarak öngörülmüştü; ürün kararıyla bu katman kaldırılarak host çağrıları
unconditional hale getirildi. Bu aynı zamanda JIT'in `HOST_NEEDS_CAPS`
(ret) katmanını kaldırarak JIT'te fs/sys host çağrılarını çalıştırılabilir kıldı.

## Kod değişikliği (net etki)

- `src/core/capability.hpp` — içerik boşaltıldı (enum/fonksiyon yok).
- `HOST_NEEDS_CAPS` bayrağı (host_abi.hpp, host_registry.cpp) — kaldırıldı.
- `ffi ... requires <cap>;` sözdizimi ve `Symbol::requiredCap`,
  `Instruction::requiredCap`, `IRFunction::capRequirements` akışı — kaldırıldı.
- `Interpreter` `E_CAP_MISSING` backstop'u, `caps_` kümesi, `setCapabilities/
  hasCapability/dropCapability` — kaldırıldı.
- `caps::drop`/`caps::has` host fonksiyonları + `CAPS_DROP`/`CAPS_HAS`
  registry girişleri — kaldırıldı.
- CLI `--allow` bayrağı ve `--capabilities` statik analizi — kaldırıldı.
- JIT `isSupportedCallhost` `HOST_NEEDS_CAPS` reti — kaldırıldı; `rt_jit_host_call`
  `HostKind::Ref` (byte[]) köprüsü eklendi → JIT'te dosya işlemleri çalışır.
- capability bağımlı golden testleri (`caps/*`, `*_no_permission`,
  `ir/capability_operand`) kaldırıldı.

## Sonuç

Artık capability kapısı yok: `readFile`, `writeFile`, `sleep`, `now()`, `random()`
vs. capability bayrağı olmadan çalışır (varsayılan durum "her şey açık"). Golden
suite 88/88 geçer; kalan JIT try/catch parity boşlukları bilinçli kapsam
dışıdır (AGENTS.md §8 notu).

## Kapsam dışı not

JIT'in `try/catch`/exception dilimi hâlâ bilinçli kapsam dışıdır (AGENTS.md §8);
capability kaldırılması bunu etkilemez.
