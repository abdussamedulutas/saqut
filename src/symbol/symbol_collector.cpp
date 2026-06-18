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
                               SourceLocation{});
    if (s) s->isBuiltin = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// typeFromName — tip adından Type üret; bilinmiyorsa E007
// ─────────────────────────────────────────────────────────────────────────────

Type SymbolCollector::typeFromName(const std::string& n, const SourceLocation& loc) {
    Type t = Type::fromName(n);
    if (!t.isError()) return t;
    if (structFields_.count(n)) return Type::structType(n);
    // TODO(faz2/faz3): bilinmeyen tip tam E007 tanısı
    diag_.report("E007", loc, "Bilinmeyen tip: '" + n + "'");
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
            if (!s)
                diag_.report("E002", fn->loc,
                             "'" + fn->name + "' zaten bu kapsamda tanımlı");
            break;
        }

        case ASTKind::StructDecl: {
            auto* st = (StructDeclNode*)child;
            Symbol* s = table_.define(st->name, SymbolKind::Struct,
                                      Type::structType(st->name), st->loc);
            if (!s) {
                diag_.report("E002", st->loc,
                             "'" + st->name + "' zaten bu kapsamda tanımlı");
                break;
            }
            // struct alan isimlerini cycle check için kaydet
            for (ASTNode* fieldNode : st->getChildren()) {
                if (fieldNode->kind == ASTKind::VariableDecl) {
                    auto* vd = (VariableDeclNode*)fieldNode;
                    // yalnızca struct tipindeki alanları izle
                    Type ft = Type::fromName(vd->varType);
                    if (ft.isError()) // primitif değilse struct tipi olabilir
                        structFields_[st->name].push_back(vd->varType);
                }
            }
            break;
        }

        case ASTKind::VariableDecl: {
            auto* vd = (VariableDeclNode*)child;
            Symbol* s = table_.define(vd->name, SymbolKind::Variable,
                                      typeFromName(vd->varType, vd->loc),
                                      vd->loc);
            if (!s)
                diag_.report("E002", vd->loc,
                             "'" + vd->name + "' zaten bu kapsamda tanımlı");
            // Sibling VariableDecl'ler (int a, b;)
            for (ASTNode* sib : vd->getChildren()) {
                if (sib->kind == ASTKind::VariableDecl) {
                    auto* sv = (VariableDeclNode*)sib;
                    Symbol* ss = table_.define(sv->name, SymbolKind::Variable,
                                              typeFromName(sv->varType, sv->loc),
                                              sv->loc);
                    if (!ss)
                        diag_.report("E002", sv->loc,
                                     "'" + sv->name + "' zaten bu kapsamda tanımlı");
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
    // white=0 / gray=1 / black=2
    std::unordered_map<std::string, int> color;
    for (auto& kv : structFields_) color[kv.first] = 0;

    std::function<bool(const std::string&)> dfs = [&](const std::string& name) -> bool {
        auto it = color.find(name);
        if (it == color.end()) return false; // primitif / bilinmeyen → çevrim değil
        if (it->second == 1) return true;    // gray → back-edge → çevrim!
        if (it->second == 2) return false;   // black → zaten işlendi

        it->second = 1; // gri yap
        auto fit = structFields_.find(name);
        if (fit != structFields_.end()) {
            for (const std::string& dep : fit->second) {
                if (dfs(dep)) return true;
            }
        }
        it->second = 2; // siyah yap
        return false;
    };

    for (auto& kv : structFields_) {
        if (color[kv.first] == 0) {
            // DFS başlat
            color[kv.first] = 1;
            for (const std::string& dep : kv.second) {
                if (dfs(dep)) {
                    // tanımlama konumunu bulmak için global scope'ta ara
                    Symbol* s = table_.global()->lookupLocal(kv.first);
                    SourceLocation loc = s ? s->definitionLoc : SourceLocation{};
                    diag_.report("E010", loc,
                                 "Döngüsel struct: '" + kv.first + "' by-value sonsuz boyut oluşturur");
                    break;
                }
            }
            color[kv.first] = 2;
        }
    }
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
                if (!s)
                    diag_.report("E002", p->loc,
                                 "Parametre '" + p->name + "' zaten tanımlı");
            }
            // gövdeyi gez (children[0] = BlockNode)
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
        // Önce başlatıcıyı gez (kendini görmesin)
        if (vd->initExpr) walkExpr(vd->initExpr);
        // Sonra tanımla
        Symbol* s = table_.define(vd->name, SymbolKind::Variable,
                                  typeFromName(vd->varType, vd->loc), vd->loc);
        if (!s)
            diag_.report("E002", vd->loc,
                         "'" + vd->name + "' zaten bu kapsamda tanımlı");
        // Sibling VariableDecl'ler (int a, b;) — TODO: fibonacci'de yok
        for (ASTNode* sib : vd->getChildren()) {
            if (sib->kind == ASTKind::VariableDecl) {
                auto* sv = (VariableDeclNode*)sib;
                if (sv->initExpr) walkExpr(sv->initExpr);
                Symbol* ss = table_.define(sv->name, SymbolKind::Variable,
                                          typeFromName(sv->varType, sv->loc), sv->loc);
                if (!ss)
                    diag_.report("E002", sv->loc,
                                 "'" + sv->name + "' zaten bu kapsamda tanımlı");
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
            diag_.report("E001", id->loc,
                         "'" + name + "' tanımlı değil");
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

    case ASTKind::Literal:
        break; // yaprak

    default:
        break;
    }
}
