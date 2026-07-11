# Semantik Analiz (`src/semantic/`)

## Sorumluluk

AST üzerinde semantik (anlamsal) doğrulama yapan iki bileşenden oluşur:
**TypeChecker** (tip denetimi ve tip çıkarımı) ve **StructuralValidator**
(yapısal kural denetimi). Pipeline'da Faz 3'te, sembol toplamadan sonra çalışır.

## Dosya envanteri

| Dosya | Rol |
|-------|-----|
| `type_checker.hpp` | `TypeChecker` sınıf bildirimi — tip denetimi ve tip çıkarımının ana girişi. |
| `type_checker.cpp` | TypeChecker gerçeklemesi (~989 satır). `check()`, `checkExpr()`, `checkStmt()`, `checkFunction()`, `checkAssign()`. |
| `structural_validator.hpp` | `StructuralValidator` sınıf bildirimi — yapısal kural denetimi. |
| `structural_validator.cpp` | StructuralValidator gerçeklemesi — break/continue/return bağlamı, iç-içe bildirim yasağı. |

## Ana tipler ve ilişkileri

```
TypeChecker
  ├─ table_   : SymbolTable&  — sembol çözümleme için
  ├─ diag_    : DiagnosticEngine& — hata/uyarı raporlama
  ├─ currentReturnType_ : Type — aktif fonksiyonun dönüş tipi
  ├─ inFunction_ : bool
  ├─ narrowedNonNull_ : unordered_set<string> — ADR-021 null daraltma
  │
  ├─ check(program) — giriş noktası
  ├─ checkFunction(fnNode) — fonksiyon tip denetimi + pathAlwaysReturns
  ├─ checkStmt(node) — ifade/statement denetimi
  ├─ checkExpr(node, expected) → Type — ifade tip çıkarımı (~480 satır)
  ├─ checkAssign(target, src, ...) → bool — atama/parametre uyumu
  ├─ numericRank(t) → int — sayısal genişlik sırası (int=0, float=1, double=2, decimal=3)
  ├─ extractNullCheck(cond) → {varName, isNotNull} — null kontrol kalıbı
  ├─ alwaysExits(stmt) → bool — return/throw/break/continue?
  └─ pathAlwaysReturns(stmt) → bool — tüm yollar return ile bitiyor mu?

StructuralValidator
  ├─ diag_          : DiagnosticEngine&
  ├─ loopDepth_     : int — döngü+switch derinliği (break için)
  ├─ pureLoopDepth_ : int — yalnızca döngü derinliği (continue için)
  └─ inFunction_    : bool
       ├─ validate(program) — giriş noktası
       ├─ walkDecl(node) — bildirim gezintisi
       └─ walkStmt(node) — ifade gezintisi + yapısal kontroller
```

## Veri akışı

```
AST (Parser çıktısı) → SymbolCollector → SymbolTable (dolu)
                                              ↓
  StructuralValidator::validate(program)
       ├─ walkDecl() → FunctionDecl bulunca inFunction_=true
       └─ walkStmt() → loopDepth_/pureLoopDepth_/inFunction_ kontrolleri
                        (E004: break/continue, E005: return, E011: iç-içe bildirim)
                                              ↓
  TypeChecker::check(program)
       ├─ FunctionDecl → checkFunction()
       │    ├─ dönüş tipi çözümleme
       │    ├─ checkStmt() → gövde denetimi
       │    │    ├─ VariableDecl → checkAssign() (E003, W004)
       │    │    ├─ ReturnStatement → checkAssign() (E006)
       │    │    ├─ TryStatement → catch/throw denetimi (ADR-025)
       │    │    ├─ SwitchStatement → tip homojenliği, float W005 (ADR-027)
       │    │    └─ IfStatement → ADR-021 null narrowing
       │    │         ├─ "a != null" → narrowedNonNull_.insert("a")
       │    │         └─ "a == null" + alwaysExits → narrowedNonNull_
       │    └─ pathAlwaysReturns() → E006 (return eksik)
       │
       └─ checkExpr(node, expected) → Type
            ├─ Literal → bağlama göre tip (ADR-010/ADR-028)
            ├─ Identifier → resolvedSymbol.type (null daraltma dahil)
            ├─ BinaryExpression → atama/aritmetik/mantıksal (ADR-021 operand kontrolü)
            ├─ Call → argüman sayısı/tipi (E008)
            ├─ MemberAccess → struct/enum alan çözümleme (ADR-021 nullable kontrolü)
            ├─ IndexExpression → array elementType
            ├─ ScopeCall → BuiltinMethodRegistry araması
            ├─ CastExpression → as dönüşümü (ADR-026)
            └─ Postfix ++/-- → numeric kontrol
```

## Diğer modüllerle temas

| Modül | İlişki |
|-------|--------|
| Symbol | `SymbolTable` üzerinden sembol çözümleme, struct/enum layout erişimi. |
| Parser | `ASTNode*` girdi olarak alır, `ASTKind` ile düğüm tiplerini ayırt eder. |
| Diagnostic | E001, E003, E004, E005, E006, E008, E011, W004, W005 hata/uyarılarını üretir. |
| Builtin | `BuiltinMethodRegistry` — ScopeCall (E::method) tip denetimi için. |
| Optimizasyon | `resolvedType` ve `isConstant` alanlarını kullanır. |

## Tasarım kararları

- **Gizli tip dönüşümü YOK** (ADR-010): `checkAssign()` tüm atama/parametre
  geçişlerini denetler. Yalnızca literal → daha geniş tip sessiz geçer (W004
  uyarısı). Değişken → değişken daraltma her durumda E003 hatasıdır.
- **Nullable tipler ve null narrowing** (ADR-021): `narrowedNonNull_` akış-duyarlı
  küme ile yönetilir. `extractNullCheck()` if koşulundan "a != null" / "a == null"
  kalıbını ayrıştırır. `alwaysExits()` ile guard pattern desteklenir
  ("if (a == null) return;" sonrası a non-null).
- **Literal genişletme** (ADR-010, ADR-028): `checkExpr`'de `expected` parametresi
  ile literal tipi bağlama göre belirlenir. `int x = 1` → Int; `float x = 1` → Float;
  `decimal x = 1` → Decimal.
- **String birleştirme** (ADR-024): BinaryExpression `+` operatöründe her iki
  operand string ise String döndürülür. Bool dışı numeric operasyonlar ayrıdır.
- **Float switch case uyarısı** (ADR-027): IEEE 754'te tam temsil edilemeyen
  float değerleri W005 uyarısı üretir.
- **break/continue bağlamı**: StructuralValidator `loopDepth_` ve `pureLoopDepth_`
  sayaçları ile yönetilir. break switch içinde de geçerlidir (`loopDepth_`),
  continue yalnızca döngüde (`pureLoopDepth_`).
- **Non-void return kontrolü**: `pathAlwaysReturns()` recursive olarak tüm akış
  yollarını kontrol eder. IfStatement'te else yoksa false döner (if atlanabilir).
- **Error tipi sessiz atlama**: `Error` tipi ile karşılaşan kontroller (checkAssign,
  checkExpr) sessizce geçer — önceki hatanın ardışık sahte hata üretmesi önlenir.

## Bilinen sınırlar / TODO

- `checkExpr` ~480 satırla en karmaşık fonksiyondur; bölünmesi düşünülebilir.
- `narrowedNonNull_` yalnızca aynı kapsamda geçerlidir; iç içe bloklarda narrowing
  bilgisi taşınmaz (taşınması gerekebilir).
- `extractNullCheck()` yalnızca doğrudan `BinaryExpression(==/!=, identifier, null)`
  kalıbını tanır; `&&` ile zincirleme narrowing sınırlıdır.
- IndexExpression'da (dizi indeksi) tip çıkarımı tam değil — bilinmeyen
  durumda varsayılan Int döner.
- `pathAlwaysReturns()` throw'u return ile eşdeğer kabul eder, ancak catch
  bloğu throw'u yakalayabilir — bu durum modellenmemiştir.
