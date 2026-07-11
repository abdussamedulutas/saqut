# Sembol Sistemi (`src/symbol/`)

## Sorumluluk

AST üzerinde 3 geçişli (3-pass) sembol toplama işlemini yürütür. Tüm bildirimleri
(fonksiyon, struct, enum, değişken, import) kaydeder, tipleri çözümler, kapsam
(scope) hiyerarşisini kurar ve fonksiyon gövdelerindeki isimleri tanımlarına
bağlar. Çok dosyalı (multi-module) derlemeyi destekler.

## Dosya envanteri

| Dosya | Rol |
|-------|-----|
| `symbol.hpp` | `SymbolKind` enum, `Symbol` struct'ı — bir ismin tüm metaverisi (ad, tür, tip, konum, referanslar, scope). |
| `scope.hpp` | `Scope` sınıfı — kapsam hiyerarşisi, lexical scoping (`resolve` parent zincirini tırmanır). |
| `symbol_table.hpp` | `SymbolTable` sınıfı — `Symbol` ömrü yönetimi (`pool_`), `define`/`resolve`, `structLayouts`/`enumLayouts` haritaları. |
| `symbol_collector.hpp` | `SymbolCollector` sınıf bildirimi — 3 geçişli toplama algoritması. |
| `symbol_collector.cpp` | SymbolCollector gerçeklemesi — pass1a, pass1b, pass2, `seedBuiltins`, `typeFromName`, `checkStructCycles`, `validateImports`, `walkStmt`/`walkExpr`. |

## Ana tipler ve ilişkileri

```
SymbolKind: Variable | Function | Parameter | Struct | Field | Enum | EnumValue

Symbol
  ├─ name          : string
  ├─ kind          : SymbolKind
  ├─ type          : Type (core/type.hpp)
  ├─ moduleId      : int (ModuleRegistry ID: -1=main, 0=builtin)
  ├─ definitionLoc : SourceLocation
  ├─ references    : vector<SourceLocation> — tüm kullanım noktaları
  ├─ scope         : Scope* — ait olduğu kapsam
  ├─ isBuiltin     : bool
  └─ paramNames    : vector<string> — yalnızca Function türünde

Scope
  ├─ parent  : Scope* (null = global)
  ├─ table   : unordered_map<string, Symbol*> — non-owning pointer'lar
  ├─ order   : vector<Symbol*> — ekleme sırası
  ├─ defineLocal(s) → Symbol* (duplicate → nullptr)
  ├─ lookupLocal(n) → Symbol* (sadece bu scope)
  └─ resolve(n) → Symbol* (parent zincirini tırmanır, lexical scoping)

SymbolTable
  ├─ global_  : Scope* — en üst kapsam
  ├─ current_ : Scope* — şu anki kapsam
  ├─ pool_    : vector<unique_ptr<Symbol>> — sahiplik
  ├─ scopes_  : vector<unique_ptr<Scope>> — sahiplik
  ├─ structLayouts : unordered_map<string, vector<pair<string, Type>>>
  ├─ enumLayouts   : unordered_map<string, vector<pair<string, int>>>
  ├─ define(name, kind, type, loc, moduleId) → Symbol*
  ├─ resolve(n) → Symbol*
  └─ enterScope() / exitScope()

SymbolCollector
  ├─ table_ : SymbolTable&
  ├─ diag_  : DiagnosticEngine&
  ├─ collect(program) — tek dosya (geriye dönük uyumluluk)
  ├─ collectModuleGraph(graph) — çok dosyalı
  ├─ seedBuiltins() — print + Error struct'ı
  ├─ pass1aRegisterNames() — tip isimlerini kaydet
  ├─ pass1bResolveLayouts() — struct/fonksiyon imzalarını çöz
  ├─ validateImports() — import doğrulama
  ├─ pass2Bodies() → walkStmt() / walkExpr() — gövde çözümleme
  └─ checkStructCycles() — döngü kontrolü (DFS)
```

## Veri akışı (3 geçiş)

```
AST (Parser çıktısı)
       ↓
  pass1aRegisterNames()  — struct/enum adlarını + fonksiyon stub'larını kaydet
       ↓
  pass1bResolveLayouts() — struct alan tiplerini ve fonksiyon imzalarını çöz
       ↓
  checkStructCycles()    — döngüsel struct tespiti (E010)
       ↓
  validateImports()      — export edilmiş mi? isim var mı? (çok dosyalı)
       ↓
  pass2Bodies() → walkStmt()/walkExpr() — identifier'ları resolve() ile bağla
       ↓
  SymbolTable (dolu) → TypeChecker | IRGenerator | LSP
```

## Diğer modüllerle temas

| Modül | İlişki |
|-------|--------|
| Parser | `ASTNode*` (ProgramNode) girdi olarak alır. |
| Semantic (TypeChecker) | `SymbolTable::structLayouts`/`enumLayouts` ve `resolve()` ile tip/sembol bilgisine erişir. |
| IRGenerator | `SymbolTable::allSymbols()`, `structLayouts`, `enumLayouts` ile sembol bilgisini IR'ye taşır. |
| Module (ModuleGraph) | `collectModuleGraph()` çok dosyalı derleme için ModuleGraph alır. |
| Diagnostic | E001 (tanımsız), E002 (çift tanım), E007 (bilinmeyen tip), E010 (döngüsel struct) hatalarını üretir. |
| LSP | `allSymbols()` ile documentSymbol, definition, references, completion sağlanır. |

## Tasarım kararları

- **3 geçişli algoritma**: (1a) tip isimlerini kaydet → (1b) layout çözümle →
  (2) gövde çözümleme. Bu, struct alanının başka bir struct tipine referans
  vermesine izin verir (ileriye referans, forward reference).
- **`Symbol` struct (sınıf değil)**: Tüm alanlar public; doğrudan erişilir.
  Ömür yönetimi `SymbolTable::pool_` tarafından `unique_ptr` ile yapılır;
  diğer modüller `Symbol*` (non-owning) kullanır.
- **Lexical scoping**: `Scope::resolve()` parent zincirini yukarı tırmanır.
  İç blok dış bloktaki değişkeni görebilir (C/Java modeli).
- **`defineLocal` duplicate kontrolü**: Aynı isimde sembol varsa `nullptr`
  döner; çağıran (SymbolTable::define) E002 hatası üretebilir.
- **`structLayouts` / `enumLayouts`**: Sembol tablosunun içinde yaşar.
  Struct alan sırası ve enum üye değerleri burada korunur. TypeChecker
  ve IRGenerator doğrudan okur.
- **Builtin'ler (`seedBuiltins`)**: `print` fonksiyonu ve `Error` struct'ı
  (ADR-025) global scope'a eklenir. moduleId=0 (BUILTIN_ID). TODO(#89):
  ileride gerçek builtin kataloğundan yüklenecek.
- **Import doğrulama**: `collectModuleGraph` içinde, tüm pass1'ler
  tamamlandıktan sonra yapılır. Kaynak modülde export edilmemiş isim
  E_SYMBOL_NOT_IMPORTED hatası verir.

## Bilinen sınırlar / TODO

- `seedBuiltins()` geçicidir — TODO(#89) ile belirtilmiş; ileride
  BuiltinMethodRegistry'den yüklenecek.
- `print` fonksiyonu şu an parametresiz/void olarak kaydedilir; gerçek
  imzası farklı olabilir.
- Struct döngü kontrolü (`checkStructCycles`) yalnızca doğrudan alan
  tiplerini kontrol eder; dolaylı döngüler (A→B→A) de tespit edilir.
- `collect()` (tek dosya) geriye dönük uyumluluk içindir; yeni kod
  `collectModuleGraph()` kullanmalıdır.
