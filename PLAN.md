# PLAN.md — MIR JIT Kalan İş Planı (#80)

> Bu belge JIT backend'e dönünce nereden devam edileceğinin haritasıdır.
> Detaylı ilerleme kaydı: hafıza `mir-jit-ilerleme` + issue #80.
> Tasarım: ADR-032 (backend), ADR-037 (Value ABI), ADR-039 (IR tip zenginleştirme).

## Durum (2026-07-15)

| Dilim | Kapsam | Durum |
|---|---|---|
| 0/1 | İskelet + int-skaler tam küme (fibonacci JIT) | ✅ |
| 1.5 | slot-tip tablosu + float skaler | ✅ |
| 3 | string (LOAD_STRING/concat/==/!=), cast (skaler), decimal (kutulu) | ✅ |
| — | IR tip zenginleştirme ŞEMASI (ADR-039: valueType, globalSlotTypes) | ✅ (doldurma bekliyor) |
| 2 | struct/array + shadow-stack GC | ⏳ altyapı hazır, codegen yok |
| — | global (LOAD/STORE_GLOBAL) | ⏳ IR şema hazır |
| — | nullable (null/T?) | ⏳ tasarım gerekli |
| 5 | try/catch (ENTER_TRY/LEAVE_TRY/THROW) | ⛔ #110 bloklu |

Tüm biten dilimler VM≡JIT diferansiyel geçti. Nesneler host arenasında
(g_jitRuntimeStrings/g_jitDecimals), program sonu toplu silinir (GC #112'ye ertelendi).

## Sıradaki: struct/array + global (IR altyapısı ADR-039 ile kuruldu)

**Adım 1 — IRGenerator doldurma:**
- `Type → SlotType` köprü helper'ı (isArray/isStruct→Ref; String→Str; Decimal→Decimal;
  Date→Date; Float/Double→Float; Int/Byte/Bool/Char→Int).
- emit noktalarında `ins.valueType` doldur: `emitFieldGet` (field tipi, structLayouts_'tan),
  `emitArrayGet`/`emitArrayNew` (eleman tipi, node.resolvedType'tan), `emitLoadGlobal`
  (global tipi). Emit imzalarına SlotType param ekle; çağıranlar node.resolvedType'tan geçir.
- Global tipleri `program.globalSlotTypes[moduleId]`'e doldur (VarDecl global işlenirken,
  ir_generator.cpp:50/114 civarı).

**Adım 2 — finalizeSlotTypes:**
- FIELD_GET/ARRAY_GET/LOAD_GLOBAL dest tipini `ins.valueType`'tan çöz (şu an Int'te
  tıkanıyor). Bu, slotTypes fixpoint'ini açar → JIT bu opcode'ları görebilir.

**Adım 3 — mir_backend (Value↔register köprüsü):**
- SlotType::Ref register izni (I64 pointer).
- STRUCT_NEW/ARRAY_NEW: host arena'da StructObject/ArrayObject (allocStruct/allocArray
  benzeri; GC #112'ye kadar leak-then-bulk-free). dest = pointer.
- FIELD_GET/SET, ARRAY_GET/SET, ARRAY_LEN: elemanlar tagged Value; register tek-tip →
  eleman tipine göre (valueType) trampolin: Value↔register dönüşümü. ⚠️ String elemanı:
  JIT StringObject* ↔ VM Value inline string köprüsü (ADR-037 kutulama farkı elemanda).
- LOAD_GLOBAL/STORE_GLOBAL: sabit-adres global storage (flat, globalSlotTypes'tan tip),
  mem load/store (float→D, diğer→I64).
- Her adımda diferansiyel test (VM≡JIT).

## Sonraki (ayrı turlar)

- **nullable**: null'un register temsili (int? null≠0) + null-safe path (null string
  deref). LOAD_NULL + nullable-hedef cast şu an REDDEDİLİYOR (temiz unsupported).
- **GC (#112)**: shadow-stack kökleri (MIRPLAN §8) → arena'yı mark-sweep'e çevir.
  Sahibin algoritması issue #112'de.
- **Dilim 5 try/catch (#110 bloklu)**: deterministik-stacktrace açık sorusu kapanmadan
  kod YAZILMAZ. Bununla birlikte **uncaught hata prefix farkı** ("runtime error:" vs
  "exec: runtime error:") çözülür — JIT trampolinleri exit ile çıkıp exec/run
  wrapper'ından geçmiyor; hata-yayma mimarisi birleştirecek.
- **exec --jit bozuk** ("undeclared reg 0"): exec sarmalayıcısı JIT'in beklediği main'i
  üretmiyor (double-wrap kaynaklı) — ayrı bug, diferansiyel testte gerçek `.sqt` dosyası kullan.
- **AOT (#81)**: LOAD_STRING/LOAD_DECIMAL derleme-zamanı-pointer-gömme yolu farklı
  process'te çalışmaz → runtime call'a çevrilecek.
- **Diferansiyel test altyapısı (#92)**: şu an elle; otomatik golden'a bağlanmalı.
