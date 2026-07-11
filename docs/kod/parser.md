# Ayrıştırıcı (`src/parser/` + `src/parser/nodes/`)

## Sorumluluk

Tokenizer'dan gelen token listesini Pratt parsing algoritması ile Soyut Sözdizimi
Ağacına (AST) dönüştürür. Pipeline'da Katman 3 olarak yer alır. Sözdizimi
hatalarında E9xx kodlu tanılar üretir ve panic-mode recovery ile hatanın
ötesindeki kodun parse edilmeye devam etmesini sağlar.

## Dosya envanteri

| Dosya | Rol |
|-------|-----|
| `parser.hpp` | Facade header — `parser_base.hpp`'i include eder. Dış modüllere açılan arayüz. |
| `parser_base.hpp` | `Parser` sınıfının bildirimi — token navigasyonu, Pratt ayrıştırma, hata raporlama. |
| `parser.cpp` | Parser metodlarının tam gerçeklemesi (~1154 satır). |
| `ast.hpp` | Umbrella header — tüm AST düğüm sınıflarını tek noktada toplar. |
| `ast_node.hpp` | `ASTNode` taban sınıfı, `ASTKind`/`LiteralType` enum'ları, `ExpressionNode`/`StatementNode` tabanları. |
| `ast_json.hpp` | `JsonObject` — AST'nin JSON serileştirmesi için builder pattern. |
| `token.hpp` | `TokenType` enum, `ParserToken`, `KEYWORD_MAP`/`OPERATOR_MAP`, öncelik tablosu (`TokenPrecedence`). |
| `nodes/program.hpp` / `.cpp` | `ProgramNode` — AST kök düğümü. |
| `nodes/declarations.hpp` / `.cpp` | `FunctionDeclNode`, `StructDeclNode`, `EnumDeclNode`, `VariableDeclNode`, `ImportDeclNode`. |
| `nodes/statements.hpp` / `.cpp` | `BlockNode`, `IfStatementNode`, `ForStatementNode`, `WhileStatementNode`, `DoWhileStatementNode`, `ReturnStatementNode`, `BreakStatementNode`, `ContinueStatementNode`, `TryStatementNode`, `ThrowStatementNode`, `SwitchStatementNode`, `ExpressionStatementNode`. |
| `nodes/expressions.hpp` / `.cpp` | `CallExpressionNode`, `PostfixNode`, `MemberAccessNode`, `IndexExpressionNode`, `ScopeCallNode`, `ArrayLiteralNode`, `CastExpressionNode`, `TernaryExpressionNode`. |
| `nodes/binary_expr.hpp` / `.cpp` | `BinaryExpressionNode` — ikili işlem düğümü (18 öncelik seviyesi). |
| `nodes/literal.hpp` / `.cpp` | `LiteralNode` — sabit değer (integer, float, string, bool, null). |
| `nodes/identifier.hpp` / `.cpp` | `IdentifierNode` — değişken/fonksiyon adı. |
| `nodes/error_node.hpp` / `.cpp` | `ErrorNode` — sözdizimi hatası düğümü, panic-mode recovery çıktısı. |

## Ana tipler ve ilişkileri

```
Parser
  ├─ tokens: TokenList (vector<Token*>)
  ├─ current: int — token indeksi
  ├─ diag_: DiagnosticEngine* — opsiyonel (Faz 2)
  ├─ parse(tokens) → ProgramNode
  ├─ parseDeclaration() → FunctionDecl | StructDecl | EnumDecl | VariableDecl | ImportDecl
  ├─ parseStatement() → Block | If | For | While | DoWhile | Return | Break | Continue | Try | Throw | Switch | ExpressionStmt
  └─ parseExpression(precedence) → ASTNode  ← Pratt algoritması
       ├─ parseNullDenotation()  — prefix/primary (sayı, string, identifier, unary, parantez, array literal, scope call)
       └─ parseLeftDenotation()  — infix/postfix (binary op, çağrı, indeks, üye erişimi, as, ternary)

ASTNode (abstract)
  ├─ kind: ASTKind — switch/case ile tip kontrolü (dynamic_cast yok)
  ├─ parent: ASTNode* — yukarı doğru gezinme (semantic analiz için)
  ├─ loc: SourceLocation — token konumu
  ├─ children: vector<ASTNode*>
  ├─ log() / toJson() — polimorfik çıktı
  ├─ addChild() / getChildren()
  └─ ~ASTNode() — children'ı özyinelemeli siler

ASTKind enum (~30 değer):
  Program | ImportDecl | FunctionDecl | StructDecl | EnumDecl | VariableDecl |
  Block | If/ElseIf/Else | For | While | DoWhile | Return | Break | Continue |
  Try | Catch | Finally | Throw | Switch | Case |
  Binary | Unary | Postfix | Call | MemberAccess | Index | Literal | Identifier |
  ScopeCall | ArrayLiteral | Cast | Ternary |
  ErrorNode

ExpressionNode (ASTNode altı)
  ├─ resolvedType: Type — tip denetleyici tarafından doldurulacak
  └─ isConstant: bool — sabit katlama için

StatementNode (ASTNode altı)
  └─ isReachable: bool — ölü-kod analizi için

ParserToken (köprü yapı)
  ├─ token: Token* — ham token
  ├─ type: TokenType — anlamsal tip (~100+ değer)
  └─ getPowerOperator() → öncelik seviyesi (0-18)

JsonObject (builder pattern)
  ├─ add(key, value) — string/sayı/bool
  ├─ addRaw(key, json) — ham JSON gömme
  ├─ addArray(key, callback) — dizi oluşturma
  └─ str() → JSON stringi
```

## Veri akışı

```
Tokenizer::scan() → vector<Token*>
       ↓
  Parser::parse(tokens)
       ├─ parseProgram() (sonsuz döngü)
       │    └─ parseDeclaration() → FunctionDecl | StructDecl | ...
       │         └─ parseBlock() / parseStatement()
       │              └─ parseExpression(precedence)  ← Pratt
       │                   ├─ parseNullDenotation()
       │                   └─ parseLeftDenotation()
       ↓
  ProgramNode (AST kökü)
       ↓
  SymbolCollector | TypeChecker | IRGenerator  (sonraki katmanlar)
```

## Diğer modüllerle temas

| Modül | İlişki |
|-------|--------|
| Tokenizer | `vector<Token*>` girdi olarak alır. |
| Symbol (sembol toplayıcı) | AST'yi gezer, sembolleri toplar. |
| Semantic (tip denetleyici) | AST düğümlerindeki `resolvedType` alanını doldurur, `ASTKind` ile düğüm tiplerini kontrol eder. |
| Optimizasyon | AST klonu üstünde dönüşüm yapar (`clone()` ile). |
| IRGenerator | AST'yi dolaşarak IR talimatları üretir. |
| Diagnostic | `diag_` üzerinden E9xx sözdizimi hatalarını raporlar (opsiyonel). |
| LSP | Sözdizimi hataları `publishDiagnostics` ile gönderilir. |

## Tasarım kararları

- **Pratt parsing** (Top-Down Operator Precedence): 100+ token tipi, 18 öncelik
  seviyesi. `parseNullDenotation()` prefix/primary işler, `parseLeftDenotation()`
  infix/postfix işler. Her operatörün önceliği ve birleşme yönü tek merkezden
  (`token.hpp` -> `TokenPrecedence`/`RightAssociative`) yönetilir.
- **enum class ASTKind ile tip kontrolü**: `dynamic_cast` yerine switch/case
  kullanılır. Yeni düğüm tipi eklemek derleyiciden "eksik case" uyarısı alır —
  `dynamic_cast`'ten daha güvenlidir.
- **Parent pointer**: `ASTNode::parent` semantic analizde kapsam bulmak için
  kullanılır (`parent->parent->...` yukarı çıkma).
- **Hata düğümü (ErrorNode)**: Sözdizimi hatasında E9xx tanısı üretilir,
  panic-mode recovery (`synchronizeAndMakeError`) bilinen bir sınıra (`;`, `}`,
  statement-başlangıcı, EOF) kadar atlar ve ErrorNode döndürür. Sonraki
  katmanlar (SymbolCollector, TypeChecker) ErrorNode'u switch/default ile sessizce
  atlar — AST'nin geri kalanı analiz edilmeye devam eder.
- **DiagnosticEngine* opsiyonel**: Parser iki modda çalışabilir — `DiagnosticEngine*`
  verilmezse hatalar `std::cerr`'e yazılır (CLI komutları), verilirse konumlu
  E9xx tanıları üretilir (LSP/IDE modu).
- **JsonObject builder pattern**: Her AST düğümü kendi `toJson()`'unda JsonObject
  kullanarak JSON üretir. `addArray(callback)` ile alt düğümler eklenir.
- **TokenType ↔ ham token ayrımı**: Tokenizer yalnızca ham token'lar üretir;
  anlamsal dönüşüm (`parseToken()`) Parser tarafından yapılır. Yeni keyword
  eklemek için yalnızca `token.hpp`'deki `KEYWORD_MAP` güncellenir.

## Bilinen sınırlar / TODO

- `resolvedType` (ExpressionNode) henüz tüm düğümlerde dolu değil — tip denetleyici
  tarafından doldurulacak (faz-3).
- `isConstant` (ExpressionNode) ve `isReachable` (StatementNode) bayrakları
  tanımlanmış ama henüz tüm senaryolarda kullanılmıyor (faz-4).
- `ASTNode::loc` (SourceLocation) şu anda tüm düğümlerde dolu değil.
- `JsonObject::m_arrayDepth` alanı tanımlanmış ama henüz kullanılmıyor
  (ileride çok boyutlu diziler için).
- Operatör tablosuna yeni operatör eklemek için: `token.hpp` (TokenType, öncelik,
  harita) + `parser_base.hpp` (parseNullDenotation/parseLeftDenotation) +
  `ast_node.hpp` (ASTKind) güncellenmelidir.
