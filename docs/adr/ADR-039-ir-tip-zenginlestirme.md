# ADR-039 — IR Tip Zenginleştirme (kayıpsız tip bilgisi)

İlgili: ADR-030 (heavyIR/lightIR ayrımı), ADR-037 (JIT Value ABI), #80 (MIR JIT),
ADR-032 (VM referans + diferansiyel test), CLAUDE.md ("IR = dar bel").

## Bağlam

IR bugün bazı tip bilgilerini **kaybediyor**. Tip AST'de vardır, IRGenerator onu
bilir (`structLayouts_`, tip denetleyici), ama IR çıktısına (Instruction/IRProgram)
yansıtmaz. VM bunu umursamaz — çalışma zamanında `Value.kind` etiketiyle telafi eder
(tembel olabilir, çünkü tagged). JIT edemez: statik olmak zorunda, register tek-tip.

Somut kayıp noktaları (`finalizeSlotTypes`'ın "sonuç türü opcode'dan belli değil"
dediği yerler):
- **FIELD_GET** dest tipi = struct alanının tipi → IR'de yok.
- **ARRAY_GET** dest tipi = dizi eleman tipi → IR'de yok.
- **LOAD_GLOBAL** dest tipi = global değişkenin tipi → IR'de yok.

Bunlar `slotTypes` (Dilim 1.5) fixpoint'ini bu opcode'ların dest'inde tıkıyor → JIT
struct/array/global'i derleyemiyor. Sahip ilkesi (netleştirildi): **IR, VM ve JIT'ten
AYRI bir katmandır; sadece iki backend'in bugünkü ihtiyacını değil, AST'yi olabildiğince
KAYIPSIZ yansıtmalı** — ama şişirilmemeli.

## Karar

**Tip bilgisini IR'ye taşı — opcode SAYISINI artırmadan (dar bel korunur), yalnızca
mevcut opcode'ları ve IRProgram'ı zenginleştirerek.**

1. **`Instruction::valueType` (SlotType, default Unknown).** GET-tarafı opcode'ların
   sonuç tipini taşır: **FIELD_GET / ARRAY_GET / LOAD_GLOBAL** dest tipi; **ARRAY_NEW**
   eleman tipi (init değeri için). IRGenerator bunu `structLayouts_` / tip
   denetleyiciden doldurur. `finalizeSlotTypes` GET dest'ini buradan çözer.
   - SET-tarafı (FIELD_SET / ARRAY_SET / STORE_GLOBAL) alan gerektirmez: yazılan
     değerin tipi zaten value slot'unun `slotType`'ından bilinir.
   - SlotType yeterli (Ref = pointer); nested erişim zinciri her adımda kendi
     `valueType`'ını taşıdığı için (a.b.c → ardışık FIELD_GET) tip zincir boyunca çözülür.

2. **`IRProgram::globalSlotTypes` (moduleId → vector<SlotType>).** Global değişkenlerin
   statik tipleri; IRGenerator VarDecl'den doldurur. LOAD_GLOBAL `valueType`'ı da
   buradan gelir (tek kaynak).

3. **Struct alan tipleri (GC ön-hazırlık, opsiyonel/ertelenebilir):** STRUCT_NEW'in
   hangi alanlarının Ref olduğu shadow-stack GC (#112) için gerekli olacak. Şimdilik
   FIELD_GET `valueType`'ı yeterli olduğundan STRUCT_NEW'e alan-tip tablosu eklemek
   **GC turuna ertelenir** (erken şişirme yapma).

**Dar bel & heavyIR/lightIR (ADR-030):** opcode sayısı değişmez; her opcode daha
zengin metadata taşır. Zengin tip **heavyIR** dökümünde (`--types`, cam-kutu)
görünür; lightIR sade opcode kalır (metadata dump'ta gizlenir).

**Kapsam dışı:** liveness/`detach` annotation'ı bu ADR'de YOK — statik ömür analizi
(CFG backward-dataflow) gerektirdiği için geri çekildi, GC (#112) turuna bırakıldı.

## Sonuç / Kapsam

Bu değişiklik backend-agnostik bir kazanım: `--types` cam-kutu dökümü, ileride bir
type-directed optimizasyon ve MIR aynı bilgiden içer. Doğrudan açtığı iş: JIT'te
struct/array (Dilim 2, Value↔register köprüsü) ve global. VM davranışı değişmez
(tagged Value'yu zaten kullanıyor); diferansiyel test (#92) koruması sürer.
