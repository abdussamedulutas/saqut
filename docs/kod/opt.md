# Optimizasyon (`src/opt/`)

## Sorumluluk

AST üzerinde çalışan optimizasyon pasajlarını yönetir. İki pasaj içerir:
**ConstantFoldingPass** (sabit katlama) ve **DeadCodeElimPass** (ölü kod eleme).
ADR-007/009 kapsamında: orijinal AST klonlanır, dönüşüm klon üzerinde yapılır;
analiz orijinal AST üstünde annotation olarak kalır.

## Dosya envanteri

| Dosya | Rol |
|-------|-----|
| `optimization_manager.hpp` | `OptimizationManager` — pasajları `CompilerConfig`'e göre seçer ve fixpoint döngüsünde çalıştırır. |
| `optimization_pass.hpp` | `OptimizationPass` soyut temel sınıfı — `run()` + `name()` arayüzü. |
| `ast_clone.hpp` | `deepClone()` — AST derin kopyalama (parent pointer remap). |
| `constant_folding.hpp` | `ConstantFoldingPass` — binary/unary literal ifadeleri derleme zamanında hesaplar. |
| `dead_code_elim.hpp` | `DeadCodeElimPass` — terminator sonrası erişilemez kodları siler. |

## Ana tipler ve ilişkileri

```
OptimizationPass (abstract)
  ├─ run(root, table) → bool  — true = değişiklik yapıldı
  └─ name() → string

ConstantFoldingPass : OptimizationPass
  ├─ diag_   : DiagnosticEngine&
  ├─ changed_: bool
  ├─ fold(node) → ASTNode* — bottom-up (önce çocuklar, sonra düğüm)
  ├─ isIntLit / isScalarLit
  ├─ getIntVal / getScalarVal
  ├─ canFoldOp / computeOp  (binary: +-*/% == != < > <= >= & | << >> && ||)
  └─ canFoldUnary / computeUnary (! ~ - +)

DeadCodeElimPass : OptimizationPass
  ├─ diag_   : DiagnosticEngine&
  ├─ changed_: bool
  └─ visit(node) — top-down, Block'ta terminator sonrasını siler (W003)

OptimizationManager
  ├─ passes_ : vector<unique_ptr<OptimizationPass>>
  ├─ maxRounds_ : int (varsayılan 10)
  ├─ runPassesInPlace(root, table) — yerinde dönüşüm
  └─ optimize(root, table) — önce deepClone(), sonra runPassesInPlace()

deepClone(node) → ASTNode*
  └─ recursive: her ASTKind için özel case, parent pointer remap
     IdentifierNode::resolvedSymbol → orijinal sembol tablosuna read-only referans
```

## Veri akışı

```
AST (Parser/SymbolCollector/TypeChecker çıktısı)
       ↓
  OptimizationManager::optimize(root, table)
       ├─ deepClone() → klon AST
       └─ runPassesInPlace(klon, table)
            └─ fixpoint döngüsü (maxRounds_)
                 ├─ ConstantFoldingPass::run()
                 │    └─ fold() bottom-up → LITERAL+LITERAL = LITERAL
                 └─ DeadCodeElimPass::run()
                      └─ visit() top-down → terminator sonrasını sil (W003)
       ↓
  Optimize edilmiş klon (IRGenerator'a gider)
  Orijinal AST korunur (ast komutu öncesi/sonrası karşılaştırma)
```

## Diğer modüllerle temas

| Modül | İlişki |
|-------|--------|
| Core (config) | `CompilerConfig` hangi pasajların aktif olduğunu belirler. |
| Parser | AST düğüm tipleri (ASTKind, LiteralNode, BinaryExpressionNode vb.). |
| Symbol | `SymbolTable` — pasajlara iletilir (şu an kullanılmıyor, ileride kullanılabilir). |
| Diagnostic | W002 (sıfıra bölme), W003 (erişilemez kod) uyarıları. |
| IRGenerator | Optimize edilmiş AST'yi alır. |

## Tasarım kararları

- **Analiz vs Optimizasyon ayrımı** (ADR-007): Analiz orijinal AST'de annotation,
  optimizasyon klon üzerinde dönüşüm. `ast` komutu öncesi/sonrası gösterir.
- **Fixpoint döngüsü** (ADR-009): Her pasaj yalnızca küçülten dönüşümler yapar
  (n düğüm → 1 düğüm veya silme). Hiçbir pasaj değişiklik yapmazsa döngü kırılır.
- **deepClone parent remap**: Klon ağaçta parent pointer'lar yeni düğümlere bağlanır.
  `IdentifierNode::resolvedSymbol` orijinal sembol tablosuna read-only referans
  olarak kalır (performans).
- **ConstantFoldingPass**: Yalnızca integer sabitleri katlanır; float/decimal
  henüz kapsam dışı. `computeOp` 18 binary operatörü destekler.
- **DeadCodeElimPass**: Block'ta ilk return/break/continue/throw sonrasını siler.
  `isReachable=false` + `W003` uyarısı. `remove_if` + `erase` ile bellekten silinir.
- **Sıfıra bölme** (W002): ConstantFoldingPass içinde tespit edilir; katlama
  yapılmaz, orijinal ifade korunur.
- **Pass arayüzü** (`OptimizationPass`): 2 saf virtual metod (`run`, `name`).
  Yeni bir pasaj eklemek için bu sınıftan türetmek ve `optimization_manager.hpp`
  yapıcısına eklemek yeterlidir.

## Bilinen sınırlar / TODO

- Float/decimal sabit katlama henüz yok (yalnızca integer).
- `UnaryExpression` (ASTKind) yerine BinaryExpression(left=nullptr) ile unary
  işlem yapılır — ayrı bir düğüm tipi olabilir.
- DeadCodeElimPass `isReachable` bayrağını kullanır; bu bayrak TypeChecker
  tarafından da set edilebilir (çakışma olmamalı).
- `deepClone`'daki varsayılan case bilinmeyen node tipini olduğu gibi döndürür
  (güvenli değil).
