#include "symbol/symbol_collector.hpp"
#include <functional>
#include "parser/nodes/program.hpp"
#include "parser/nodes/declarations.hpp"
#include "parser/nodes/statements.hpp"
#include "parser/nodes/expressions.hpp"
#include "parser/nodes/binary_expr.hpp"
#include "parser/nodes/identifier.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// collect — dört aşamalı toplama
// ─────────────────────────────────────────────────────────────────────────────

void SymbolCollector::collect(ASTNode* program) {
    if (!program) return;
    seedBuiltins();
    pass1Globals(program);
    checkStructCycles();
    pass2Bodies(program);
}

// ─────────────────────────────────────────────────────────────────────────────
// seedBuiltins — global scope'a yerleşik fonksiyonları ekle
// ─────────────────────────────────────────────────────────────────────────────

void SymbolCollector::seedBuiltins() {
    // TODO(#89 builtin kataloğu): geçici; ileride gerçek katalog gelecek.
    Symbol* s = table_.define("print", SymbolKind::Function,
                               Type::function(Type::Void(), {}),
                               SourceLocation{}, 0 /* BUILTIN_ID */);
    if (s) s->isBuiltin = true;

    // ADR-025: Error builtin struct — try/catch için
    // Alan sırası VM makeErrorValue() ile eşleşmeli: [line, col, message, trace, code]
    table_.structLayouts["Error"] = {
        {"line",    Type::Int()},
        {"col",     Type::Int()},
        {"message", Type::String()},
        {"trace",   Type::String()},
        {"code",    Type::String()}
    };
    Symbol* errSym = table_.define("Error", SymbolKind::Struct,
                                   Type::structType("Error"), {}, 0 /* BUILTIN_ID */);
    if (errSym) errSym->isBuiltin = true;
    structFields_["Error"]; // cycle checker'a tanıt
}

// ─────────────────────────────────────────────────────────────────────────────
// typeFromName — tip adından Type üret; bilinmiyorsa E007
// ─────────────────────────────────────────────────────────────────────────────

Type SymbolCollector::typeFromName(const std::string& n, const SourceLocation& loc) {
    // "Node?" gibi nullable struct: Type::fromName struct'ı bilmez, burada çözüyoruz.
    if (!n.empty() && n.back() == '?') {
        Type base = typeFromName(n.substr(0, n.size() - 1), loc);
        if (!base.isError()) return base.asNullable();
        return Type::error();
    }
    Type t = Type::fromName(n);
    if (!t.isError()) return t;
    if (structFields_.count(n)) return Type::structType(n);
    if (table_.isEnumName(n)) return Type::enumType(n);
    diag_.report("E007", loc, "unknown type: '" + n + "'",
        "known types: int, float, double, decimal, bool, string. if using a struct, define it first: `struct " + n + " { ... }`, or an enum: `enum " + n + " { ... }`");
    return Type::error();
}

// ─────────────────────────────────────────────────────────────────────────────
// pass1Globals — üst-seviye isimleri hoist eder (gövdelere girmez)
// ─────────────────────────────────────────────────────────────────────────────

void SymbolCollector::pass1Globals(ASTNode* program) {
    for (ASTNode* child : program->getChildren()) {
        switch (child->kind) {

        case ASTKind::FunctionDecl: {
            auto* fn = (FunctionDeclNode*)child;
            // parametre tiplerini topla
            std::vector<Type> paramTypes;
            for (auto* p : fn->params)
                paramTypes.push_back(typeFromName(p->varType, p->loc));
            Type retType = typeFromName(fn->returnType, fn->loc);
            Symbol* s = table_.define(fn->name, SymbolKind::Function,
                                      Type::function(retType, paramTypes),
                                      fn->loc);
            if (!s) {
                Symbol* ex_ = table_.resolve(fn->name);
                std::string h_ = ex_ ? "'" + fn->name + "' first defined at " + ex_->definitionLoc.toString() + " — choose a different name" : "choose a different name";
                diag_.report("E002", fn->loc, "'" + fn->name + "' already defined in this scope", h_);
            }
            break;
        }

        case ASTKind::EnumDecl: {
            auto* en = (EnumDeclNode*)child;
            Symbol* s = table_.define(en->name, SymbolKind::Enum,
                                      Type::enumType(en->name), en->loc);
            if (!s) {
                Symbol* ex_ = table_.resolve(en->name);
                std::string h_ = ex_ ? "'" + en->name + "' first defined at " + ex_->definitionLoc.toString() : "choose a different name";
                diag_.report("E002", en->loc, "'" + en->name + "' already defined in this scope", h_);
                break;
            }
            auto& layout = table_.enumLayouts[en->name];
            for (auto& m : en->members)
                layout.push_back({m.name, m.value});
            break;
        }

        case ASTKind::StructDecl: {
            auto* st = (StructDeclNode*)child;
            Symbol* s = table_.define(st->name, SymbolKind::Struct,
                                      Type::structType(st->name), st->loc);
            if (!s) {
                Symbol* ex_ = table_.resolve(st->name);
                std::string h_ = ex_ ? "'" + st->name + "' first defined at " + ex_->definitionLoc.toString() + " — choose a different name" : "choose a different name";
                diag_.report("E002", st->loc, "'" + st->name + "' already defined in this scope", h_);
                break;
            }
            // Always open an entry in structFields_ (needed for typeFromName)
            structFields_[st->name]; // creates empty vector; by-value cycles now valid with reference semantics (ADR-020)

            // structLayouts: tüm alanlar (isim + tip) sırayla — IR üreteci ve tip denetleyici için
            for (ASTNode* fieldNode : st->getChildren()) {
                if (fieldNode->kind == ASTKind::VariableDecl) {
                    auto* vd = (VariableDeclNode*)fieldNode;
                    Type ft = typeFromName(vd->varType, vd->loc);
                    table_.structLayouts[st->name].push_back({vd->name, ft});
                }
            }
            break;
        }

        case ASTKind::VariableDecl: {
            auto* vd = (VariableDeclNode*)child;
            Symbol* s = table_.define(vd->name, SymbolKind::Variable,
                                      typeFromName(vd->varType, vd->loc),
                                      vd->loc);
            if (!s) {
                Symbol* ex_ = table_.resolve(vd->name);
                std::string h_ = ex_ ? "'" + vd->name + "' first defined at " + ex_->definitionLoc.toString() + " — choose a different name" : "choose a different name";
                diag_.report("E002", vd->loc, "'" + vd->name + "' already defined in this scope", h_);
            }
            // Sibling VariableDecl's (int a, b;)
            for (ASTNode* sib : vd->getChildren()) {
                if (sib->kind == ASTKind::VariableDecl) {
                    auto* sv = (VariableDeclNode*)sib;
                    Symbol* ss = table_.define(sv->name, SymbolKind::Variable,
                                              typeFromName(sv->varType, sv->loc),
                                              sv->loc);
                    if (!ss) {
                        Symbol* ex_ = table_.resolve(sv->name);
                        std::string h_ = ex_ ? "'" + sv->name + "' first defined at " + ex_->definitionLoc.toString() + " — choose a different name" : "choose a different name";
                        diag_.report("E002", sv->loc, "'" + sv->name + "' already defined in this scope", h_);
                    }
                }
            }
            break;
        }

        default:
            break;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// checkStructCycles — E010 döngüsel struct (by-value çevrim → sonsuz boyut)
// ─────────────────────────────────────────────────────────────────────────────

void SymbolCollector::checkStructCycles() {
    // ADR-020: Struct alanları referans semantiği taşır (Object* pointer).
    // By-value gömme yok → sonsuz-boyut döngüsü imkânsız.
    // E010 artık üretilmez; bu metot koşullu olarak devre dışı.
    // TODO(gelecek): Primitive tipler için by-value gömme eklenirse E010 geri açılır.
}

// ─────────────────────────────────────────────────────────────────────────────
// pass2Bodies — fonksiyon gövdelerini gez; isim çözümle + referans topla
// ─────────────────────────────────────────────────────────────────────────────

void SymbolCollector::pass2Bodies(ASTNode* program) {
    for (ASTNode* child : program->getChildren()) {
        switch (child->kind) {

        case ASTKind::FunctionDecl: {
            auto* fn = (FunctionDeclNode*)child;
            table_.enterScope();
            // parametreleri tanımla
            for (auto* p : fn->params) {
                Symbol* s = table_.define(p->name, SymbolKind::Parameter,
                                         typeFromName(p->varType, p->loc), p->loc);
                if (!s) {
                    Symbol* ex_ = table_.resolve(p->name);
                    std::string h_ = ex_ ? "'" + p->name + "' first defined at " + ex_->definitionLoc.toString() + " — choose a different name" : "choose a different name";
                    diag_.report("E002", p->loc, "parameter '" + p->name + "' already defined", h_);
                }
            }
            // walk body (children[0] = BlockNode)
            auto& ch = fn->getChildren();
            if (!ch.empty()) walkStmt(ch[0]);
            table_.exitScope();
            break;
        }

        case ASTKind::VariableDecl: {
            // global değişken başlatıcısı (declare-before-use)
            auto* vd = (VariableDeclNode*)child;
            if (vd->initExpr) walkExpr(vd->initExpr);
            // TODO(faz2): global init sırası kontrolü (fibonacci'de global var yok)
            for (ASTNode* sib : vd->getChildren()) {
                if (sib->kind == ASTKind::VariableDecl) {
                    auto* sv = (VariableDeclNode*)sib;
                    if (sv->initExpr) walkExpr(sv->initExpr);
                }
            }
            break;
        }

        case ASTKind::StructDecl:
        case ASTKind::EnumDecl:
            break; // pass2'de gövde gezme gerekmez

        default:
            break;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// walkStmt — ifade bloğunu gez
// ─────────────────────────────────────────────────────────────────────────────

void SymbolCollector::walkStmt(ASTNode* node) {
    if (!node) return;

    switch (node->kind) {

    case ASTKind::Block: {
        table_.enterScope();
        for (ASTNode* child : node->getChildren()) walkStmt(child);
        table_.exitScope();
        break;
    }

    case ASTKind::VariableDecl: {
        auto* vd = (VariableDeclNode*)node;
        // First walk initializer (prevent self-reference)
        if (vd->initExpr) walkExpr(vd->initExpr);
        // Then define
        Symbol* s = table_.define(vd->name, SymbolKind::Variable,
                                  typeFromName(vd->varType, vd->loc), vd->loc);
        if (!s) {
            Symbol* ex_ = table_.resolve(vd->name);
            std::string h_ = ex_ ? "'" + vd->name + "' first defined at " + ex_->definitionLoc.toString() + " — choose a different name" : "choose a different name";
            diag_.report("E002", vd->loc, "'" + vd->name + "' already defined in this scope", h_);
        }
        // Sibling VariableDecl's (int a, b;) — TODO: not in fibonacci
        for (ASTNode* sib : vd->getChildren()) {
            if (sib->kind == ASTKind::VariableDecl) {
                auto* sv = (VariableDeclNode*)sib;
                if (sv->initExpr) walkExpr(sv->initExpr);
                Symbol* ss = table_.define(sv->name, SymbolKind::Variable,
                                          typeFromName(sv->varType, sv->loc), sv->loc);
                if (!ss) {
                    Symbol* ex_ = table_.resolve(sv->name);
                    std::string h_ = ex_ ? "'" + sv->name + "' first defined at " + ex_->definitionLoc.toString() + " — choose a different name" : "choose a different name";
                    diag_.report("E002", sv->loc, "'" + sv->name + "' already defined in this scope", h_);
                }
            }
        }
        break;
    }

    case ASTKind::IfStatement: {
        auto* ifn = (IfStatementNode*)node;
        if (ifn->condition)  walkExpr(ifn->condition);
        if (ifn->thenBranch) walkStmt(ifn->thenBranch);
        if (ifn->elseBranch) walkStmt(ifn->elseBranch);
        break;
    }

    case ASTKind::WhileStatement: {
        auto* ws = (WhileStatementNode*)node;
        if (ws->condition) walkExpr(ws->condition);
        if (ws->body)      walkStmt(ws->body);
        break;
    }

    case ASTKind::ForStatement: {
        auto* fs = (ForStatementNode*)node;
        table_.enterScope(); // init değişkeni döngüye ait
        if (fs->init) {
            if (fs->init->kind == ASTKind::VariableDecl) walkStmt(fs->init);
            else walkExpr(fs->init);
        }
        if (fs->condition) walkExpr(fs->condition);
        if (fs->update)    walkExpr(fs->update);
        if (fs->body)      walkStmt(fs->body);
        table_.exitScope();
        break;
    }

    case ASTKind::DoWhileStatement: {
        auto* dw = (DoWhileStatementNode*)node;
        if (dw->body)      walkStmt(dw->body);
        if (dw->condition) walkExpr(dw->condition);
        break;
    }

    case ASTKind::ReturnStatement: {
        auto* rs = (ReturnStatementNode*)node;
        if (rs->value) walkExpr(rs->value);
        break;
    }

    case ASTKind::ExpressionStatement: {
        auto* es = (ExpressionStatementNode*)node;
        if (es->expression) walkExpr(es->expression);
        break;
    }

    case ASTKind::BreakStatement:
    case ASTKind::ContinueStatement:
        break; // yaprak

    // ADR-025: try { body } catch (Error e) { handler }
    case ASTKind::TryStatement: {
        auto* ts = (TryStatementNode*)node;
        if (ts->body) walkStmt(ts->body);
        // catch değişkeni catch bloğu kapsamında görünür
        if (ts->handler) {
            table_.enterScope();
            if (!ts->catchVar.empty())
                table_.define(ts->catchVar, SymbolKind::Variable,
                              Type::structType("Error"), {});
            walkStmt(ts->handler);
            table_.exitScope();
        }
        break;
    }

    // ADR-025: throw <ifade>;
    case ASTKind::ThrowStatement: {
        auto* th = (ThrowStatementNode*)node;
        if (th->value) walkExpr(th->value);
        break;
    }

    // ADR-027: switch (expr) { case v: ... default: ... }
    case ASTKind::SwitchStatement: {
        auto* sw = (SwitchStatementNode*)node;
        if (sw->subject) walkExpr(sw->subject);
        for (auto& clause : sw->cases) {
            for (auto* val : clause.values) walkExpr(val);
            for (auto* s   : clause.body)   walkStmt(s);
        }
        break;
    }

    default:
        break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// walkExpr — ifade ağacında isimleri çöz
// ─────────────────────────────────────────────────────────────────────────────

void SymbolCollector::walkExpr(ASTNode* node) {
    if (!node) return;

    switch (node->kind) {

    case ASTKind::Identifier: {
        auto* id = (IdentifierNode*)node;
        if (!id->parserToken.token) break;
        std::string name = id->parserToken.token->token;
        Symbol* s = table_.resolve(name);
        if (s) {
            id->resolvedSymbol = s;
            table_.addReference(s, id->loc);
        } else {
            std::vector<std::string> cands_;
            for (auto* sym_ : table_.allSymbols()) cands_.push_back(sym_->name);
            std::string sug_ = suggestName(name, cands_);
            std::string h_ = sug_.empty()
                ? "define it before use: `int " + name + " = 0;` (set type and value)"
                : "did you mean: `" + sug_ + "`?";
            diag_.report("E001", id->loc, "'" + name + "' is not defined", h_);
        }
        break;
    }

    case ASTKind::BinaryExpression: {
        auto* bin = (BinaryExpressionNode*)node;
        if (bin->Left)  walkExpr(bin->Left);
        if (bin->Right) walkExpr(bin->Right);
        break;
    }

    case ASTKind::Call: {
        auto* call = (CallExpressionNode*)node;
        if (call->callee) walkExpr(call->callee);
        for (ASTNode* arg : call->arguments) walkExpr(arg);
        break;
    }

    case ASTKind::Postfix: {
        auto* pf = (PostfixNode*)node;
        if (pf->operand) walkExpr(pf->operand);
        break;
    }

    case ASTKind::MemberAccess: {
        auto* ma = (MemberAccessNode*)node;
        if (ma->object) walkExpr(ma->object); // member çözümü Faz 3 → TODO
        break;
    }

    case ASTKind::IndexExpression: {
        auto* ie = (IndexExpressionNode*)node;
        if (ie->object) walkExpr(ie->object);
        if (ie->index)  walkExpr(ie->index);
        break;
    }

    case ASTKind::ArrayLiteral: {
        auto* al = (ArrayLiteralNode*)node;
        for (auto* e : al->elements) walkExpr(e);
        break;
    }

    case ASTKind::CastExpression: {  // ADR-026
        auto* cast = (CastExpressionNode*)node;
        if (cast->operand) walkExpr(cast->operand);
        break;
    }

    case ASTKind::Literal:
        break; // yaprak

    default:
        break;
    }
}
