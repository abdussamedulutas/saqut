// ============================================================================
// saQut Benchmark — Aşama Profiler
//
// Yalnızca `saqut bench` komutu tarafından kullanılır.
// Normal derleme/çalışma pipeline'ına sıfır etkisi vardır.
//
// TASARIM KURALI (kullanıcı isteği):
//   VM çalışırken hesap/karşılaştırma YOK.
//   Bunun yerine iki paralel büyüyen vektör:
//     traceOpcodes[i]  = i. dispatch'teki opcode
//     traceTicks[i]    = i. dispatch'teki zaman damgası
//   VM bittikten sonra analyzeVMTrace() ile istatistikler türetilir.
//
// DONANIM ZAMANLAMA:
//   x86/x86_64: __rdtsc() — ~2-3 saat döngüsü, nanosaniyeden hızlı.
//   Diğerleri: std::chrono::steady_clock.
//   analyzeVMTrace() içinde tek seferlik TSC kalibrasyon yapılır.
// ============================================================================

#pragma once

#include <algorithm>
#include <climits>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

// ── Donanım zaman damgası ────────────────────────────────────────────────────
#if defined(__x86_64__) || defined(__i386__)
#  include <x86intrin.h>
static inline uint64_t benchTick() { return __rdtsc(); }
static constexpr bool kUseTSC = true;
#else
#  include <chrono>
static inline uint64_t benchTick() {
    return static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
}
static constexpr bool kUseTSC = false;
#endif

// ── VM Execution Trace ───────────────────────────────────────────────────────
// VM çalışırken: sadece push — hesap yok.
// VM bittikten sonra: analyzeVMTrace() ile istatistik türet.
//
// İki ayrı vektör (cache-locality için tek struct yerine):
//   traceOpcodes — 1 byte/talimat
//   traceTicks   — 8 byte/talimat
// 8M talimat → 8MB + 64MB = ~72MB, kullanıcının beklediği değer.

struct BenchVMTrace {
    std::vector<uint8_t>  traceOpcodes;
    std::vector<uint64_t> traceTicks;

    // Lightweight sayaçlar — VM döngüsündeki null-check aynı yerde artar
    uint64_t vmLoopIter     = 0;  // toplam dispatch iterasyonu
    uint64_t vmSaqutCalls   = 0;  // CALL opcodu (saQut→saQut)
    uint64_t vmFfiCalls     = 0;  // CALLHOST (host FFI veya builtin)
    uint64_t vmBuiltinCalls = 0;  // CALLHOST + __builtin_method__

    void reserve(size_t hint) {
        traceOpcodes.reserve(hint);
        traceTicks.reserve(hint);
    }

    // VM döngüsünden çağrılır — hesap yok, sadece kaydet
    inline void pushDispatch(uint8_t op) {
        traceTicks.push_back(benchTick());
        traceOpcodes.push_back(op);
        ++vmLoopIter;
    }
};

// ── Per-Opcode Analiz Sonucu ─────────────────────────────────────────────────
struct OpcodeStats {
    uint64_t count     = 0;
    uint64_t totalTick = 0;
    uint64_t minTick   = UINT64_MAX;
    uint64_t maxTick   = 0;
};

// ── Token İstatistikleri ─────────────────────────────────────────────────────
struct TokenStats {
    uint64_t total       = 0;
    uint64_t keywords    = 0;
    uint64_t identifiers = 0;
    uint64_t numbers     = 0;
    uint64_t strings     = 0;
    uint64_t operators_  = 0;
    uint64_t delimiters  = 0;
    uint64_t other       = 0;
    uint64_t fileCount   = 0;
};

// ── AST İstatistikleri ───────────────────────────────────────────────────────
struct ASTStats {
    uint64_t totalNodes   = 0;
    uint64_t declarations = 0;
    uint64_t statements   = 0;
    uint64_t expressions  = 0;
    uint64_t funcDecls    = 0;
    uint64_t varDecls     = 0;
    uint64_t importDecls  = 0;
};

// ── Sembol İstatistikleri ────────────────────────────────────────────────────
struct SymbolStats {
    uint64_t total      = 0;
    uint64_t functions  = 0;
    uint64_t variables  = 0;
    uint64_t parameters = 0;
    uint64_t structs    = 0;
    uint64_t enums      = 0;
    uint64_t passes     = 3;  // SymbolCollector her zaman 3 geçiş yapar
};

// ── IR İstatistikleri ────────────────────────────────────────────────────────
struct IRStats {
    uint64_t totalInstr  = 0;
    uint64_t funcCount   = 0;
    uint64_t ffiSites    = 0;  // CALLHOST talimatı sayısı (statik)
    uint64_t callSites   = 0;  // CALL talimatı sayısı (statik)
    std::unordered_map<std::string, uint64_t> staticOpcodes;  // opcode dağılımı (statik)
};

// ── Tam Profil ───────────────────────────────────────────────────────────────
struct BenchProfile {
    TokenStats  tok;
    ASTStats    ast;
    SymbolStats sym;
    IRStats     ir;
    BenchVMTrace vmTrace;

    uint64_t vmHeapAllocCount = 0;  // Heap::allocCount (toplam tahsis)

    // Analiz sonuçları — analyzeVMTrace() ile doldurulur
    std::unordered_map<std::string, OpcodeStats> opcodeResult;
    uint64_t tscHz = 0;  // kalibrasyon: tsc/saniye

    // ── TSC kalibrasyon ───────────────────────────────────────────────────────
    // Kısa bir chrono interval ile tsc/s ölçer.
    void calibrateTSC() {
        if (!kUseTSC) { tscHz = 1'000'000'000ULL; return; }
        // 10ms ölçüm — bench bağlamında ihmal edilebilir ek süre
        using Clk = std::chrono::steady_clock;
        auto  ta = Clk::now();
        uint64_t ra = benchTick();
        // busy-wait (sleep gerekmez — kalibrasyon için yeterli)
        uint64_t wait_ns = 10'000'000; // 10ms
        while (true) {
            auto tb = Clk::now();
            if ((uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
                    tb - ta).count() >= wait_ns) {
                tscHz = (benchTick() - ra) * 1'000'000'000ULL /
                        (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
                            tb - ta).count();
                break;
            }
        }
    }

    // ticks → nanosaniye
    uint64_t ticksToNs(uint64_t ticks) const {
        if (!tscHz) return ticks;
        return ticks * 1'000'000'000ULL / tscHz;
    }

    // ── VM trace analizi ──────────────────────────────────────────────────────
    // VM bittikten sonra çağrılır.
    // Parametre: opcode int → isim fonksiyonu (instruction.hpp'den opcodeName())
    void analyzeVMTrace(const char* (*nameFunc)(int)) {
        auto& oc = vmTrace.traceOpcodes;
        auto& tc = vmTrace.traceTicks;
        size_t N = oc.size();
        if (N == 0) return;

        // Her opcode için geçici stats map (uint8_t key, sabit boyutlu dizi daha iyi
        // ama unordered_map portable ve yeterince hızlı analiz aşamasında)
        std::unordered_map<uint8_t, OpcodeStats> tmp;
        tmp.reserve(64);  // max ~60 opcode

        // Instruction i'nin süresi = tick[i+1] - tick[i]
        // Son instruction'ın süresi bilinmiyor — sadece count artar
        for (size_t i = 0; i + 1 < N; i++) {
            uint8_t  op  = oc[i];
            uint64_t dur = tc[i + 1] - tc[i];
            auto& s = tmp[op];
            s.count++;
            s.totalTick += dur;
            if (dur < s.minTick) s.minTick = dur;
            if (dur > s.maxTick) s.maxTick = dur;
        }
        // Son talimat
        if (N > 0) tmp[oc[N - 1]].count++;

        // İsimlere çevir
        opcodeResult.clear();
        for (auto& [op, s] : tmp) {
            std::string name = nameFunc ? nameFunc((int)op) : std::to_string(op);
            opcodeResult[name] = s;
        }
    }
};

// ── Token istatistiklerini topla ─────────────────────────────────────────────
// Token::gettype() → "keyword" / "identifier" / "number" / "string" /
//                     "operator" / "delimiter"
#include "tokenizer/token.hpp"

inline void collectTokenStats(TokenStats& out,
                               const std::vector<std::vector<Token*>>& allTokens) {
    out.fileCount = allTokens.size();
    for (auto& toks : allTokens) {
        out.total += toks.size();
        for (auto* t : toks) {
            const std::string& ty = const_cast<Token*>(t)->gettype();
            if      (ty == "keyword")    ++out.keywords;
            else if (ty == "identifier") ++out.identifiers;
            else if (ty == "number")     ++out.numbers;
            else if (ty == "string")     ++out.strings;
            else if (ty == "operator")   ++out.operators_;
            else if (ty == "delimiter")  ++out.delimiters;
            else                         ++out.other;
        }
    }
}

// ── AST istatistiklerini topla ───────────────────────────────────────────────
// AST düğümleri hem getChildren() hem tipli pointer'lar kullanır.
// Doğru sayım için her düğüm tipine özgü pointer'lar da gezilir.
#include "parser/ast_node.hpp"
#include "parser/nodes/statements.hpp"
#include "parser/nodes/declarations.hpp"
#include "parser/nodes/binary_expr.hpp"
#include "parser/nodes/expressions.hpp"
#include "module/module_graph.hpp"

static void walkAST(ASTNode* node, ASTStats& out);

static void walkList(const std::vector<ASTNode*>& list, ASTStats& out) {
    for (auto* n : list) walkAST(n, out);
}

static void walkAST(ASTNode* node, ASTStats& out) {
    if (!node) return;
    ++out.totalNodes;

    switch (node->kind) {
        // ── Declarations ──────────────────────────────────────────────────────
        case ASTKind::FunctionDecl: {
            ++out.declarations; ++out.funcDecls;
            auto* fn = static_cast<FunctionDeclNode*>(node);
            for (auto* p : fn->params) walkAST(p, out);
            // body is in getChildren()
            break;
        }
        case ASTKind::VariableDecl: {
            ++out.declarations; ++out.varDecls;
            auto* vd = static_cast<VariableDeclNode*>(node);
            walkAST(vd->initExpr, out);
            break;
        }
        case ASTKind::ImportDecl:   ++out.declarations; ++out.importDecls; break;
        case ASTKind::StructDecl:
        case ASTKind::EnumDecl:
        case ASTKind::FfiDecl:      ++out.declarations; break;

        // ── Statements ────────────────────────────────────────────────────────
        case ASTKind::Block:
        case ASTKind::BreakStatement:
        case ASTKind::ContinueStatement:
            ++out.statements;
            break;

        case ASTKind::IfStatement: {
            ++out.statements;
            auto* n2 = static_cast<IfStatementNode*>(node);
            walkAST(n2->condition, out);
            walkAST(n2->thenBranch, out);
            walkAST(n2->elseBranch, out);
            break;
        }
        case ASTKind::WhileStatement: {
            ++out.statements;
            auto* n2 = static_cast<WhileStatementNode*>(node);
            walkAST(n2->condition, out);
            walkAST(n2->body, out);
            break;
        }
        case ASTKind::ForStatement: {
            ++out.statements;
            auto* n2 = static_cast<ForStatementNode*>(node);
            walkAST(n2->init, out);
            walkAST(n2->condition, out);
            walkAST(n2->update, out);
            walkAST(n2->body, out);
            break;
        }
        case ASTKind::DoWhileStatement: {
            ++out.statements;
            auto* n2 = static_cast<DoWhileStatementNode*>(node);
            walkAST(n2->condition, out);
            walkAST(n2->body, out);
            break;
        }
        case ASTKind::ReturnStatement: {
            ++out.statements;
            walkAST(static_cast<ReturnStatementNode*>(node)->value, out);
            break;
        }
        case ASTKind::ThrowStatement: {
            ++out.statements;
            walkAST(static_cast<ThrowStatementNode*>(node)->value, out);
            break;
        }
        case ASTKind::TryStatement: {
            ++out.statements;
            auto* n2 = static_cast<TryStatementNode*>(node);
            walkAST(n2->body, out);
            walkAST(n2->handler, out);
            break;
        }
        case ASTKind::SwitchStatement: {
            ++out.statements;
            auto* sw = static_cast<SwitchStatementNode*>(node);
            walkAST(sw->subject, out);
            for (auto& cas : sw->cases) {
                walkList(cas.values, out);
                walkList(cas.body, out);
            }
            break;
        }
        case ASTKind::ExpressionStatement: {
            ++out.statements;
            walkAST(static_cast<ExpressionStatementNode*>(node)->expression, out);
            break;
        }

        // ── Expressions ───────────────────────────────────────────────────────
        case ASTKind::BinaryExpression: {
            ++out.expressions;
            auto* n2 = static_cast<BinaryExpressionNode*>(node);
            walkAST(n2->Left, out);
            walkAST(n2->Right, out);
            break;
        }
        case ASTKind::UnaryExpression:
            // Şu an ayrı sınıf yok; parser BinaryExpressionNode/PostfixNode kullanır.
            ++out.expressions;
            break;
        case ASTKind::Postfix: {
            ++out.expressions;
            walkAST(static_cast<PostfixNode*>(node)->operand, out);
            break;
        }
        case ASTKind::Call: {
            ++out.expressions;
            auto* c = static_cast<CallExpressionNode*>(node);
            walkAST(c->callee, out);
            walkList(c->arguments, out);
            break;
        }
        case ASTKind::CastExpression: {
            ++out.expressions;
            walkAST(static_cast<CastExpressionNode*>(node)->operand, out);
            break;
        }
        case ASTKind::MemberAccess: {
            ++out.expressions;
            auto* n2 = static_cast<MemberAccessNode*>(node);
            walkAST(n2->object, out);
            // member alanı string — ASTNode* değil, gezilmez
            break;
        }
        case ASTKind::IndexExpression: {
            ++out.expressions;
            auto* n2 = static_cast<IndexExpressionNode*>(node);
            walkAST(n2->object, out);
            walkAST(n2->index, out);
            break;
        }
        case ASTKind::ArrayLiteral: {
            ++out.expressions;
            walkList(static_cast<ArrayLiteralNode*>(node)->elements, out);
            break;
        }
        case ASTKind::ScopeCall: {
            ++out.expressions;
            walkList(static_cast<ScopeCallNode*>(node)->arguments, out);
            break;
        }
        case ASTKind::Literal:
        case ASTKind::Identifier:
            ++out.expressions;
            break;

        // ── Program (root) ────────────────────────────────────────────────────
        case ASTKind::Program:
            break;

        // ── Faz 2: panic-mode kurtarma yer tutucusu — yaprak, children yok ────
        case ASTKind::Error:
            break;
    }
    // getChildren() üzerinden ulaşılabilen çocuklar (Block içindeki stmtler, vb.)
    for (ASTNode* child : node->getChildren())
        walkAST(child, out);
}

inline void collectASTStats(ASTStats& out, const std::vector<ModuleUnit>& units) {
    for (auto& u : units)
        walkAST(u.ast, out);
}

// ── Sembol istatistiklerini topla ────────────────────────────────────────────
#include "symbol/symbol_table.hpp"
#include "symbol/symbol.hpp"

inline void collectSymbolStats(SymbolStats& out, const SymbolTable& table) {
    for (Symbol* s : table.allSymbols()) {
        out.total++;
        switch (s->kind) {
            case SymbolKind::Function:   ++out.functions;  break;
            case SymbolKind::Variable:   ++out.variables;  break;
            case SymbolKind::Parameter:  ++out.parameters; break;
            case SymbolKind::Struct:     ++out.structs;    break;
            case SymbolKind::Enum:
            case SymbolKind::EnumValue:  ++out.enums;      break;
            case SymbolKind::Field:                        break;
        }
    }
}

// ── IR istatistiklerini topla ────────────────────────────────────────────────
#include "ir/ir_program.hpp"
#include "ir/instruction.hpp"

inline void collectIRStats(IRStats& out, const IRProgram& prog) {
    out.funcCount = prog.functions.size();
    // functions: unordered_map<string, IRFunction>
    for (auto& [name, fn] : prog.functions) {
        for (auto& ins : fn.instructions) {
            out.totalInstr++;
            const char* nm = opcodeName(ins.opcode);
            out.staticOpcodes[nm]++;
            if (ins.opcode == Opcode::CALLHOST) ++out.ffiSites;
            if (ins.opcode == Opcode::CALL)      ++out.callSites;
        }
    }
}

// ── Profil yazdır ────────────────────────────────────────────────────────────
inline void printBenchProfile(const BenchProfile& p,
                               uint64_t tokUs, uint64_t parseUs,
                               uint64_t symUs,  uint64_t tcUs,
                               uint64_t irUs,   uint64_t vmUs,
                               bool compileOnly) {
    auto fmtN = [](uint64_t n) -> std::string {
        // Binlik ayraç
        std::string s = std::to_string(n);
        int ins = (int)s.size() - 3;
        while (ins > 0) { s.insert(ins, "."); ins -= 3; }
        return s;
    };
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════╗\n";
    std::cout << "║           saQut Aşama Profil Raporu                 ║\n";
    std::cout << "╚══════════════════════════════════════════════════════╝\n\n";

    // ── Tokenizer ──────────────────────────────────────────────────────────
    auto& t = p.tok;
    std::cout << "┌─ [Tokenizer]  " << fmtN(tokUs) << " µs\n";
    std::cout << "│  Toplam token : " << fmtN(t.total) << "\n";
    std::cout << "│  Dosya sayısı : " << fmtN(t.fileCount) << "\n";
    std::cout << "│  Anahtar kelime: " << fmtN(t.keywords)
              << "   Tanımlayıcı: " << fmtN(t.identifiers)
              << "   Sayı: " << fmtN(t.numbers) << "\n";
    std::cout << "│  Metin literal: " << fmtN(t.strings)
              << "   Operatör: " << fmtN(t.operators_)
              << "   Sınırlayıcı: " << fmtN(t.delimiters) << "\n";
    std::cout << "│\n";

    // ── Parser ─────────────────────────────────────────────────────────────
    auto& a = p.ast;
    std::cout << "├─ [Parser]  " << fmtN(parseUs) << " µs\n";
    std::cout << "│  Toplam düğüm : " << fmtN(a.totalNodes) << "\n";
    std::cout << "│  İfade (expr) : " << fmtN(a.expressions)
              << "   Deyim (stmt): " << fmtN(a.statements)
              << "   Tanım (decl): " << fmtN(a.declarations) << "\n";
    std::cout << "│  Fonksiyon tanımı: " << fmtN(a.funcDecls)
              << "   Değişken tanımı: " << fmtN(a.varDecls)
              << "   Import: " << fmtN(a.importDecls) << "\n";
    std::cout << "│\n";

    // ── Sembol ─────────────────────────────────────────────────────────────
    auto& s = p.sym;
    std::cout << "├─ [Sembol Toplama]  " << fmtN(symUs + tcUs) << " µs\n";
    std::cout << "│  Geçiş sayısı   : " << s.passes << "\n";
    std::cout << "│  Toplam sembol  : " << fmtN(s.total) << "\n";
    std::cout << "│  Fonksiyon: " << fmtN(s.functions)
              << "   Değişken: " << fmtN(s.variables)
              << "   Parametre: " << fmtN(s.parameters)
              << "   Struct: " << fmtN(s.structs) << "\n";
    std::cout << "│\n";

    // ── IR ─────────────────────────────────────────────────────────────────
    auto& ir = p.ir;
    std::cout << "├─ [IR Üretimi]  " << fmtN(irUs) << " µs\n";
    std::cout << "│  Fonksiyon     : " << fmtN(ir.funcCount) << "\n";
    std::cout << "│  Talimat       : " << fmtN(ir.totalInstr) << "\n";
    std::cout << "│  CALL site     : " << fmtN(ir.callSites)
              << "   CALLHOST site: " << fmtN(ir.ffiSites) << "\n";
    std::cout << "│\n";

    // ── VM ─────────────────────────────────────────────────────────────────
    if (!compileOnly) {
        auto& vm = p.vmTrace;
        std::cout << "├─ [VM Çalıştırma]  " << fmtN(vmUs) << " µs\n";
        std::cout << "│  Dispatch döngüsü : " << fmtN(vm.vmLoopIter) << "\n";
        std::cout << "│  saQut CALL       : " << fmtN(vm.vmSaqutCalls) << "\n";
        std::cout << "│  FFI (CALLHOST)   : " << fmtN(vm.vmFfiCalls) << "\n";
        std::cout << "│  Builtin metod    : " << fmtN(vm.vmBuiltinCalls) << "\n";
        std::cout << "│  Heap tahsis      : " << fmtN(p.vmHeapAllocCount) << " nesne\n";
        std::cout << "│  Trace boyutu     : "
                  << fmtN(vm.traceOpcodes.size())
                  << " kayıt (~"
                  << fmtN((vm.traceOpcodes.size() * 9) / 1024)
                  << " KB)\n";
        std::cout << "│\n";
    }

    // ── Opcode profili ─────────────────────────────────────────────────────
    if (!compileOnly && !p.opcodeResult.empty()) {
        std::cout << "└─ [Opcode Profili — çalışma zamanı dağılımı]\n\n";

        // Çalışma sayısına göre sırala (azalan)
        std::vector<std::pair<std::string, OpcodeStats>> sorted(
            p.opcodeResult.begin(), p.opcodeResult.end());
        std::sort(sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) {
                return a.second.count > b.second.count;
            });

        // Toplam tick (% hesabı için)
        uint64_t totalTick = 0;
        for (auto& [_, s] : sorted) totalTick += s.totalTick;

        const int W1 = 22, W2 = 12, W3 = 10, W4 = 10, W5 = 10, W6 = 7;
        std::cout << std::left  << std::setw(W1) << "Opcode"
                  << std::right << std::setw(W2) << "Çalışma"
                  << std::right << std::setw(W3) << "Ort(ns)"
                  << std::right << std::setw(W4) << "Min(ns)"
                  << std::right << std::setw(W5) << "Max(ns)"
                  << std::right << std::setw(W6) << "%Süre"
                  << "\n";
        std::string sep(W1 + W2 + W3 + W4 + W5 + W6, '-');
        std::cout << sep << "\n";

        for (auto& [name, s] : sorted) {
            if (s.count == 0) continue;
            uint64_t avgNs = s.count > 1 ?
                p.ticksToNs(s.totalTick / s.count) : 0;
            uint64_t minNs = (s.minTick != UINT64_MAX) ?
                p.ticksToNs(s.minTick) : 0;
            uint64_t maxNs = p.ticksToNs(s.maxTick);
            double   pct   = totalTick > 0 ?
                100.0 * s.totalTick / totalTick : 0.0;

            std::cout << std::left  << std::setw(W1) << name
                      << std::right << std::setw(W2) << fmtN(s.count)
                      << std::right << std::setw(W3) << avgNs
                      << std::right << std::setw(W4) << minNs
                      << std::right << std::setw(W5) << maxNs
                      << std::right << std::setw(W6-1)
                      << std::fixed << std::setprecision(1) << pct << "%"
                      << "\n";
        }
        std::cout << "\n";
    } else if (compileOnly) {
        std::cout << "└─ (VM atlandı — opcode profili yok)\n\n";
    }
}
