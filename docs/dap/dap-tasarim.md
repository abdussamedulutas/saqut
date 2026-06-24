# saQut DAP — Hata Ayıklama (Debugging) Tasarımı

> Bu belge Debug Adapter Protocol entegrasyonunu belgeler.
> Büyük resim için önce `docs/tooling-buyuk-resim.md` okunmalıdır.

---

## 1. DAP Nedir?

**Debug Adapter Protocol**, editör ile hata ayıklama aracı arasında
JSON-RPC üzerinden konuşan bir Microsoft standardıdır. LSP'nin debugging karşılığıdır.

```
Editör (VS Code / Neovim / ...)
    │   JSON  (TCP veya stdin/stdout)
    ▼
saqut dap           ← saqut binary'sinin DAP modu
    │
    ▼
saQut VM            ← interpreter.cpp, kontrollü modda
```

LSP programı analiz eder, DAP programı **çalıştırır ve durdurur**.

---

## 2. DAP için VM'e Eklenmesi Gerekenler

Şu an `Interpreter::run()` sonuna kadar koşar ve durur.
DAP için VM'in **dışarıdan kontrol edilebilir** olması gerekir.

### 2.1 Yeni VM Durumları

```
Running  — normal yürütme
Paused   — breakpoint/step sonrası durdu, editörden komut bekliyor
Finished — program bitti
```

### 2.2 VM'e Eklenecek API (`interpreter.hpp`)

```cpp
// Breakpoint yönetimi
void setBreakpoint(const std::string& filePath, int line);
void clearBreakpoint(const std::string& filePath, int line);
void clearAllBreakpoints();

// Yürütme kontrolü
void  resume();          // Paused → Running
void  stepInstruction(); // Tek IR instruction çalıştır
void  stepOver();        // Fonksiyon çağrısını tek adım say
void  stepIn();          // Fonksiyon içine gir
void  stepOut();         // Fonksiyondan çık

// Durum sorgulama
bool  isPaused() const;
int   currentLine() const;    // IR lineTable'dan kaynak satırı
std::string currentFile() const;

// Değişken okuma
Value readSlot(int slotIndex) const;
int   frameDepth() const;
const CallFrame& frameAt(int depth) const;
```

### 2.3 Yürütme döngüsü değişikliği

```cpp
// Şu an (interpreter.cpp)
int Interpreter::run() {
    while (/* talimat var */) {
        execute(instruction);
    }
}

// DAP ile
int Interpreter::run() {
    while (/* talimat var */) {
        if (isBreakpoint(currentFile(), currentLine()))
            pause(); // DAP'a "stopped" olayı → döngü bekler

        if (state_ == Paused)
            waitForCommand(); // DAP'tan resume/step komutunu bekle

        execute(instruction);
    }
}
```

---

## 3. IR lineTable — Kritik Önkoşul

Her IR instruction'ın hangi kaynak satırından geldiğini bilmek gerekir.
`IRFunction` bu bilgiyi kısmen taşıyor; DAP için **eksiksiz ve güvenilir** olmalı.

```cpp
// ir_function.hpp — hedef yapı
struct LineInfo {
    int instrIndex;   // instruction listesindeki konum
    int sourceLine;   // kaynak .sqt dosyasındaki satır
    int sourceCol;    // kaynak sütun (step-in için)
    std::string file; // hangi modül dosyası
};

struct IRFunction {
    // ...mevcut alanlar...
    std::vector<LineInfo> lineTable; // instruction → kaynak eşlemesi
};
```

`IRGenerator` her instruction yazarken `loc` bilgisini lineTable'a ekler.
Bu veri **hem DAP hem de hata mesajları** için kritiktir.

---

## 4. DAP Mesaj Akışı — Adım Adım

### 4.1 Oturum başlatma

```
Editör → initialize      (yetenekler müzakere edilir)
saqut  ← initialized

Editör → launch          { program: "main.sqt", stopOnEntry: true }
saqut  ← (programı derle + VM'i hazırla, henüz çalıştırma)
saqut  → stopped         { reason: "entry" }  ← ilk satırda durduruldu
```

### 4.2 Breakpoint döngüsü

```
Editör → setBreakpoints  { source: "main.sqt", lines: [10, 25] }
saqut  ← (VM'e breakpoint'leri kaydet)
saqut  → breakpoints     [{ verified: true, line: 10 }, { verified: true, line: 25 }]

Editör → continue
saqut  ← VM çalışmaya devam eder
         Satır 10'a geldiğinde: pause()
saqut  → stopped         { reason: "breakpoint", threadId: 1 }
```

### 4.3 Değişken inceleme

```
Editör → stackTrace      { threadId: 1 }
saqut  ← stackFrames: [
           { id: 0, name: "main", source: "main.sqt", line: 10 },
           { id: 1, name: "add",  source: "math.sqt", line: 3  }
         ]

Editör → scopes          { frameId: 0 }
saqut  ← scopes: [{ name: "Locals", variablesReference: 1 }]

Editör → variables       { variablesReference: 1 }
saqut  ← variables: [
           { name: "x", value: "42",    type: "int" },
           { name: "p", value: "Point", type: "Point",
             variablesReference: 2 }  ← struct → expand edilebilir
         ]

Editör → variables       { variablesReference: 2 }  ← Point'i aç
saqut  ← variables: [
           { name: "x", value: "10", type: "int" },
           { name: "y", value: "20", type: "int" }
         ]
```

---

## 5. C++ Dizin Yapısı

```
src/dap/
  dap_server.hpp/.cpp     — JSON-RPC döngüsü (stdin/stdout veya TCP)
  dap_handler.hpp/.cpp    — DAP metodlarını dispatche eder
  dap_types.hpp           — Breakpoint, StackFrame, Variable, Scope yapıları
  dap_session.hpp/.cpp    — Tek debug oturumunun durumu
                            (derlenmiş program + VM örneği)

src/vm/
  interpreter.hpp/.cpp    — Mevcut; breakpoint/step API eklenir

src/ir/
  ir_function.hpp         — lineTable tamamlanır

src/cli/commands/
  dap.hpp                 — cmdDap fonksiyonu (main.cpp'ye register edilir)
```

---

## 6. Değişken Değeri Gösterimi

VM `Value` tipini (`src/vm/value.hpp`) DAP'ın beklediği stringe çevirmek gerekir:

```cpp
// src/dap/dap_types.hpp
std::string valueToString(const Value& v) {
    switch (v.kind) {
        case Value::Int:    return std::to_string(v.intVal);
        case Value::Float:  return std::to_string(v.floatVal);
        case Value::Bool:   return v.boolVal ? "true" : "false";
        case Value::String: return "\"" + v.stringVal + "\"";
        case Value::Object: return v.typeName; // "Point" gibi
        case Value::Array:  return "array[" + std::to_string(v.length) + "]";
        case Value::Null:   return "null";
    }
}
```

Struct ve array'lar `variablesReference > 0` ile işaretlenir — editör
"genişlet" oku ile alt alanları isteyebilir.

---

## 7. VS Code Eklentisi — DAP Tarafı

`saqut lsp` gibi `saqut dap` da aynı VS Code eklentisinden başlatılır.
Eklentinin `package.json`'ına debugger tanımı eklenir:

```json
"debuggers": [{
    "type": "sqt",
    "label": "saQut Debugger",
    "program": "saqut",
    "args": ["dap"],
    "languages": ["sqt"],
    "configurationAttributes": {
        "launch": {
            "properties": {
                "program": { "type": "string" }
            }
        }
    }
}]
```

VS Code bu tanımı görünce F5'e basıldığında `saqut dap` başlatır.

---

## 8. Önkoşullar ve Uygulama Sırası

```
Önce yapılmalı:
  1. IR lineTable eksiksiz doldurulmalı    (IRGenerator değişikliği)
  2. VM adım/breakpoint API eklenmeli      (interpreter.cpp)

Sonra:
  3. src/dap/ altyapısı — JSON-RPC döngüsü
  4. dap_handler: initialize, launch, setBreakpoints, continue
     → Sonuç: temel breakpoint çalışır

  5. dap_handler: stackTrace, scopes, variables
     → Sonuç: değişken paneli çalışır

  6. dap_handler: stepOver, stepIn, stepOut
     → Sonuç: adım adım yürütme çalışır
```

---

## 9. Açık Kararlar

| Soru | Seçenekler | Not |
|------|-----------|-----|
| Transport | stdin/stdout vs TCP | stdin/stdout başlangıç için yeterli |
| Çok thread | Tek thread yeterli mi? | Evet — VM zaten tek thread |
| Uzak debugging | Şimdilik gerekli mi? | TCP transport ile gelecekte mümkün |
| `stopOnEntry` | Varsayılan açık mı? | Evet — kullanıcı hemen görsün |

---

*Bağlantılı belgeler: `docs/tooling-buyuk-resim.md` · `docs/lsp/lsp-tasarim.md`*
