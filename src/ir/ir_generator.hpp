// ============================================================================
// saQut IR — IRGenerator (AST → IR Dönüşümü)
//
// AST'yi (parse edilmiş kaynak kodu) Instruction listelerine çevirir.
// Her fonksiyon için bir IRFunction üretir, hepsini IRProgram'a toplar.
//
// SLOT ATAMA STRATEJİSİ:
//   - Her fonksiyon üretiminde nextSlot_ sıfırdan başlar.
//   - Parametreler 0, 1, 2, ... slotlarına sırayla atanır.
//   - Sonraki her değişken veya geçici sonuç freshSlot() ile yeni slot alır.
//   - Slotlar asla geri verilmez (basitlik öncelikli).
//   - Fonksiyon bitince nextSlot_ = slotCount.
//
// SINIRLAMALAR (fibonacci için yeterli, genel dil için TODO):
//   - Aynı isimli iki değişken farklı iç kapsamlarda olsa bile çakışır.
//     (fibonacci.sqt'de bu durum yok; gelecekte scope-aware slot atama gerekir.)
//   - Sadece int değerler desteklenir (Value.kind şu an hep Int).
// ============================================================================

#ifndef SAQUT_IR_GENERATOR
#define SAQUT_IR_GENERATOR

#include <string>
#include <unordered_map>
#include "ir/ir_program.hpp"
#include "symbol/symbol_table.hpp"
#include "parser/ast_node.hpp"

class IRGenerator {
public:
    // Ana giriş noktası: programNode = ProgramNode, tablo = sembol tablosu
    IRProgram generate(ASTNode* programNode, SymbolTable& symbolTable);

private:
    // ── Fonksiyon üretimi ─────────────────────────────────────────────────
    void generateFunction(ASTNode* functionDeclNode);

    // ── Deyim (statement) üretimi — talimat listesine yazar ──────────────
    void generateStatement(ASTNode* node);

    // ── İfade (expression) üretimi — sonucun slotunu döndürür ────────────
    // Sonuç her zaman bir slotta bulunur. Identifier zaten bir slotta,
    // hesaplamalar freshSlot() ile yeni slot alır.
    int generateExpression(ASTNode* node);

    // ── İkili operatör (binary op) için ortak yardımcı ───────────────────
    int generateBinaryArithmetic(Opcode opcode, ASTNode* leftNode, ASTNode* rightNode);

    // ── Slot yönetimi ─────────────────────────────────────────────────────
    int  freshSlot();                           // Yeni slot numarası al (nextSlot_++)
    void registerVariable(const std::string& name, int slot); // name → slot kaydı
    int  lookupVariable(const std::string& name);             // name → slot (bulunamazsa hata)

    // ── Talimat yazma yardımcıları ────────────────────────────────────────
    // Talimatları currentFunction_->instructions'a ekler.

    void emitLoadConst(int destSlot, int value);
    void emitLoadSlot(int destSlot, int srcSlot);
    void emitBinaryOp(Opcode op, int destSlot, int leftSlot, int rightSlot);
    void emitReturn(int srcSlot);
    // Koşulsuz atlama yazar; instruction indeksini döndürür (backpatch için).
    // Hedef bilinmiyorsa -1 geçilir, patchJump() ile doldurulur.
    int  emitJumpUnconditional(int targetInstrIndex);

    // JIF_FALSE talimatını -1 hedefle yazar, instruction indeksini döndürür.
    // Döndürülen indeks ileride patchJump() ile doldurulur (backpatch).
    int  emitJumpIfFalse(int condSlot);

    // Daha önce -1 hedefle yazılan jump'ın hedefini şu anki pozisyona doldur.
    void patchJump(int instrIndex);

    // Şu an kaç talimat üretildi? (jump hedefi belirlemek için)
    int currentInstrIndex() const;

    // ── Per-function üretim durumu ────────────────────────────────────────
    IRFunction* currentFunction_ = nullptr; // şu an üretilen fonksiyon
    int         nextSlot_        = 0;       // sıradaki boş slot numarası

    // Değişken ismi → slot numarası.
    // Sınırlama: aynı isimdeki farklı scope değişkenleri çakışır (TODO).
    std::unordered_map<std::string, int> nameToSlot_;
};

#endif // SAQUT_IR_GENERATOR
