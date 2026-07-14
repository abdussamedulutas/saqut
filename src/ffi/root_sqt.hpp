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
)SQT";

#endif // SAQUT_FFI_ROOT_SQT
