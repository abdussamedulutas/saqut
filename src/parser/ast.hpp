// ============================================================================
// saQut Compiler — AST Umbrella Header
// ============================================================================
//
// DİZİN:   src/parser/ast.hpp
// KATMAN:  Katman 3 — Tüm AST düğümlerini tek include'da toplar
//
// AMAÇ:
//   Parser ve diğer modüller (SymbolCollector, TypeChecker, IRGenerator)
//   sadece bu dosyayı include ederek tüm AST düğümlerine erişebilir.
//   Doğrudan kendi kodu yoktur — bir "facade" veya "umbrella header"dır.
//
// ============================================================================

#ifndef SAQUT_AST
#define SAQUT_AST

#include "parser/ast_node.hpp"
#include "parser/nodes/program.hpp"
#include "parser/nodes/declarations.hpp"
#include "parser/nodes/statements.hpp"
#include "parser/nodes/binary_expr.hpp"
#include "parser/nodes/literal.hpp"
#include "parser/nodes/identifier.hpp"
#include "parser/nodes/expressions.hpp"
#include "parser/nodes/error_node.hpp"

#endif
