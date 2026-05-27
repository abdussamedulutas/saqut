// ============================================================================
// saQut Compiler — Soyut Sözdizim Ağacı (Aggregator)
// ============================================================================
//
// DİZİN:   src/parser/ast.hpp
// KATMAN:  Katman 3 — Parser'ın ürettiği, IR'nin tükettiği
//
// Bu dosya bir AGGREGATOR'dür. Tüm AST düğüm sınıflarını tek bir include
// ile kullanılabilir yapar.
//
// AST DÜĞÜM HİYERARŞİSİ:
//   ASTNode (soyut taban) — ast_node.hpp
//   ├── ProgramNode            : Kök düğüm          — ast_decl.hpp
//   ├── FunctionDeclNode       : Fonksiyon tanımı    — ast_decl.hpp
//   ├── StructDeclNode         : struct tanımı       — ast_decl.hpp
//   ├── VariableDeclNode       : Değişken tanımı     — ast_decl.hpp
//   ├── BlockNode              : { ... } bloğu       — ast_stmt.hpp
//   ├── IfStatementNode        : if/else             — ast_stmt.hpp
//   ├── WhileStatementNode     : while               — ast_stmt.hpp
//   ├── ForStatementNode       : for                 — ast_stmt.hpp
//   ├── DoWhileStatementNode   : do-while            — ast_stmt.hpp
//   ├── ReturnStatementNode    : return               — ast_stmt.hpp
//   ├── BreakStatementNode     : break               — ast_stmt.hpp
//   ├── ContinueStatementNode  : continue            — ast_stmt.hpp
//   ├── ExpressionStatementNode: expression;          — ast_stmt.hpp
//   ├── BinaryExpressionNode   : a + b               — ast_expr.hpp
//   ├── LiteralNode            : 42, "hello"         — ast_expr.hpp
//   ├── IdentifierNode         : değişken ismi       — ast_expr.hpp
//   ├── PostfixNode            : a++                 — ast_expr.hpp
//   ├── CallExpressionNode     : f(x)                — ast_expr.hpp
//   ├── MemberAccessNode       : a.b                 — ast_expr.hpp
//   └── IndexExpressionNode    : a[i]                — ast_expr.hpp
//
// ============================================================================

#ifndef SAQUT_AST
#define SAQUT_AST

#include "parser/ast_node.hpp"
#include "parser/ast_json.hpp"
#include "parser/ast_expr.hpp"
#include "parser/ast_stmt.hpp"
#include "parser/ast_decl.hpp"

#endif // SAQUT_AST
