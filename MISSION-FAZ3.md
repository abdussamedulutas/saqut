# MISSION — Faz 3: Semantik Analiz (Gitea #72)

> **Bu dosya kendi kendine yeter.** "MISSION-FAZ3'ü oku ve yap" denince
> doğrudan uygula. Bitince DOĞRULAMA bölümünü çalıştır, #72'ye tamamlanma
> yorumu düş, bu dosyayı sil veya MISSION-FAZ4.md ile değiştir.
> `Co-Authored-By: Claude Opus 4.8` trailer'ı unutma.

---

## 0) HEDEF & BAŞARI KRİTERLERİ (#72)

İki modül: **TypeChecker** (her ExpressionNode'a tip ata) +
**StructuralValidator** (kontrol akışı kuralları).

Bitti tanımı:
1. `build/saqut check file:examples/fibonacci.sqt` → 0 hata 0 uyarı; her
   expression node'unun `resolvedType` alanı JSON'da dolu.
2. `examples/semantic/` test fixture'ları doğru hata/uyarı üretiyor.
3. `-Wall -Wextra` temiz. Faz 0/1/2 testleri hâlâ geçer.

---

## 1) DÖNÜŞÜM KURALLARI (ADR-010 özeti)

| Durum | Sonuç | Kod |
|---|---|---|
| `float x = 1` | Geçerli — literal 1.0 sayılır | — |
| `double x = 1` | Geçerli | — |
| `float x = intVar` | **Uyarı** — örtük genişletme, veri kaybı yok | W004 |
| `double x = intVar` | **Uyarı** | W004 |
| `double x = floatVar` | **Uyarı** | W004 |
| `int x = 1.5` | **Hata** — literal'den veri kaybı | E003 |
| `int x = floatVar` | **Hata** — daraltma | E003 |
| `float x = doubleVar` | **Hata** — daraltma | E003 |
| `int x = intVar` | Geçerli | — |

**Kural özeti:**
- Literal bağlama-göre tiplenir: hedef tip daha geniş veya aynıysa uyarısız geçer.
- Değişken→değişken: genişletme (widening) = W004, daraltma (narrowing) = E003.
- Aynı tip = her zaman geçerli.

**Yardımcı fonksiyon (type_checker içinde):**
```cpp
enum class ConvResult { Ok, Warn_W004, Error_E003 };
ConvResult checkAssignCompat(const Type& target, const Type& src, bool srcIsLiteral);
```

---

## 2) KATALOG GÜNCELLEMESİ

`src/diagnostic/diagnostic.hpp` → W004 ekle:
```cpp
{"W004", DiagLevel::Warning, "Örtük sayısal genişletme (widening)"},
```

---

## 3) DOSYA DOSYA PLAN

Yeni dizin: `src/semantic/`. CMake GLOB_RECURSE → yeni `.cpp`'ler otomatik
alınır ama `build.sh` ile derle (cmake yeniden config eder).

---

### 3.1 `src/semantic/type_checker.hpp`

```cpp
#ifndef SAQUT_SEMANTIC_TYPE_CHECKER
#define SAQUT_SEMANTIC_TYPE_CHECKER

#include "symbol/symbol_table.hpp"
#include "diagnostic/diagnostic_engine.hpp"
#include "parser/ast_node.hpp"
#include "core/type.hpp"

class TypeChecker {
public:
    TypeChecker(SymbolTable& table, DiagnosticEngine& diag)
        : table_(table), diag_(diag) {}

    void check(ASTNode* program);

private:
    // İfadeyi gez, resolvedType ata, tipi döndür.
    // expectedType: bağlam tipi (literal genişletme için); boş = bilinmiyor.
    Type checkExpr(ASTNode* node, const Type& expected = Type::error());

    // Deyimi gez (statement'lar tip döndürmez).
    void checkStmt(ASTNode* node);

    // Fonksiyon gövdesini gez; dönüş tipi bağlamını stack'te tut.
    void checkFunction(ASTNode* fnNode);

    // Atama uyumunu denetle; uyarı/hata raporla. true = geçerli.
    bool checkAssign(const Type& target, const Type& src,
                     bool srcIsLiteral, const SourceLocation& loc,
                     const std::string& context);

    SymbolTable&      table_;
    DiagnosticEngine& diag_;

    // Aktif fonksiyonun beklenen dönüş tipi (iç içe fonksiyon yok, stack
    // yerine tek değer yeterli).
    Type currentReturnType_;
    bool inFunction_ = false;
};

#endif // SAQUT_SEMANTIC_TYPE_CHECKER
```

---

### 3.2 `src/semantic/type_checker.cpp`

`check(program)`:
- Program children'ı gez: FunctionDecl → `checkFunction`, VariableDecl →
  `checkStmt`, StructDecl → atla.

`checkFunction(fnNode)`:
- `inFunction_ = true; currentReturnType_ = typeFromReturnType(fn->returnType);`
- `checkStmt(body)` (body = children[0])
- `inFunction_ = false;`

`checkStmt(node)` — switch(node->kind):
- **Block**: her child → `checkStmt`.
- **VariableDecl**: initExpr varsa `checkExpr(initExpr, typeFromName(vd->varType))`;
  sonucu target tiple `checkAssign` ile denetle.
- **ExpressionStatement**: `checkExpr(es->expression)`.
- **ReturnStatement**: value varsa `checkExpr(value, currentReturnType_)`;
  sonucu `checkAssign(currentReturnType_, valueType, isLiteral, loc, "return")`.
  Değer yok ve returnType != void → E006.
- **IfStatement**: condition checkExpr (bool beklenir, ama şimdilik herhangi
  sayısal tip de geçerli — E003 değil, TODO Faz3+); then/else checkStmt.
- **WhileStatement / DoWhileStatement**: condition checkExpr; body checkStmt.
- **ForStatement**: init checkStmt; condition checkExpr; update checkExpr; body checkStmt.
- **Break / Continue / ExpressionStatement**: içerik checkExpr.
- **ReturnStatement**: yukarıda.

`checkExpr(node, expected)` — switch(node->kind) → Type döndürür:
- **Literal(INTEGER)**:
  - expected sayısal ve daha geniş veya aynıysa → `expected` tipini döndür
    (literal 1.0 olarak tiplenir, uyarısız).
  - expected int veya error → `Type::Int()`.
  - expected float literal'e uymuyor (ör. expected=int, literal=FLOAT) → E003.
- **Literal(FLOAT)**: expected double ise double döndür (kayıpsız); expected int → E003;
  aksi → `Type::Float()`.
- **Literal(BOOLEAN)**: `Type::Bool()`.
- **Literal(STRING)**: `Type::String()`.
- **Literal(BOŞ)**: `Type::error()` (null — Faz 3+ kapsam dışı).
- **Identifier**: `id->resolvedSymbol ? id->resolvedSymbol->type : Type::error()`.
  Type::error() ise E001 zaten Faz 2'de raporlandı, sessiz geç.
- **BinaryExpression**:
  - Atama operatörü (EQUAL, +=, -=, vb.):
    - left = `checkExpr(left)`, right = `checkExpr(right, leftType)`.
    - `checkAssign(leftType, rightType, isLiteralRight, loc, "atama")`.
    - `resolvedType = leftType`.
  - Aritmetik (+, -, *, /, %):
    - Her iki operandı `checkExpr` et.
    - İkisi de sayısal ve aynı tip → sonuç o tip.
    - İkisi sayısal farklı tip → `checkAssign` mantığıyla uyar/hata; sonuç
      daha geniş tip (veya E003 durumunda error).
    - Sayısal değil → E003, `Type::error()`.
  - Karşılaştırma (==, !=, <, <=, >, >=):
    - Operandları checkExpr; uyumlu sayısal veya aynı tip → `Type::Bool()`.
    - Uyumsuz → E003, `Type::Bool()` (hata yayılmasın, bool olarak devam).
  - Mantıksal (&&, ||):
    - Her iki operand bool beklenir; değilse E003.
    - Sonuç `Type::Bool()`.
  - Unary prefix (-, +, !):
    - Left = nullptr; Right = operand.
    - `-`/`+`: sayısal beklenir; `!`: bool beklenir.
  - resolvedType'a sonucu ata.
- **Call**:
  - callee = `checkExpr(callee)` → Function tipi beklenir.
  - Argüman sayısı imzayla eşleşmeli → E008.
  - Her argüman: `checkExpr(arg, paramType[i])` → `checkAssign(paramType, argType, ...)`.
  - resolvedType = callee tipi's `returnType`.
- **Postfix (++/--)**: operand sayısal → sonuç aynı tip; değilse E003.
- **MemberAccess**: `Type::error()` + TODO (struct alan çözümü Faz 3+).
- **IndexExpression**: TODO; `Type::error()`.

`checkAssign(target, src, srcIsLiteral, loc, ctx)`:
- target veya src = error → `return true` (sessiz geç, hata zaten raporlandı).
- target == src → `return true`.
- Genişletme (widening):
  - `int → float`: srcIsLiteral → geçerli (**uyarısız**); değil → **W004**.
  - `int → double`, `float → double`: aynı kural.
  - srcIsLiteral ve kayıpsız → `return true`.
  - srcIsLiteral değil → W004 raporla, `return true` (yine de geçerli).
- Daraltma (narrowing):
  - `float → int`, `double → int`, `double → float` → **E003**, `return false`.
- Tamamen farklı tipler (int ↔ string, vb.) → **E003**, `return false`.

> `srcIsLiteral`: `node->kind == ASTKind::Literal` ise true.

---

### 3.3 `src/semantic/structural_validator.hpp`

```cpp
#ifndef SAQUT_SEMANTIC_STRUCTURAL_VALIDATOR
#define SAQUT_SEMANTIC_STRUCTURAL_VALIDATOR

#include "diagnostic/diagnostic_engine.hpp"
#include "parser/ast_node.hpp"

class StructuralValidator {
public:
    StructuralValidator(DiagnosticEngine& diag) : diag_(diag) {}
    void validate(ASTNode* program);

private:
    void walkDecl(ASTNode* node);
    void walkStmt(ASTNode* node);

    DiagnosticEngine& diag_;
    int  loopDepth_  = 0;
    bool inFunction_ = false;
};

#endif // SAQUT_SEMANTIC_STRUCTURAL_VALIDATOR
```

---

### 3.4 `src/semantic/structural_validator.cpp`

`validate(program)`: her top-level child → `walkDecl`.

`walkDecl`:
- FunctionDecl: `inFunction_=true`, `walkStmt(body)`, `inFunction_=false`.
- VariableDecl / StructDecl: atla.

`walkStmt(node)` — switch:
- **Block**: her child `walkStmt`.
- **IfStatement**: then/else `walkStmt`.
- **WhileStatement / DoWhileStatement**: `loopDepth_++; walkStmt(body); loopDepth_--;`
- **ForStatement**: `loopDepth_++; walkStmt(init?); walkStmt(body); loopDepth_--;`
- **BreakStatement**: `loopDepth_ == 0` → E004.
- **ContinueStatement**: `loopDepth_ == 0` → E004.
- **ReturnStatement**: `!inFunction_` → E005.
  (Return tip uyumu TypeChecker'ın işi; burada sadece yapısal kural.)
- **ExpressionStatement / VariableDecl**: `walkStmt` çocukları için yinele.
- Diğer: atla.

---

## 4) CLI — `saqut check` KOMUTU

### 4.1 `src/cli/commands/check.hpp`

```cpp
inline int cmdCheck(const CliArgs& args) {
    // tokenize → parse → symbolCollect → typeCheck → structValidate
    // Tüm hatalar DiagnosticEngine'de toplanır.
    // Çıktı: JSON (--compact destekli)
    // {"file":"...","diagnostics":{...}}
    // exit code: hasErrors() ? 1 : 0
}
```

### 4.2 `src/cli/cli.hpp` veya `main.cpp`

`check` komutu kaydet; `parseArgs` tanımlı komutlar listesine ekle.

---

## 5) TEST FIXTURE'LARI — `examples/semantic/`

| Dosya | İçerik | Beklenen |
|---|---|---|
| `widening.sqt` | `float x = 1;` (OK) + `float y = someInt;` | W004 |
| `narrowing.sqt` | `int x = 1.5;` | E003 |
| `bad_return.sqt` | `int foo() { return 1.5; }` | E003 |
| `break_outside.sqt` | top-level `break;` | E004 |
| `return_outside.sqt` | top-level `return 0;` | E005 |
| `bad_args.sqt` | `fibonacci(1, 2)` (1 parametre bekliyor) | E008 |

---

## 6) DOĞRULAMA KOMUTLARI

```bash
bash scripts/build.sh

# Başarı: 0 hata 0 uyarı
build/saqut check file:examples/fibonacci.sqt

# Her expression tipli mi?
build/saqut ast file:examples/fibonacci.sqt | python3 -c "
import json,sys
def walk(n):
    if 'resolvedType' in n and n['resolvedType'] is None:
        print('UNTIPPED:', n.get('kind'), n.get('name',''))
    for v in n.values():
        if isinstance(v, dict): walk(v)
        elif isinstance(v, list): [walk(i) for i in v if isinstance(i, dict)]
walk(json.load(sys.stdin))"

# Hata fixture'ları
build/saqut check file:examples/semantic/widening.sqt      # W004
build/saqut check file:examples/semantic/narrowing.sqt     # E003
build/saqut check file:examples/semantic/break_outside.sqt # E004
build/saqut check file:examples/semantic/bad_args.sqt      # E008

# Regresyon
bash tests/run.sh
build/saqut ast file:examples/fibonacci.sqt | python3 -m json.tool >/dev/null && echo AST-OK
```

---

## 7) HAFIZA İPUÇLARI

- `ASTKind::BinaryExpression` → `BinaryExpressionNode{Left, Right, Operator(TokenType)}`
- Atama operatörleri: `EQUAL, PLUS_EQUAL, MINUS_EQUAL, STAR_EQUAL, SLASH_EQUAL, ...`
- `ASTKind::Call` → `CallExpressionNode{callee, arguments}`
- `ASTKind::Literal` → `LiteralNode{literalType(INTEGER/FLOAT/...), parserToken}`
- `ASTKind::Identifier` → `IdentifierNode{resolvedSymbol}` (Faz 2'de set edildi)
- `FunctionDeclNode{name, returnType(string), params, children=[body]}`
- `Type::fromName(str)` primitifi çözer; `Type::function(ret, params)` fonksiyon tipi
- `diag_.report("E003", loc, "mesaj")` → katalogdan seviye otomatik çözülür
- `resolvedType` → `ExpressionNode`'un alanı (ast_node.hpp)
- Literal'ın literal tipi: `((LiteralNode*)node)->literalType`
- İki sayısal tipin genişliği: int < float < double (basit sıralama yeterli)
