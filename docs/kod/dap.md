# Hata Ayıklama — DAP (`src/dap/`)

## Sorumluluk

Debug Adapter Protocol (DAP) uygulaması. stdio üzerinden JSON-RPC mesajları
alarak VM'i breakpoint, adım adım çalıştırma, değişken görüntüleme ve
stacktrace hizmetleriyle kontrol eder. Faz 5/6 kapsamında IR satır tablosu,
slot adları ve gerçek isimler eklendi.

## Dosya envanteri

| Dosya | Rol |
|-------|-----|
| `dap_handler.hpp` / `.cpp` | `DapHandler` — tüm DAP isteklerini dispatch eder, event'leri gönderir. |
| `dap_server.hpp` | `DapServer` — stdio üzerinden DAP mesajlarını okur/yazar. |
| `dap_types.hpp` | DAP tip tanımları (StackFrame, Scope, Variable, Breakpoint, Thread). |

## Ana tipler

```
DapHandler
  ├─ out_          : ostream&
  ├─ irProgram_    : unique_ptr<IRProgram>  — debug edilecek program
  ├─ vm_           : unique_ptr<Interpreter> — VM örneği
  ├─ nextBpId_     : int — breakpoint ID sayacı
  ├─ nextVarRef_   : int — variablesReference sayacı (100000'den başlar)
  ├─ varRefs_      : unordered_map<int, Value> — değişken referans kaydı
  ├─ initialized_  : bool
  ├─ responseSeq_  : int — mesaj sıra numarası
  │
  ├─ dispatch(msg) → json
  ├─ handleInitialize / Launch / SetBreakpoints / ConfigurationDone
  ├─ handleContinue / Next / StepIn / StepOut / Pause
  ├─ handleThreads / StackTrace / Scopes / Variables / Evaluate
  ├─ handleTerminate / Disconnect
  ├─ makeResponse() / sendEvent()
  └─ DAP yaşam döngüsü: initialize → initialized event → launch →
     setBreakpoints → configurationDone → VM çalışır → ...

VM (Interpreter) DAP API:
  ├─ initForDebug()
  ├─ setBreakpoint / clearBreakpoint / clearAllBreakpoints
  ├─ resume() / stepInstruction() / stepOver() / stepLine() / stepOut()
  ├─ runUntilEvent(maxInstructions, startCallDepth) → RunReason
  ├─ currentSourceLine() / currentSourceFile()
  ├─ callDepth()
  ├─ frameSourceLine/Name/File/SlotCount(depth)
  ├─ readSlotInFrame(depth, slot) → Value
  └─ slotName(depth, slot) → string
```

## Desteklenen Özellikler (DAP)

- `initialize` / `initialized` event — protokol başlatma
- `launch` / `configurationDone` — debug oturumu
- `setBreakpoints` — satır bazlı breakpoint
- `continue` / `next` (stepOver) / `stepIn` / `stepOut`
- `threads` — tek thread (main)
- `stackTrace` — çağrı yığını (per-frame sourceFile + slotCount)
- `scopes` — frame'in scope bilgisi
- `variables` — slot değerleri (gerçek değişken adlarıyla)
- `evaluate` — ifade değerlendirme
- `terminate` / `disconnect`

## Tasarım kararları

- **DAP protokolü doğru format**: initialize + initialized event sırası,
  launch → configurationDone yaşam döngüsü, response-önce-event-sonra.
- **Gerçek değişken adları**: `IRFunction::slotNames` + `Interpreter::slotName()`
  ile slot numarası yerine değişken adı gösterilir.
- **variablesReference kaydı**: Her frame/slot için unique ref üretilir;
  `nextVarRef_` 100000'den başlar (scope ref'leriyle çakışmamak için).
  Her resume'de eski ref'ler geçersizleşir (DAP spec).
- **Breakpoint eşlemesi**: `IRFunction::lineToFirstIP` ile satır→instruction
  eşlemesi. Dosya yolu eşleşmesi TODO(faz6).
- **Özel ScopeCall (evaluate)**: evaluate handler'ı, basit ifadeler için
  Interpreter üzerinden değerlendirme yapar.
