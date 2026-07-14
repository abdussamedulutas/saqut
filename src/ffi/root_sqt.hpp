// ============================================================================
// saQut FFI — Gömülü root.sqt (Host Fonksiyon Bildirim Kataloğu)
// ============================================================================
//
// DİZİN:   src/ffi/root_sqt.hpp
// KATMAN:  FFI — derleyici binary'sine gömülü tek root.sqt kaynağı
//
// AMAÇ (ADR-034 #5, #107):
//   Tüm gömülü host fonksiyonların `ffi` bildirimleri. TEK dosya, bir kez parse
//   edilip önbelleğe alınır (derleyici binlerce derleme yapar; modül-modül FFI
//   araması istenmez). Modül üyeliği `from <ad>` ile ayrışır.
//
//   Yeni host fonksiyonu = C++ gövde (host_functions.cpp) + 1 ffi satırı + 1
//   tablo girdisi. Sembolik HOST_ID host_functions.cpp tablosundaki adla eşleşmeli.
//
// ============================================================================

#ifndef SAQUT_FFI_ROOT_SQT
#define SAQUT_FFI_ROOT_SQT

// Gömülü root.sqt kaynağı. Bir kez parse edilir (ModuleLoader önbelleği).
inline const char* kEmbeddedRootSqt = R"SQT(
// ── math modülü (saf hesap, capability'siz — #89) ──────────────────────────
// Overload yok → int/float ayrımı isimle. sqrt(-1)=NaN, Error fırlatmaz.
ffi int   abs(int x)              : MATH_ABS   from math;
ffi float absf(float x)           : MATH_ABSF  from math;
ffi int   min(int a, int b)       : MATH_MIN   from math;
ffi int   max(int a, int b)       : MATH_MAX   from math;
ffi float minf(float a, float b)  : MATH_MINF  from math;
ffi float maxf(float a, float b)  : MATH_MAXF  from math;
ffi float sqrt(float x)           : MATH_SQRT  from math;
ffi float pow(float b, float e)   : MATH_POW   from math;
ffi float floor(float x)          : MATH_FLOOR from math;
ffi float ceil(float x)           : MATH_CEIL  from math;
ffi float round(float x)          : MATH_ROUND from math;
// #89: gerçek importable sabit yok (ffi yalnızca fonksiyon) — sıfır-argümanlı
// saf fonksiyon olarak sunulur: import {PI} from math; PI();
ffi float PI()                    : MATH_PI    from math;
ffi float E()                     : MATH_E     from math;

// ── caps modülü (pledge modeli, capability'siz — #91) ───────────────────────
// drop geri alınamaz; ekleme fonksiyonu YOK (güvenlik değeri buradan gelir).
// import {drop, has} from caps; drop("fs");
ffi void drop(string capName) : CAPS_DROP from caps;
ffi bool has(string capName)  : CAPS_HAS  from caps;

// ── fs modülü (dosya sistemi, --allow-fs — #87) ─────────────────────────────
// Handle/descriptor YOK — tek atımlık read/write (record-replay önkoşulu).
ffi string  readFile(string path)                : FS_READ_FILE   from fs requires fs;
ffi byte[]  readBytes(string path)                : FS_READ_BYTES  from fs requires fs;
ffi void    writeFile(string path, string content) : FS_WRITE_FILE  from fs requires fs;
ffi void    writeBytes(string path, byte[] data)    : FS_WRITE_BYTES from fs requires fs;
ffi void    append(string path, string content)     : FS_APPEND      from fs requires fs;
ffi bool    exists(string path)                     : FS_EXISTS      from fs requires fs;
ffi void    remove(string path)                     : FS_REMOVE      from fs requires fs;

// ── sys modülü (--allow-sys — #90) ──────────────────────────────────────────
// Non-deterministik/dış-durum-okuyan; OS CSPRNG (rand() DEĞİL).
ffi float    random()                          : SYS_RANDOM     from sys requires sys;
ffi int      randomInt(int lo, int hi)         : SYS_RANDOM_INT from sys requires sys;
ffi string?  env(string name)                  : SYS_ENV        from sys requires sys;
ffi void     sleep(int millis)                 : SYS_SLEEP      from sys requires sys;
ffi string[] args()                            : SYS_ARGS       from sys requires sys;

// ── date modülü (UTC epoch-ms değer tipi — #88, ADR-036) ────────────────────
// Yalnızca now() capability ister; geri kalan saf hesap (determinizmi bozmaz).
ffi date    now()                               : DATE_NOW           from date requires sys;
ffi date    fromEpochMillis(int ms)             : DATE_FROM_EPOCH_MS from date;
ffi int     toEpochMillis(date d)               : DATE_TO_EPOCH_MS   from date;
ffi date    addDays(date d, int n)              : DATE_ADD_DAYS      from date;
ffi date    addHours(date d, int n)             : DATE_ADD_HOURS     from date;
ffi date    addMinutes(date d, int n)           : DATE_ADD_MINUTES   from date;
ffi date    addSeconds(date d, int n)           : DATE_ADD_SECONDS   from date;
ffi int     year(date d)                        : DATE_YEAR          from date;
ffi int     month(date d)                       : DATE_MONTH         from date;
ffi int     day(date d)                         : DATE_DAY           from date;
ffi int     hour(date d)                        : DATE_HOUR          from date;
ffi int     minute(date d)                      : DATE_MINUTE        from date;
ffi int     second(date d)                      : DATE_SECOND        from date;
ffi int     diffMillis(date a, date b)          : DATE_DIFF_MS       from date;
ffi date?   parse(string iso8601)               : DATE_PARSE         from date;
ffi string  format(date d, string pattern)      : DATE_FORMAT        from date;
)SQT";

#endif // SAQUT_FFI_ROOT_SQT
