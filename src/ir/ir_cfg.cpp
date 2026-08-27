// ============================================================================
// saQut IR — CFG Builder Gerçeklemesi
// ============================================================================
//
// #218, ADR-039
// Flat instruction list → CFG dönüşümü.
//
// Algoritma:
//   1. Blok liderlerini bul (instruction 0, jump hedefleri, terminator sonrası)
//   2. Liderler arasını BasicBlock'lara böl
//   3. Block'lar arası edge'leri kur (predecessor/successor)
//
// ============================================================================

#include "ir/ir_cfg.hpp"
#include <algorithm>
#include <set>

static bool isTerminator(Opcode op) {
    return op == Opcode::RETURN || op == Opcode::THROW;
}

static bool isJump(Opcode op) {
    return op == Opcode::JMP || op == Opcode::JIF_FALSE || op == Opcode::JIF_TRUE;
}

// Blok liderlerini bul
static std::vector<int> findLeaders(const std::vector<Instruction>& insns) {
    std::set<int> leaderSet;
    if (insns.empty()) return {};

    leaderSet.insert(0);

    for (int i = 0; i < (int)insns.size(); ++i) {
        const auto& ins = insns[i];

        if (isTerminator(ins.opcode)) {
            if (i + 1 < (int)insns.size())
                leaderSet.insert(i + 1);
        }

        // JMP sonrası da lider: JMP'ten sonra gelen, başka hiçbir kenarla
        // hedeflenmemiş talimatlar ölü koddur ve KENDİ bloğunda durmalıdır
        // (yoksa removeUnreachableBlocks onları blokla birlikte korurdu).
        if (ins.opcode == Opcode::JMP) {
            if (i + 1 < (int)insns.size())
                leaderSet.insert(i + 1);
        }

        // ENTER_TRY'nin catch hedefi lider: exception kenarının ucu.
        if (ins.opcode == Opcode::ENTER_TRY && ins.jumpTarget >= 0)
            leaderSet.insert(ins.jumpTarget);

        if (isJump(ins.opcode) && ins.jumpTarget >= 0) {
            leaderSet.insert(ins.jumpTarget);
        }

        if (ins.opcode == Opcode::JIF_FALSE || ins.opcode == Opcode::JIF_TRUE) {
            if (i + 1 < (int)insns.size())
                leaderSet.insert(i + 1);
        }
    }

    return std::vector<int>(leaderSet.begin(), leaderSet.end());
}

CFG buildCFG(const std::vector<Instruction>& instructions) {
    CFG cfg;
    if (instructions.empty()) return cfg;

    auto leaders = findLeaders(instructions);

    // Bloklara böl
    for (size_t li = 0; li < leaders.size(); ++li) {
        BasicBlock block;
        block.id = (int)cfg.blocks.size();
        block.startIndex = leaders[li];
        block.endIndex = (li + 1 < leaders.size()) ? leaders[li + 1] : (int)instructions.size();

        for (int i = block.startIndex; i < block.endIndex; ++i)
            block.instructions.push_back(instructions[i]);

        if (!block.instructions.empty()) {
            const auto& last = block.instructions.back();
            block.terminator = last.opcode;
            block.jumpTarget = last.jumpTarget;
        }

        cfg.blocks.push_back(std::move(block));
    }

    // Edge'leri kur. jumpTarget anlamı: buildCFG girdisinde TALİMAT İNDEKSİ,
    // çözümlendikten sonra BLOCK ID (block.jumpTarget). linearize() blok ID'yi
    // talimat indeksine geri çevirir; dump() "->BB_N" doğru blok ID basar.
    // (#218; düzeltme: --cfg görüntüleyici öncesi anlam karışıklığı kapatıldı)
    auto resolveTarget = [&cfg](BasicBlock& block, int instrIdx) {
        for (auto& target : cfg.blocks) {
            if (target.startIndex == instrIdx) {
                block.successors.push_back(target.id);
                target.predecessors.push_back(block.id);
                block.jumpTarget = target.id;
                return;
            }
        }
    };
    for (auto& block : cfg.blocks) {
        // Exception kenarları: ENTER_TRY taşıyan blok → catch bloğu. Bloğun
        // ortasından çıkan kenar olduğundan successors listesine eklenir ama
        // terminator'i değiştirmez (akış blok içinde devam eder). Hedef
        // exceptionTargets'a talimat sırasıyla kaydedilir — linearize()
        // ENTER_TRY'nin jumpTarget'ını buradan yazar.
        for (const auto& ins : block.instructions) {
            if (ins.opcode != Opcode::ENTER_TRY || ins.jumpTarget < 0) continue;
            for (auto& target : cfg.blocks) {
                if (target.startIndex == ins.jumpTarget) {
                    block.successors.push_back(target.id);
                    target.predecessors.push_back(block.id);
                    block.exceptionTargets.push_back(target.id);
                    break;
                }
            }
        }

        if (block.terminator == Opcode::JMP) {
            resolveTarget(block, block.jumpTarget);
        } else if (block.terminator == Opcode::JIF_FALSE ||
                   block.terminator == Opcode::JIF_TRUE) {
            // Fall-through successor
            if (block.id + 1 < (int)cfg.blocks.size()) {
                block.successors.push_back(block.id + 1);
                cfg.blocks[block.id + 1].predecessors.push_back(block.id);
            }
            // Jump successor
            resolveTarget(block, block.jumpTarget);
        } else if (block.terminator != Opcode::RETURN &&
                   block.terminator != Opcode::THROW) {
            // Implicit fall-through: sıradan talimatla biten blok (ör. init
            // bloğu) sonraki bloğa akar — eksik kenar hatası kapatıldı.
            if (block.id + 1 < (int)cfg.blocks.size()) {
                block.successors.push_back(block.id + 1);
                cfg.blocks[block.id + 1].predecessors.push_back(block.id);
            }
        }
    }

    return cfg;
}

// ── Erişilemez blok temizliği ────────────────────────────────────────────────
// Blok 0'dan successors üzerinden (exception kenarları dahil) BFS. Ulaşıla-
// mayanlar silinir; kalan bloklar yeniden numaralanır ve kenarlar/id alanları
// yeni ID'lere çevrilir. startIndex/endIndex orijinal flat düzene referans
// kalır (yalnızca kurulum sırasında anlamlıdır); linearize blok sırasını
// kullanır, bu alanları değil.
int CFG::removeUnreachableBlocks() {
    if (blocks.empty()) return 0;

    std::vector<char> reachable(blocks.size(), 0);
    std::vector<int> work{0};
    reachable[0] = 1;
    while (!work.empty()) {
        int b = work.back(); work.pop_back();
        for (int s : blocks[(size_t)b].successors) {
            if (!reachable[(size_t)s]) {
                reachable[(size_t)s] = 1;
                work.push_back(s);
            }
        }
    }

    int removed = 0;
    for (char r : reachable) if (!r) ++removed;
    if (removed == 0) return 0;

    // Eski ID → yeni ID eşlemesi; silinenler -1.
    std::vector<int> newId(blocks.size(), -1);
    std::vector<BasicBlock> kept;
    kept.reserve(blocks.size() - (size_t)removed);
    for (size_t i = 0; i < blocks.size(); ++i) {
        if (!reachable[i]) continue;
        newId[i] = (int)kept.size();
        kept.push_back(std::move(blocks[i]));
    }

    auto remap = [&](std::vector<int>& v) {
        std::vector<int> out;
        out.reserve(v.size());
        for (int x : v)
            if (x >= 0 && newId[(size_t)x] >= 0) out.push_back(newId[(size_t)x]);
        v = std::move(out);
    };

    for (auto& b : kept) {
        b.id = newId[(size_t)b.id];
        if (b.jumpTarget >= 0) b.jumpTarget = newId[(size_t)b.jumpTarget];
        remap(b.predecessors);
        remap(b.successors);
        remap(b.exceptionTargets);
    }

    blocks = std::move(kept);
    idom.clear(); rpo.clear(); loops.clear();  // ID'ler değişti — yeniden hesap
    return removed;
}

// ── Dominance (Cooper-Harvey-Kennedy) ────────────────────────────────────────
// RPO üzerinden iteratif intersect; yakın-doğrusal pratik performans. Tüm
// bloklar erişilebilir varsayar (önce removeUnreachableBlocks).
void CFG::computeDominance() {
    const int n = (int)blocks.size();
    idom.assign((size_t)n, -1);
    rpo.clear();
    if (n == 0) return;

    // RPO: DFS postorder'ın tersi. Successor'lar blok sırasıyla gezilir —
    // deterministik sonuç (ADR-038 gereği analiz de tekrarlanabilir olmalı).
    std::vector<char> visited((size_t)n, 0);
    std::vector<int> post;
    std::vector<std::pair<int, size_t>> stack;  // (blok, sonraki succ indeksi)
    visited[0] = 1;
    stack.emplace_back(0, 0);
    while (!stack.empty()) {
        auto& [b, si] = stack.back();
        if (si < blocks[(size_t)b].successors.size()) {
            int s = blocks[(size_t)b].successors[si++];
            if (!visited[(size_t)s]) {
                visited[(size_t)s] = 1;
                stack.emplace_back(s, 0);
            }
        } else {
            post.push_back(b);
            stack.pop_back();
        }
    }
    rpo.assign(post.rbegin(), post.rend());

    auto intersect = [&](int b, int p) {
        while (b != p) {
            while (b > p) b = idom[(size_t)b];
            while (p > b) p = idom[(size_t)p];
        }
        return b;
    };

    idom[0] = 0;  // giriş: kendisi (kök işareti; -1 yerine döngüsüz intersect)
    bool changed = true;
    while (changed) {
        changed = false;
        for (int b : rpo) {
            if (b == 0) continue;
            int newIdom = -1;
            for (int p : blocks[(size_t)b].predecessors) {
                if (idom[(size_t)p] < 0) continue;  // henüz işlenmemiş
                newIdom = (newIdom < 0) ? p : intersect(p, newIdom);
            }
            if (newIdom >= 0 && idom[(size_t)b] != newIdom) {
                idom[(size_t)b] = newIdom;
                changed = true;
            }
        }
    }
    idom[0] = -1;  // kökün dominatörü yoktur
}

bool cfgDominates(const CFG& cfg, int a, int b) {
    // a, b'yi dominate ediyor mu: b'den idom zinciri yukarı çık, a'ya uğra.
    if (a < 0 || b < 0 || a >= (int)cfg.idom.size() || b >= (int)cfg.idom.size())
        return false;
    int cur = b;
    while (cur >= 0) {
        if (cur == a) return true;
        cur = cfg.idom[(size_t)cur];
    }
    return false;
}

// ── Natural loop tespiti ─────────────────────────────────────────────────────
// Geri kenar u→h (h, u'yu dominate eder). Gövde: u'dan pred'ler üzerinden
// geriye yürü, h'ye ulaşınca dur (h dahil).
void CFG::computeLoops() {
    loops.clear();
    if (idom.empty()) computeDominance();
    if (blocks.empty()) return;

    for (int u = 0; u < (int)blocks.size(); ++u) {
        for (int h : blocks[(size_t)u].successors) {
            if (!cfgDominates(*this, h, u)) continue;  // ileri kenar

            NaturalLoop loop;
            loop.header = h;
            std::vector<char> inLoop(blocks.size(), 0);
            inLoop[(size_t)h] = 1;
            std::vector<int> work;
            if (u != h) {  // kendine kenar: gövde yalnızca h, pred yürünmez
                inLoop[(size_t)u] = 1;
                work.push_back(u);
            }
            while (!work.empty()) {
                int b = work.back(); work.pop_back();
                for (int p : blocks[(size_t)b].predecessors) {
                    if (!inLoop[(size_t)p]) {
                        inLoop[(size_t)p] = 1;
                        work.push_back(p);
                    }
                }
            }
            for (size_t b = 0; b < blocks.size(); ++b)
                if (inLoop[b]) loop.body.push_back((int)b);
            loops.push_back(std::move(loop));
        }
    }
}
