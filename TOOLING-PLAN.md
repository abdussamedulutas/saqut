# saQut — Araç Zinciri Uygulama Planı

> **Bu belge bir yapay zekaya handoff'tur.**
> Renklendirme (TextMate), akıllı editör özellikleri (LSP) ve hata ayıklama (DAP)
> için sıfırdan uygulamaya geçecek kişinin ihtiyacı olan her şeyi içerir.
> Felsefe, tasarım kararları ve alternatifler için `docs/tooling-buyuk-resim.md`,
> `docs/lsp/lsp-tasarim.md` ve `docs/dap/dap-tasarim.md` okunabilir.
> Bu belge ise **ne, nerede, nasıl** sorusuna odaklanır.

---

## Mimari Karar — Tek Binary

```
saqut run   source.sqt   → derleme + çalıştırma (mevcut)
saqut exec  "1+2"        → inline ifade (mevcut)
saqut lsp                → LSP sunucu modu (yapılacak)
saqut dap                → DAP hata ayıklama modu (yapılacak)
```

Ayrı binary **yok**. Her mod `main.cpp`'de `cli.registerCommand(...)` ile
kayıtlıdır. `src/lsp/` ve `src/dap/` bağımsız dizinler; CMakeLists.txt'e
glob veya elle eklenir.

---

## AŞAMA 1 — TextMate Grameri

### Ne Değişir?

**C++ kodu değişmez.** Tamamen editör tarafı, iki dosya.

### Yapılacaklar

#### 1.1 Dizin Oluştur

```
editor/
  vscode/
    package.json
    extension.ts                      (Faz 3'te doldurulur — şimdi iskelet)
    language-configuration.json
    syntaxes/
      sqt.tmLanguage.json
    snippets/
      sqt.json                        (opsiyonel)
```

#### 1.2 `language-configuration.json`

```json
{
  "comments": {
    "lineComment": "//"
  },
  "brackets": [
    ["{", "}"],
    ["(", ")"],
    ["[", "]"]
  ],
  "autoClosingPairs": [
    { "open": "{", "close": "}" },
    { "open": "(", "close": ")" },
    { "open": "[", "close": "]" },
    { "open": "\"", "close": "\"" }
  ],
  "surroundingPairs": [
    ["{", "}"],
    ["(", ")"],
    ["[", "]"],
    ["\"", "\""]
  ],
  "indentationRules": {
    "increaseIndentPattern": "\\{\\s*$",
    "decreaseIndentPattern": "^\\s*\\}"
  },
  "onEnterRules": [
    {
      "beforeText": "^\\s*\\/\\/.*$",
      "action": { "indent": "none", "appendText": "// " }
    }
  ]
}
```

#### 1.3 `syntaxes/sqt.tmLanguage.json`

Anahtar kelimeler `src/tokenizer/tokenizer.cpp` `KW_MAP`'ten ve
`src/parser/token.hpp` `KEYWORD_MAP`'ten okunarak güncel tutulur.

```json
{
  "$schema": "https://raw.githubusercontent.com/martinring/tmlanguage/master/tmlanguage.json",
  "name": "saQut",
  "scopeName": "source.sqt",
  "fileTypes": ["sqt"],
  "patterns": [
    { "include": "#comments" },
    { "include": "#strings" },
    { "include": "#keywords_control" },
    { "include": "#keywords_type" },
    { "include": "#keywords_module" },
    { "include": "#keywords_other" },
    { "include": "#constants" },
    { "include": "#numbers" },
    { "include": "#operators" }
  ],
  "repository": {
    "comments": {
      "patterns": [
        {
          "name": "comment.line.double-slash.sqt",
          "match": "//.*$"
        },
        {
          "name": "comment.block.sqt",
          "begin": "/\\*",
          "end": "\\*/"
        }
      ]
    },
    "strings": {
      "name": "string.quoted.double.sqt",
      "begin": "\"",
      "end": "\"",
      "patterns": [
        { "name": "constant.character.escape.sqt", "match": "\\\\." }
      ]
    },
    "keywords_control": {
      "name": "keyword.control.sqt",
      "match": "\\b(if|else|for|while|do|return|break|continue|switch|case|default|try|catch|throw)\\b"
    },
    "keywords_type": {
      "name": "storage.type.sqt",
      "match": "\\b(int|float|double|bool|string|void|decimal|char|auto)\\b"
    },
    "keywords_module": {
      "name": "keyword.other.sqt",
      "match": "\\b(import|export|from|struct|enum|as)\\b"
    },
    "keywords_other": {
      "name": "keyword.other.sqt",
      "match": "\\b(new|class|interface|extends|implements|const|extern|static|final)\\b"
    },
    "constants": {
      "name": "constant.language.sqt",
      "match": "\\b(true|false|null)\\b"
    },
    "numbers": {
      "patterns": [
        {
          "name": "constant.numeric.hex.sqt",
          "match": "\\b0[xX][0-9a-fA-F]+\\b"
        },
        {
          "name": "constant.numeric.binary.sqt",
          "match": "\\b0[bB][01]+\\b"
        },
        {
          "name": "constant.numeric.float.sqt",
          "match": "\\b[0-9]+\\.[0-9]+([eE][+-]?[0-9]+)?\\b"
        },
        {
          "name": "constant.numeric.integer.sqt",
          "match": "\\b[0-9]+\\b"
        }
      ]
    },
    "operators": {
      "name": "keyword.operator.sqt",
      "match": "(\\+\\+|--|\\+=|-=|\\*=|/=|%=|==|!=|<=|>=|&&|\\|\\||[+\\-*/%<>=!&|^~])"
    }
  }
}
```

#### 1.4 `package.json` (şimdilik minimal)

```json
{
  "name": "saqut",
  "displayName": "saQut Language",
  "version": "0.1.0",
  "engines": { "vscode": "^1.75.0" },
  "categories": ["Programming Languages"],
  "contributes": {
    "languages": [{
      "id": "sqt",
      "aliases": ["saQut"],
      "extensions": [".sqt"],
      "configuration": "./language-configuration.json"
    }],
    "grammars": [{
      "language": "sqt",
      "scopeName": "source.sqt",
      "path": "./syntaxes/sqt.tmLanguage.json"
    }]
  },
  "main": "./extension.js"
}
```

#### 1.5 `extension.ts` (şimdilik iskelet)

```typescript
import * as vscode from 'vscode';
export function activate(_ctx: vscode.ExtensionContext) {}
export function deactivate() {}
```

### Başarı Kriteri

`.sqt` dosyası VS Code'da açıldığında anahtar kelimeler, string'ler ve
yorumlar doğru renkleniyorsa TextMate aşaması tamamdır.

---

## AŞAMA 2 — LSP

### Mevcut Kodun Durumu

| Bileşen | Durum | LSP İçin Not |
|---------|-------|-------------|
| `SourceLocation` | `line`/`column` **1-tabanlı** | LSP 0-tabanlı ister — adapter gerekli |
| `DiagnosticEngine` | `toJsonObj()` mevcut | LSP formatına çeviren adapter eklenir |
| `Symbol.definitionLoc` | Dolu | `textDocument/definition` hazır |
| `Symbol.references` | Dolu | `textDocument/references` hazır |
| `Symbol.type` | Dolu | `textDocument/hover` hazır |
| `SymbolTable.resolve()` | Mevcut | `textDocument/completion` için kullanılır |
| `ModuleGraph` | Mevcut | Çok-dosya desteği hazır |

### C++ Tarafında Değişecekler

#### 2.1 `src/core/location.hpp` — adapter ekle

```cpp
// Mevcut struct SourceLocation'a eklenecek:
struct LspPosition {
    int line;       // 0-tabanlı
    int character;  // 0-tabanlı
};

struct LspRange {
    LspPosition start;
    LspPosition end;
};

// SourceLocation içine metod:
LspPosition toLspPosition() const {
    return { line > 0 ? line - 1 : 0,
             column > 0 ? column - 1 : 0 };
}
```

#### 2.2 `src/diagnostic/diagnostic_engine.hpp` — LSP format

```cpp
// DiagnosticEngine'e eklenecek metod:
nlohmann::json toLspDiagnostics() const {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& d : diagnostics_) {
        nlohmann::json item;
        auto pos = d.loc.toLspPosition();
        item["range"] = {
            {"start", {{"line", pos.line}, {"character", pos.character}}},
            {"end",   {{"line", pos.line}, {"character", pos.character + 1}}}
        };
        item["severity"] = (d.level == DiagLevel::Error) ? 1 : 2;
        item["code"]    = d.code;
        item["message"] = d.hint.empty() ? d.message
                                         : d.message + "\n" + d.hint;
        item["source"]  = "saQut";
        arr.push_back(item);
    }
    return arr;
}
```

#### 2.3 Yeni dizin: `src/lsp/`

**`src/lsp/lsp_types.hpp`** — LSP JSON-RPC temel yapıları:

```cpp
#ifndef SAQUT_LSP_TYPES
#define SAQUT_LSP_TYPES

#include "vendor/nlohmann/json.hpp"
#include <string>

// JSON-RPC mesaj zarfı
struct JsonRpcMessage {
    std::string jsonrpc = "2.0";
    nlohmann::json id;          // null → notification
    std::string method;
    nlohmann::json params;
    nlohmann::json result;
    nlohmann::json error;
    bool isNotification() const { return id.is_null(); }
};

#endif
```

**`src/lsp/json_rpc.hpp/.cpp`** — stdin/stdout okuma/yazma:

```cpp
// json_rpc.hpp
class JsonRpc {
public:
    // "Content-Length: N\r\n\r\n{...}" formatında oku
    static nlohmann::json readMessage(std::istream& in);

    // Aynı formatta yaz
    static void writeMessage(std::ostream& out, const nlohmann::json& msg);

    // Cevap üret
    static nlohmann::json makeResponse(const nlohmann::json& id,
                                       const nlohmann::json& result);
    // Bildirim üret
    static nlohmann::json makeNotification(const std::string& method,
                                           const nlohmann::json& params);
    // Hata cevabı üret
    static nlohmann::json makeError(const nlohmann::json& id,
                                    int code, const std::string& msg);
};
```

`readMessage` şablonu (LSP Header protokolü):

```cpp
nlohmann::json JsonRpc::readMessage(std::istream& in) {
    int contentLength = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line == "\r" || line.empty()) break;
        if (line.rfind("Content-Length:", 0) == 0)
            contentLength = std::stoi(line.substr(16));
    }
    if (contentLength <= 0) return nullptr;
    std::string body(contentLength, '\0');
    in.read(body.data(), contentLength);
    return nlohmann::json::parse(body);
}
```

**`src/lsp/document_store.hpp`** — açık belgeler önbelleği:

```cpp
#ifndef SAQUT_LSP_DOCUMENT_STORE
#define SAQUT_LSP_DOCUMENT_STORE

#include <string>
#include <unordered_map>
#include <memory>
#include "parser/ast_node.hpp"
#include "symbol/symbol_table.hpp"
#include "diagnostic/diagnostic_engine.hpp"
#include "tokenizer/tokenizer.hpp"

struct DocumentState {
    std::string      uri;
    std::string      content;
    int              version = 0;
    ASTNode*         ast     = nullptr;
    SymbolTable      symbolTable;
    DiagnosticEngine diagnostics;
    std::vector<Token*> tokens;

    ~DocumentState() {
        delete ast;
        for (auto* t : tokens) delete t;
    }
};

class DocumentStore {
public:
    // Belgeyi aç veya güncelle; pipeline'ı yeniden çalıştır
    DocumentState& update(const std::string& uri,
                          const std::string& content, int version);

    // URI ile belgeyi al; yoksa nullptr
    DocumentState* get(const std::string& uri);

    // Belgeyi kapat
    void close(const std::string& uri);

private:
    void runPipeline(DocumentState& state, const std::string& filePath);

    std::unordered_map<std::string, std::unique_ptr<DocumentState>> store_;
};

#endif
```

`runPipeline` gövdesi (document_store.cpp):

```cpp
void DocumentStore::runPipeline(DocumentState& state,
                                const std::string& filePath) {
    state.diagnostics = DiagnosticEngine{};
    delete state.ast;
    for (auto* t : state.tokens) delete t;

    Tokenizer tokenizer;
    state.tokens = tokenizer.scan(state.content, filePath);

    Parser parser;
    state.ast = parser.parse(state.tokens);
    if (!state.ast) return;

    state.symbolTable = SymbolTable{};
    SymbolCollector(state.symbolTable, state.diagnostics).collect(state.ast);
    if (!state.diagnostics.hasErrors()) {
        TypeChecker(state.symbolTable, state.diagnostics).check(state.ast);
        StructuralValidator(state.diagnostics).validate(state.ast);
    }
}
```

**`src/lsp/lsp_handler.hpp/.cpp`** — LSP metodlarını yönetir:

Öncelik sırasına göre implemente edilecek metodlar:

```
Tier 0 (zorunlu):
  initialize            → server capabilities döndür
  initialized           → (notification, boş cevap)
  shutdown              → (boş cevap, çıkışa hazırlan)
  exit                  → process çık
  textDocument/didOpen  → DocumentStore.update() + publishDiagnostics
  textDocument/didChange → DocumentStore.update() + publishDiagnostics
  textDocument/didClose → DocumentStore.close()

Tier 1 (hemen ardından):
  textDocument/definition       → Symbol.definitionLoc
  textDocument/references       → Symbol.references
  textDocument/hover            → Symbol.type + kind
  textDocument/documentHighlight → Symbol.references (aynı dosyada)

Tier 2 (sonra):
  textDocument/completion       → scope'taki semboller
  textDocument/signatureHelp    → fonksiyon parametre listesi
  textDocument/rename           → tüm referanslara uygula
  textDocument/documentSymbol   → dosya outline

Tier 3 (en son):
  textDocument/semanticTokens/full → TypeChecker sonrası zengin renk
```

`initialize` cevabı örneği (capabilities):

```cpp
nlohmann::json capabilities = {
    {"textDocumentSync", 1},   // 1 = Full sync
    {"definitionProvider",   true},
    {"referencesProvider",   true},
    {"hoverProvider",        true},
    {"documentHighlightProvider", true},
    // Tier 2 eklenince bunlar true yapılır:
    // {"completionProvider", {{"triggerCharacters", {"."}}}},
    // {"renameProvider", true},
};
```

**`src/lsp/lsp_server.hpp/.cpp`** — ana döngü:

```cpp
void LspServer::run() {
    while (std::cin.good()) {
        auto msg = JsonRpc::readMessage(std::cin);
        if (msg.is_null()) continue;

        std::string method = msg["method"];
        auto response = handler_.dispatch(msg);

        if (!response.is_null())
            JsonRpc::writeMessage(std::cout, response);
    }
}
```

**`src/cli/commands/lsp.hpp`** — main.cpp'ye register edilecek:

```cpp
#ifndef SAQUT_CLI_LSP
#define SAQUT_CLI_LSP
#include "cli/args.hpp"
#include "lsp/lsp_server.hpp"

inline int cmdLsp(const CliArgs&) {
    LspServer server;
    server.run();
    return 0;
}
#endif
```

**`src/main.cpp`'e eklenecek:**

```cpp
#include "cli/commands/lsp.hpp"
// ...
cli.registerCommand({"lsp",
    "start LSP server (JSON-RPC on stdin/stdout)",
    false, cmdLsp});
```

### VS Code Eklentisi — LSP Tarafı (`extension.ts`)

```typescript
import * as vscode from 'vscode';
import { LanguageClient, LanguageClientOptions,
         ServerOptions } from 'vscode-languageclient/node';

let client: LanguageClient;

export function activate(ctx: vscode.ExtensionContext) {
    const serverOptions: ServerOptions = {
        command: 'saqut',
        args: ['lsp']
    };
    const clientOptions: LanguageClientOptions = {
        documentSelector: [{ scheme: 'file', language: 'sqt' }]
    };
    client = new LanguageClient('saQut', 'saQut Language Server',
                                 serverOptions, clientOptions);
    ctx.subscriptions.push(client.start());
}

export function deactivate(): Thenable<void> | undefined {
    return client?.stop();
}
```

`package.json`'a eklenecek:

```json
"activationEvents": ["onLanguage:sqt"],
"dependencies": {
    "vscode-languageclient": "^8.0.0"
}
```

### Başarı Kriteri

1. `.sqt` dosyası açıldığında Problems panelinde hatalar görünüyor.
2. Bir sembol üzerinde F12 → tanıma gidiyor.
3. Bir sembol üzerinde hover → tip bilgisi görünüyor.

---

## AŞAMA 3 — DAP

### Kritik Önkoşul: IR lineTable

**Bu olmadan DAP başlatılamaz.** Şu an `Instruction`'da `sourceLine`/`sourceCol`
var ama **yalnızca 9/93 emit çağrısı** bu alanları dolduruyor (`ir_generator.cpp`'de
grep edildi). Tüm emit çağrılarının `loc` bilgisini taşıması gerekir.

#### 3.1 IR lineTable'ı tamamla

**`src/ir/instruction.hpp`** — `sourceLine`/`sourceCol` zaten var, `sourceFile` ekle:

```cpp
struct Instruction {
    // ... mevcut alanlar ...
    int         sourceLine = 0;
    int         sourceCol  = 0;
    std::string sourceFile;   // YENİ — hangi .sqt dosyası
};
```

**`src/ir/ir_generator.cpp`** — tüm `emit*` fonksiyonlarına `loc` geçir:

Şu an örnek pattern:

```cpp
// ÖNCEKİ (loc eksik):
void IRGenerator::emitLoadConst(int destSlot, int value) {
    Instruction ins;
    ins.opcode   = Opcode::LOAD_CONST;
    ins.dest     = destSlot;
    ins.intValue = value;
    currentFunction_->instructions.push_back(ins);
}

// SONRASI (loc eklendi):
void IRGenerator::emitLoadConst(int destSlot, int value,
                                const SourceLocation& loc) {
    Instruction ins;
    ins.opcode     = Opcode::LOAD_CONST;
    ins.dest       = destSlot;
    ins.intValue   = value;
    ins.sourceLine = loc.line;
    ins.sourceCol  = loc.column;
    ins.sourceFile = loc.filePath;
    currentFunction_->instructions.push_back(ins);
}
```

Tüm `emit*` fonksiyonları bu şekilde güncellenir. `generateExpression` ve
`generateStatement` çağrı noktalarında ilgili AST düğümünün `loc`'u geçilir.

### VM'e Eklenecekler

**`src/vm/interpreter.hpp`** — yeni API:

```cpp
class Interpreter {
public:
    // Mevcut
    int run();

    // DAP API — YENİ
    enum class RunState { Running, Paused, Finished };

    void  setBreakpoint(const std::string& file, int line);
    void  clearBreakpoint(const std::string& file, int line);
    void  clearAllBreakpoints();

    RunState  state() const { return state_; }
    void      resume();
    void      stepInstruction();
    void      stepOver();

    int         currentSourceLine() const;
    std::string currentSourceFile() const;
    int         callDepth() const;
    std::string frameFunctionName(int depth) const;
    int         frameSourceLine(int depth) const;

    // Değişken okuma
    Value readSlotInFrame(int frameDepth, int slotIndex) const;
    // slot index → değişken adı (SymbolTable üzerinden)
    std::string slotName(int frameDepth, int slotIndex) const;

private:
    RunState state_ = RunState::Running;
    std::set<std::pair<std::string,int>> breakpoints_;  // {file, line}

    bool isBreakpoint() const;
    void checkBreakpoint();   // her instruction öncesi çağrılır

    // Mevcut
    std::vector<CallFrame> frames_;
    // ...
};
```

**`src/vm/interpreter.cpp`** — ana döngü değişikliği:

```cpp
int Interpreter::run() {
    while (/* talimat var */) {
        checkBreakpoint();      // breakpoint mi? → state_ = Paused, döngüden çık

        if (state_ == Paused) {
            return -1;          // DAP bir sonraki komuta kadar bekler
        }

        execute(currentInstruction());
        advance();
    }
    state_ = Finished;
    return exitCode_;
}

void Interpreter::resume() {
    state_ = Running;
    run();
}

void Interpreter::stepInstruction() {
    state_ = Running;
    // Tek instruction çalıştır, sonra durdur
    execute(currentInstruction());
    advance();
    state_ = Paused;
}
```

### Yeni dizin: `src/dap/`

**`src/dap/dap_types.hpp`** — DAP temel yapıları:

```cpp
struct DapBreakpoint {
    int         id;
    bool        verified;
    int         line;
    std::string source;
};

struct DapStackFrame {
    int         id;
    std::string name;       // fonksiyon adı
    std::string sourceFile;
    int         line;
};

struct DapVariable {
    std::string name;
    std::string value;
    std::string type;
    int         variablesReference;  // 0 = yaprak, >0 = genişletilebilir (struct/array)
};
```

**`src/dap/dap_handler.hpp/.cpp`** — implemente edilecek metodlar:

```
Tier 0 (zorunlu):
  initialize          → capabilities döndür
  launch              { program: "main.sqt" }
                        → derle + VM'i hazırla + stopped@entry gönder
  setBreakpoints      → VM.setBreakpoint() çağır, verified listesi döndür
  continue            → VM.resume()
  threads             → tek thread: [{ id: 1, name: "main" }]
  disconnect          → VM'i temizle

Tier 1 (hemen ardından):
  stackTrace          → VM.callDepth() üzerinden DapStackFrame listesi
  scopes              → her frame için "Locals" scope
  variables           → VM.readSlotInFrame() + slotName() ile değişkenler

Tier 2 (sonra):
  stepOver            → VM.stepOver()
  stepIn              → VM.stepInstruction() (fonksiyon içine girer)
  stepOut             → frame bitene kadar çalıştır
  evaluate            → (watch expressions) basit identifier değerlendirme
```

`launch` cevabı akışı:

```cpp
// dap_handler.cpp — launch handler
void DapHandler::handleLaunch(const nlohmann::json& args) {
    std::string program = args["program"];

    // 1. Derle
    ModuleRegistry registry;
    DiagnosticEngine diag;
    ModuleGraph graph = ModuleLoader(registry, diag).load(program);
    SymbolTable symbolTable;
    SymbolCollector(symbolTable, diag).collectModuleGraph(graph);
    // ... TypeChecker ...

    if (diag.hasErrors()) {
        // output event ile hataları bildir
        sendEvent("output", {{"category","stderr"},
                              {"output", "Build failed\n"}});
        sendEvent("terminated", {});
        return;
    }

    IRGenerator irgen;
    IRProgram irProgram = irgen.generateModuleGraph(graph, symbolTable);

    // 2. VM'i oluştur, stopOnEntry ile durdur
    vm_ = std::make_unique<Interpreter>(irProgram);
    vm_->stepInstruction();   // ilk instruction'da durdur

    // 3. Editöre "stopped" olayı gönder
    sendEvent("stopped", {{"reason","entry"}, {"threadId",1}});
}
```

**`src/dap/dap_server.hpp/.cpp`** — JSON-RPC döngüsü (LSP ile aynı yapı):

```cpp
void DapServer::run() {
    while (std::cin.good()) {
        auto msg = JsonRpc::readMessage(std::cin);
        if (msg.is_null()) continue;
        auto response = handler_.dispatch(msg);
        if (!response.is_null())
            JsonRpc::writeMessage(std::cout, response);
    }
}
```

**`src/cli/commands/dap.hpp`:**

```cpp
#ifndef SAQUT_CLI_DAP
#define SAQUT_CLI_DAP
#include "cli/args.hpp"
#include "dap/dap_server.hpp"

inline int cmdDap(const CliArgs&) {
    DapServer server;
    server.run();
    return 0;
}
#endif
```

**`src/main.cpp`'e eklenecek:**

```cpp
#include "cli/commands/dap.hpp"
// ...
cli.registerCommand({"dap",
    "start DAP debug adapter (JSON-RPC on stdin/stdout)",
    false, cmdDap});
```

### VS Code Eklentisi — DAP Tarafı (`package.json`)

```json
"debuggers": [{
    "type": "sqt",
    "label": "saQut Debugger",
    "languages": ["sqt"],
    "configurationAttributes": {
        "launch": {
            "required": ["program"],
            "properties": {
                "program": {
                    "type": "string",
                    "description": "Çalıştırılacak .sqt dosyasının yolu",
                    "default": "${file}"
                },
                "stopOnEntry": {
                    "type": "boolean",
                    "default": true
                }
            }
        }
    },
    "initialConfigurations": [{
        "type": "sqt",
        "request": "launch",
        "name": "saQut: Dosyayı Çalıştır",
        "program": "${file}",
        "stopOnEntry": true
    }]
}]
```

`extension.ts`'e eklenecek:

```typescript
const debugFactory: vscode.DebugAdapterDescriptorFactory = {
    createDebugAdapterDescriptor(_session) {
        return new vscode.DebugAdapterExecutable('saqut', ['dap']);
    }
};
ctx.subscriptions.push(
    vscode.debug.registerDebugAdapterDescriptorFactory('sqt', debugFactory)
);
```

### Başarı Kriteri

1. F5 → program başlar, ilk satırda durur.
2. Satır numarasına tıkla → breakpoint koyulur, program o satırda durur.
3. Variables panelinde lokal değişkenler ve değerleri görünür.
4. F10 (Step Over) ile satır satır ilerlenebilir.

---

## Özet: Dosya Değişim Tablosu

### Mevcut kodda değişecekler

| Dosya | Ne değişir |
|-------|-----------|
| `src/core/location.hpp` | `toLspPosition()` metodu eklenir |
| `src/diagnostic/diagnostic_engine.hpp` | `toLspDiagnostics()` metodu eklenir |
| `src/ir/instruction.hpp` | `sourceFile: std::string` alanı eklenir |
| `src/ir/ir_generator.cpp` | Tüm `emit*` fonksiyonları `loc` alır; ~84 çağrı güncellenir |
| `src/vm/interpreter.hpp` | Breakpoint/step/slot-okuma API eklenir |
| `src/vm/interpreter.cpp` | Ana döngüye `checkBreakpoint()` eklenir |
| `src/main.cpp` | `cmdLsp`, `cmdDap` register edilir |

### Yeni dosyalar

| Dosya | İçerik |
|-------|--------|
| `src/lsp/lsp_types.hpp` | JSON-RPC mesaj yapısı |
| `src/lsp/json_rpc.hpp/.cpp` | stdin/stdout Content-Length protokolü |
| `src/lsp/document_store.hpp/.cpp` | Açık belgeler + pipeline önbelleği |
| `src/lsp/lsp_handler.hpp/.cpp` | LSP metod dispatch |
| `src/lsp/lsp_server.hpp/.cpp` | Ana JSON-RPC döngüsü |
| `src/lsp/semantic_tokens.hpp` | Token → delta-encode (Tier 3) |
| `src/cli/commands/lsp.hpp` | `cmdLsp` |
| `src/dap/dap_types.hpp` | DAP Breakpoint/StackFrame/Variable |
| `src/dap/dap_handler.hpp/.cpp` | DAP metod dispatch |
| `src/dap/dap_server.hpp/.cpp` | Ana JSON-RPC döngüsü |
| `src/cli/commands/dap.hpp` | `cmdDap` |
| `editor/vscode/package.json` | VS Code eklenti tanımı |
| `editor/vscode/extension.ts` | LSP istemci + DAP kayıt |
| `editor/vscode/language-configuration.json` | Bracket/yorum/girinti |
| `editor/vscode/syntaxes/sqt.tmLanguage.json` | TextMate grameri |

### Değişmeyenler

`Parser`, `SymbolCollector`, `TypeChecker`, `SymbolTable`, `ModuleLoader`,
`ModuleGraph`, `IRGenerator` — bunlara **dokunulmaz**.

---

## Bağımlılık Sırası

```
TextMate grameri    → Bağımlılık yok. İlk başlanacak yer.
        ↓
LSP Tier 0          → location.hpp + diagnostic_engine.hpp + src/lsp/ altyapısı
        ↓
LSP Tier 1          → Tier 0 + Symbol bilgileri (hazır)
        ↓
VS Code Eklentisi   → TextMate + LSP Tier 0
        ↓
IR lineTable        → ir_generator.cpp tüm emit'ler (DAP için önkoşul)
        ↓
VM step/breakpoint  → IR lineTable + interpreter.cpp değişiklikleri
        ↓
DAP Tier 0          → VM API + src/dap/ altyapısı
        ↓
DAP Tier 1          → DAP Tier 0 + değişken okuma
```
