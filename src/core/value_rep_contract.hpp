// ============================================================================
// saQut — Tip Temsili Sözleşme Tablosu (#130)
//
// BORÇ GÖRÜNÜRLÜĞÜ ARTIFACT'I — KARAR ÜRETMEZ.
// #130 kapsamı: ValueKind → backend/tüketici eşlemesi tek tabloda toplanır
// ki "hangi backend hangi kind'ı nasıl temsil ediyor" sorusu tek bakışta
// cevaplanabilsin. Bu tablo bugünkü DURUMU kaydeder; uyumsuzlukları açık
// işaretler; hiçbir davranışı değiştirmez. Tam harmonizasyon ADR-037
// revizyonuna bağlıdır (bkz. docs/adr/ADR-037-jit-value-abi.md).
//
// EKSENLER (tüketici/backend):
//   vmStorage   : VM'in Value struct'ındaki inline alanı (src/vm/value.hpp).
//   jitRegister : MIR JIT register/slot temsili (ADR-037 + mir_backend.cpp
//                 regTypeOf: Float→MIR_T_D, Float32→MIR_T_F, diğer→MIR_T_I64).
//   jitStatus   : JIT'in bu kind için temsil durumu:
//                   "uyumlu"        — register skaler, bugün çalışıyor
//                   "tasarim"       — ADR-037'de kararlaştırıldı, Dilim 3'te
//                                     gelecek (kutulu pointer temsili)
//                   "temsil-yok"    — JIT'te karşılığı yok (reddedilir)
//   dapFormat   : DAP valueToString davranışı (src/dap/dap_handler.cpp).
//   borcNotu    : açık borç/uyarı; nerede neyin ayrıştığı.
//
// DİĞER TÜKETİCİLER (bu tabloda henüz sütunu olmayan ama kind'a bakan):
//   - host_functions (FFI): kendi ValueKind switch'i (src/ffi/host_functions.cpp).
//   - JSON serileştirme: şu an yalnız AST/diagnostic; çalışma-zamanı Value
//     serileştiricisi YOK. longint (int64) JSON'a sayı olarak sığmaz — JS/JSON
//     tüketiciler string (BigInt) ister; bu ayrım bugün hiçbir yerde temsil
//     edilmiyor (borç).
//   - bench/profile: opcode istatistiği kind'a değil opcode'a bakıyor.
// ============================================================================

#ifndef SAQUT_CORE_VALUE_REP_CONTRACT
#define SAQUT_CORE_VALUE_REP_CONTRACT

// Satır başına sözleşme. kind sütunu ValueKind enumerator adıyla EŞLEŞİR
// (src/vm/value.hpp) — bu başlık katman gereği ValueKind'a bağımlı DEĞİL,
// isimler kanoniktir; test_value_rep_contract.cpp ikisini çapraz doğrular.
struct ValueRepRow {
    const char* kind;        // ValueKind adı ("Int", "LongInt", ...)
    const char* vmStorage;   // VM inline temsili (Value alanı)
    const char* jitRegister; // JIT register/slot temsili (ADR-037)
    const char* jitStatus;   // "uyumlu" | "tasarim" | "temsil-yok"
    const char* dapFormat;   // DAP valueToString davranışı
    const char* borcNotu;    // açık borç / ayrışma notu
};

inline constexpr ValueRepRow kValueRepTable[] = {
    // ── Skalerler — VM inline, JIT register'da taşınır ─────────────────────
    {"Int",
     "Value::intValue (inline)",
     "MIR_T_I64 (skaler; EXT32)",
     "uyumlu",
     "to_string(intValue)",
     "Bool ayrı kind değildir — 0/1 int olarak saklanır (ADR-020)."},
    {"LongInt",
     "Value::int64Value (inline)",
     "MIR_T_I64 (skaler; EXT32 yok — tam 64-bit)",
     "uyumlu",
     "to_string(int64Value)",
     "int64Value alanı Date ile PAYLAŞILIR — yorum kind'a göre değişir "
     "(value.hpp yorumu güncellenmeli). JSON/JS tüketicide int64 → string "
     "(BigInt) gerekir; çalışma-zamanı JSON serileştirici henüz yok."},
    {"Float",
     "Value::floatValue (inline, double)",
     "MIR_T_D (64-bit double)",
     "uyumlu",
     "ostringstream << floatValue",
     "—"},
    {"Float32",
     "Value::floatValue (inline; (float) truncate)",
     "MIR_T_F (gerçek 32-bit single)",
     "uyumlu",
     "ostringstream << floatValue",
     "VM double slot'ta truncate eder, JIT gerçek single tutar — ADR-040 "
     "gözlemlenen davranış eşitliği bunu kapsar; iç temsil farklı."},
    {"Date",
     "Value::int64Value (inline, UTC epoch-ms)",
     "MIR_T_I64 (epoch-ms doğrudan; ADR-037 Int/Date ailesi)",
     "uyumlu",
     "to_string(int64Value)",
     "LongInt ile aynı alan — temsil ayrımı yalnız kind etiketinde."},

    // ── Kutulu / referans değerler ─────────────────────────────────────────
    {"Decimal",
     "Value::decimalValue (inline, ADR-028)",
     "pointer (kutulu — runtime call ile aritmetik)",
     "tasarim",
     "decimalValue.toString()",
     "VM inline vs JIT kutulu — BİLİNÇLİ fark, ADR-037 kayıtlı; "
     "mir_value_abi (toMir/fromMir) Dilim 3'te gelecek."},
    {"String",
     "Value::stringValue (inline, immutable — ADR-024)",
     "pointer (StringObject kutulu)",
     "tasarim",
     "\\\"...\\\" (escaped)",
     "VM inline vs JIT kutulu — BİLİNÇLİ fark, ADR-037 kayıtlı; iç bellek "
     "modeli backend'e göre değişir, gözlemlenen davranış aynı olmalı (#92)."},
    {"Ref",
     "Value::ref (inline Object*)",
     "pointer (MIR_T_I64; struct/array Object ailesi)",
     "tasarim",
     "struct/array tek-seviye özeti ({x:1} / [4,5,6])",
     "array/struct opcode'ları JIT'te henüz DİLİM YOK — opcode bazında reddedilir "
     "(opcodeSupported). Ref JIT'i ADR-037 Dilim 3/4 kapsamı."},
    {"Null",
     "kind etiketi yalnız (alan yok)",
     "yandaş isNull bayrak register'ı (#221)",
     "tasarim",
     "\"null\"",
     "VM'de null'luk Value::kind'da; JIT'te nullable slot başına GİZLİ bir "
     "isNull register'ında. Değer register'ı 0'a çekilir, null'luk ayrı bitte "
     "taşınır — bir Int register'ı 0 ile null'u ayıramaz (64 bitin tamamı "
     "geçerli değer). Eşitlik null-öncelikli ve dallanmasız üretilir (VM ile "
     "birebir). Nullable slot'u null-farkında OLMAYAN bir opcode tüketirse "
     "wholeProgramSupported reddeder: eksik kapsam kabul, yanlış cevap değil. "
     "Nullable fallible cast'ler (left==1) hâlâ reddedilir — ayrı dilim."},
};

// Tablo satır sayısı — türetilmiş; ValueKind sayısı (9) ile test çapraz doğrular.
inline constexpr int kValueRepRowCount =
    static_cast<int>(sizeof(kValueRepTable) / sizeof(kValueRepTable[0]));

#endif  // SAQUT_CORE_VALUE_REP_CONTRACT
