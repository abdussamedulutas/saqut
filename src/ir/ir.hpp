// ============================================================================
// saQut Compiler — Intermediate Representation (Ara Gösterim)
// ============================================================================
//
// DİZİN:   src/ir/ir.hpp
// KATMAN:  Katman 4 — AST'yi alır, IR üretir
// BAĞIMLI: AST (src/parser/ast.hpp), dolaylı olarak Tokenizer ve Parser
// KULLANAN: main.cpp (debug çıktısı), gelecekte Code Generator
//
// AMAÇ:
//   Zengin AST'yi, çalıştırılabilir düşük seviyeli komutlara (IR) dönüştürür.
//   IR, bir "virtual register machine" (sanal kayıtçı makinesi) modelidir.
//
// IR MODELİ:
//   Her işlem (instruction) şu bileşenlerden oluşur:
//   - opcode:     İşlem kodu (mathadd, mathsub, mathmul, mathdiv, declare)
//   - targetReg:  Sonucun yazılacağı sanal register numarası
//   - arg1-arg3:  İşlem parametreleri (register veya sabit değer)
//
//   Sanal register'lar sınırsızdır (gerçek register tahsisi sonraki aşamada).
//   Bu yaklaşım, register allocation'ı ayrı bir probleme dönüştürür.
//
// ADR-005: IR Tasarımı
//   İki katmanlı IR planlanıyor:
//   - LightIR: Sadece çalıştırma için minimum bilgi (JIT/compiler)
//   - HeavyIR: Debug bilgisi, tip bilgisi, değişken isimleri (interpreter/debug)
//
//   Mevcut IR, LightIR'in embriyonik halidir.
//
// MEVCUT DURUM:
//   Desteklenen AST düğümleri:
//   ✅ BinaryExpression (sadece +, -, *, /)
//   ✅ Literal (NumberToken)
//   ✅ Program, FunctionDecl, Block (çocukları dolaşır)
//   ✅ ExpressionStatement, VariableDecl, ReturnStatement
//   ✅ IfStatement, WhileStatement, ForStatement, DoWhileStatement
//   ❌ Kontrol akışı (branch/jump/compare) — TODO
//   ❌ Fonksiyon çağrısı (call/ret) — TODO
//   ❌ Mantıksal/kıyaslama operatörleri — TODO
//
// BİLİNEN SINIRLAMALAR (TODO):
//   TODO: Kontrol akışı opcode'ları: br, jmp, cmp, br_eq, br_lt, vb.
//   TODO: Fonksiyon çağrısı: call, ret, param
//   TODO: Bellek: load, store, alloca
//   TODO: Tip bilgisi: IR opcode'ları tipleri taşımıyor
//   TODO: Float/int ayrımı düzgün değil (processNumber void* döndürüyor)
//   TODO: String, bool, null literal'ları işlenmiyor
//   TODO: Identifier (değişken okuma) işlenmiyor
//
// ============================================================================

#ifndef SAQUT_IR
#define SAQUT_IR

#include <vector>
#include <variant>
#include "parser/ast.hpp"

// ============================================================================
// OPCode — İşlem Kodları
// ============================================================================
//
// Sanal makinenin komut seti. Her komut bir veya daha fazla sanal register
// üzerinde işlem yapar.
//
// mathadd/mathsub/mathmul/mathdiv: arg1 ve arg2'yi işle, targetReg'e yaz
// declare: bir sabit değeri (literal) targetReg'e yükle
//
// TODO: cmp, br, jmp, call, ret, load, store, alloca eklenecek
//
enum class OPCode {
    mathadd,   // targetReg = arg1 + arg2
    mathsub,   // targetReg = arg1 - arg2
    mathdiv,   // targetReg = arg1 / arg2
    mathmul,   // targetReg = arg1 * arg2
    declare    // targetReg = literal değer (arg1)
};

// ============================================================================
// Param — İşlem Parametresi
// ============================================================================
//
// Bir IR komutunun girdisi. İki tür olabilir:
//   isRegister=true  → value bir register numarasıdır (int)
//   isRegister=false → value bir sabit değerdir (int veya float)
//
// std::variant<int,float> kullanımı: Derleme zamanı tip güvenliği sağlar.
// C union'dan farkı: Hangi tipin aktif olduğunu bilir, yanlış erişimi engeller.
//
struct Param {
    bool isRegister;                // true: register referansı, false: sabit değer
    std::variant<int, float> value; // Değer (register numarası veya sabit)
};

// ============================================================================
// IROpData — Tek Bir IR Komutu
// ============================================================================
//
// Sanal makinenin bir instruction'ı. 3 adrese kadar (3-address code) destekler.
// Çoğu işlem 2 parametre kullanır (binary ops), declare 1 parametre kullanır.
//
struct IROpData {
    OPCode op;           // İşlem kodu
    int    targetReg;    // Sonuç register'ı (sanal, sınırsız)
    Param  arg1;         // Birinci parametre
    Param  arg2;         // İkinci parametre
    Param  arg3;         // Üçüncü parametre (ileride kullanım için)
};

// ============================================================================
// Identifier — Sanal Register Yöneticisi
// ============================================================================
//
// Sınırsız sanal register tahsisi. Her yeni değer için monoton artan bir
// numara verir. Gerçek register tahsisi (register allocation) daha sonra
// yapılacak — bu aşamada sadece unique ID üretir.
//
// last: Şu ana kadar tahsis edilmiş en yüksek register numarası.
//       ++identifier.last → yeni register numarası.
//
struct Identifier {
    int last = 0;  // Son tahsis edilen register numarası
};

// ============================================================================
// CodeGenerator — AST → IR Dönüştürücü
// ============================================================================
//
// AST ağacını dolaşır (tree walk) ve her düğüm için karşılık gelen IR
// komutlarını üretir. Ziyaretçi deseni (visitor pattern) benzeri bir
// yaklaşım kullanır: parse() metodu ASTKind enum'ına göre dispatch eder.
//
// AKIŞ:
//   1. parse(rootAST) çağrılır
//   2. rootAST->kind'e göre uygun parse* metodu seçilir
//   3. Alt düğümler recursive olarak işlenir
//   4. Her BinaryExpression/Literal için IR komutu eklenir
//   5. Sonuç: IROpDatas vektörü doldurulur
//
class CodeGenerator {
private:
    // ----------------------------------------------------------------------
    // processNumber: NumberToken'dan C++ native sayı üret.
    //
    // Neden void*? Hem int hem float döndürebilmek için.
    // TODO: std::variant<int,float> ile değiştir.
    // ----------------------------------------------------------------------
    void* processNumber(NumberToken* num, const std::string& rawStr) {
        if (num->isFloat || num->hasEpsilon)
            return new float(std::strtof(rawStr.c_str(), nullptr));
        return new int(std::strtol(rawStr.c_str(), nullptr, num->base));
    }

public:
    Identifier identifier;             // Sanal register yöneticisi
    std::vector<IROpData> IROpDatas;  // Üretilen IR komutları

    // ------------------------------------------------------------------
    // parse: Ana dispatch fonksiyonu. AST düğüm tipine göre yönlendirir.
    //
    // BUG FIX (commit 40579ca): null giriş kontrolü eklendi.
    //   BinaryExpression'da Left null olabilir (unary operatörlerde).
    //   parse(nullptr) segfault'a neden oluyordu.
    // ------------------------------------------------------------------
    int parse(ASTNode* ast) {
        if (!ast) return 0;
        switch (ast->kind) {
            case ASTKind::BinaryExpression:
                return parseBinaryExpr((BinaryExpressionNode*)ast);
            case ASTKind::Literal:
                return parseLiteral((LiteralNode*)ast);
            case ASTKind::Program:
                return parseProgram((ProgramNode*)ast);
            case ASTKind::ExpressionStatement:
                return parse(((ExpressionStatementNode*)ast)->expression);
            case ASTKind::FunctionDecl:
                return parseFunctionDecl((FunctionDeclNode*)ast);
            case ASTKind::Block:
                return parseBlock((BlockNode*)ast);
            case ASTKind::ReturnStatement:
                return parseReturn((ReturnStatementNode*)ast);
            case ASTKind::VariableDecl:
                return parseVariableDecl((VariableDeclNode*)ast);
            case ASTKind::IfStatement:
                return parseIf((IfStatementNode*)ast);
            case ASTKind::WhileStatement:
                return parseWhile((WhileStatementNode*)ast);
            case ASTKind::ForStatement:
                return parseFor((ForStatementNode*)ast);
            case ASTKind::DoWhileStatement:
                return parseDoWhile((DoWhileStatementNode*)ast);
            default:
                return 0;
        }
    }

    // --- Yapısal düğümler: çocukları dolaş ---

    int parseProgram(ProgramNode* prog) {
        for (auto* child : prog->getChildren()) parse(child);
        return 0;
    }

    int parseFunctionDecl(FunctionDeclNode* fn) {
        for (auto* child : fn->getChildren()) parse(child);
        return 0;
    }

    int parseBlock(BlockNode* block) {
        for (auto* child : block->getChildren()) parse(child);
        return 0;
    }

    int parseReturn(ReturnStatementNode* ret) {
        if (ret->value) return parse(ret->value);
        return 0;
    }

    int parseVariableDecl(VariableDeclNode* vd) {
        if (vd->initExpr) return parse(vd->initExpr);
        return 0;
    }

    int parseIf(IfStatementNode* ifn) {
        parse(ifn->condition);
        parse(ifn->thenBranch);
        if (ifn->elseBranch) parse(ifn->elseBranch);
        return 0;
    }

    int parseWhile(WhileStatementNode* ws) {
        parse(ws->condition);
        parse(ws->body);
        return 0;
    }

    int parseFor(ForStatementNode* fs) {
        if (fs->init)      parse(fs->init);
        if (fs->condition) parse(fs->condition);
        if (fs->update)    parse(fs->update);
        parse(fs->body);
        return 0;
    }

    int parseDoWhile(DoWhileStatementNode* dw) {
        parse(dw->body);
        if (dw->condition) parse(dw->condition);
        return 0;
    }

    // ------------------------------------------------------------------
    // parseBinaryExpr: İkili işlem ifadesini IR'ye dönüştür.
    //
    // Sadece +, -, *, / operatörleri desteklenir.
    // Diğer operatörler (karşılaştırma, mantıksal) şimdilik es geçilir.
    //
    // BUG FIX (commit 438bc0e): Left veya Right null olabilir (unary prefix).
    //   parse(nullptr) çağrısı segfault yapıyordu. Ternary ile koruma eklendi.
    // ------------------------------------------------------------------
    int parseBinaryExpr(BinaryExpressionNode* bin) {
        OPCode op;
        switch (bin->Operator) {
            case TokenType::STAR:  op = OPCode::mathmul; break;
            case TokenType::PLUS:  op = OPCode::mathadd; break;
            case TokenType::MINUS: op = OPCode::mathsub; break;
            case TokenType::SLASH: op = OPCode::mathdiv; break;
            default: return 0;  // Desteklenmeyen operatör
        }

        int left  = bin->Left  ? parse(bin->Left)  : 0;
        int right = bin->Right ? parse(bin->Right) : 0;

        IROpDatas.push_back({
            op,
            ++identifier.last,
            {true, left},
            {true, right},
            {false, 0}
        });
        return identifier.last;
    }

    // ------------------------------------------------------------------
    // parseLiteral: Sayısal literal'ı IR'ye dönüştür.
    //
    // BUG FIX (commit 40579ca): &lit->parserToken.token → lit->parserToken.token
    //   ParserToken artık Token* tutuyor, & gereksiz (ve hatalı).
    //
    // BİLİNEN SORUN: Sadece NumberToken destekleniyor.
    //   StringToken, KeywordToken (true/false/null) için cast hatalı.
    //   TODO: Token tipine göre dispatch ekle.
    // ------------------------------------------------------------------
    int parseLiteral(LiteralNode* lit) {
        NumberToken* num = (NumberToken*)lit->parserToken.token;

        if (num->isFloat) {
            float* val = (float*)processNumber(num, num->token);
            IROpDatas.push_back({
                OPCode::declare,
                ++identifier.last,
                {false, *val},
                {false, 0},
                {false, 0}
            });
        } else {
            int* val = (int*)processNumber(num, num->token);
            IROpDatas.push_back({
                OPCode::declare,
                ++identifier.last,
                {false, *val},
                {false, 0},
                {false, 0}
            });
        }
        return identifier.last;
    }
};

#endif // SAQUT_IR
