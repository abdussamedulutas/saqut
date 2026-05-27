// ============================================================================
// saQut Compiler — Parser (Aggregator)
// ============================================================================
//
// DİZİN:   src/parser/parser.hpp
// KATMAN:  Katman 3 — Tokenizer'ı tüketir, AST üretir
//
// Bu dosya bir AGGREGATOR'dür. Parser'ın tüm bileşenlerini tek bir include
// ile kullanılabilir yapar.
//
// MİMARİ:
//   parser_base.hpp       — Parser sınıf tanımı
//   parser_core.hpp       — parse, parseProgram, parseDeclaration, parseExpression
//   parser_decl.hpp       — parseFunctionDecl, parseStructDecl, parseVariableDecl
//   parser_stmt.hpp       — parseStatement, parseBlock, parseIf/While/For/...
//
// İKİ AYRI PARSER STRATEJİSİ:
//   1. Pratt Parser (ifadeler için): Operatör önceliğini merkezi tabloda yönetir
//   2. Recursive Descent (statement/deklarasyon): Her yapı kendi parse fonksiyonuna sahip
//
// ============================================================================

#ifndef SAQUT_PARSER
#define SAQUT_PARSER

// Sıralama önemli: önce sınıf tanımı, sonra metot gövdeleri
#include "parser/parser_base.hpp"
#include "parser/parser_core.hpp"
#include "parser/parser_decl.hpp"
#include "parser/parser_stmt.hpp"

#endif // SAQUT_PARSER
