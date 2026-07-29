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
// ── core modülü (import gerektirmez, her zaman mevcut) ─────────────────────
ffi string version() : CORE_VERSION from core;   // derleyici sürümü (0.8.0)

// ── math modülü (saf hesap, capability'siz — #89) ──────────────────────────
// Overload yok → int/ondalık ayrımı isimle (abs/absf). ADR-040: ondalık
// matematik 64-bit `double` üzerinden (isimlerdeki "f" tarihsel; tip double).
// sqrt(-1)=NaN, Error fırlatmaz.
ffi int    abs(int x)               : MATH_ABS   from math;
ffi double absf(double x)           : MATH_ABSF  from math;
ffi int    min(int a, int b)        : MATH_MIN   from math;
ffi int    max(int a, int b)        : MATH_MAX   from math;
ffi double minf(double a, double b) : MATH_MINF  from math;
ffi double maxf(double a, double b) : MATH_MAXF  from math;
ffi double sqrt(double x)           : MATH_SQRT  from math;
ffi double pow(double b, double e)  : MATH_POW   from math;
ffi double floor(double x)          : MATH_FLOOR from math;
ffi double ceil(double x)           : MATH_CEIL  from math;
ffi double round(double x)          : MATH_ROUND from math;
// #89: gerçek importable sabit yok (ffi yalnızca fonksiyon) — sıfır-argümanlı
// saf fonksiyon olarak sunulur: import {PI} from math; PI();
ffi double PI()                     : MATH_PI    from math;
ffi double E()                      : MATH_E     from math;

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
ffi void    copy(string from, string to)            : FS_COPY        from fs requires fs;
ffi void    move(string from, string to)            : FS_MOVE        from fs requires fs;
ffi void    rename(string from, string to)          : FS_RENAME      from fs requires fs;
ffi void    createDirectory(string path)            : FS_CREATE_DIRECTORY from fs requires fs;
ffi void    removeDirectory(string path)            : FS_REMOVE_DIRECTORY from fs requires fs;
ffi string[] list(string path)                      : FS_LIST        from fs requires fs;
ffi string[] walk(string path)                      : FS_WALK        from fs requires fs;
ffi bool    isFile(string path)                     : FS_IS_FILE     from fs requires fs;
ffi bool    isDirectory(string path)                : FS_IS_DIRECTORY from fs requires fs;
ffi int     fileSize(string path)                   : FS_FILE_SIZE   from fs requires fs;
ffi date    modifiedTime(string path)               : FS_MODIFIED_TIME from fs requires fs;
ffi string  createTempFile()                        : FS_CREATE_TEMP_FILE from fs requires fs;
ffi string  createTempDirectory()                   : FS_CREATE_TEMP_DIRECTORY from fs requires fs;

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
