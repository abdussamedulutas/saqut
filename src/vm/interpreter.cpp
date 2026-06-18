#include "vm/interpreter.hpp"
#include <iostream>
#include <stdexcept>

// ─────────────────────────────────────────────────────────────────────────────
// run — Ana yorumlayıcı döngüsü
// ─────────────────────────────────────────────────────────────────────────────

int Interpreter::run() {
    // "main" fonksiyonunu bul
    IRFunction* mainFunction = program_.findFunction("main");
    if (!mainFunction) {
        throw std::runtime_error("Çalışma hatası: 'main' fonksiyonu bulunamadı");
    }

    // main için ilk frame'i oluştur ve stack'e ekle
    CallFrame mainFrame;
    mainFrame.function           = mainFunction;
    mainFrame.instructionPointer = 0;
    mainFrame.slots.resize(mainFunction->slotCount, Value::fromInt(0));
    mainFrame.returnDestSlot     = -1; // caller yok

    callStack_.push_back(std::move(mainFrame));

    // ── Ana döngü ─────────────────────────────────────────────────────────
    // Her iterasyonun başında mevcut frame'i TAZEDEN alırız.
    // CALL ve RETURN callStack'i değiştirir; `continue` ile döngü başına
    // dönülür ve frame yeniden alınır — dangling pointer sorunu olmaz.

    while (!callStack_.empty()) {

        // Her iterasyonda taze referans al (CALL sonrası vector büyüyebilir)
        CallFrame& frame = callStack_.back();

        // Tüm instruction'lar tükendi mi? (RETURN olmadan biten fonksiyon)
        if (frame.instructionPointer >= (int)frame.function->instructions.size()) {
            // void fonksiyon gibi davran — 0 döndür
            int destSlot = frame.returnDestSlot;
            callStack_.pop_back();
            if (!callStack_.empty() && destSlot != -1) {
                callStack_.back().slots[destSlot] = Value::fromInt(0);
            }
            continue;
        }

        // Sıradaki talimatı al ve ip'yi ÖNCE İLERLET.
        // Neden önce? CALL veya RETURN ip'ye dokunmaz. Böylece:
        //   - CALL: yeni frame ip=0 ile açılır, caller'ın ip'si zaten ilerletilmiş.
        //   - RETURN sonrası caller kaldığı yerden (ip zaten doğru) devam eder.
        const Instruction& instr = frame.function->instructions[frame.instructionPointer];
        frame.instructionPointer++;

        // ── Talimat switch'i ──────────────────────────────────────────────
        switch (instr.opcode) {

        // slots[dest] = sabit değer
        case Opcode::LOAD_CONST:
            frame.slots[instr.dest] = Value::fromInt(instr.intValue);
            break;

        // slots[dest] = slots[src]  (kopyala)
        case Opcode::LOAD_SLOT:
            frame.slots[instr.dest] = frame.slots[instr.src];
            break;

        // ── Aritmetik ────────────────────────────────────────────────────
        case Opcode::ADD:
            frame.slots[instr.dest] = Value::fromInt(
                frame.slots[instr.left].intValue + frame.slots[instr.right].intValue);
            break;

        case Opcode::SUB:
            frame.slots[instr.dest] = Value::fromInt(
                frame.slots[instr.left].intValue - frame.slots[instr.right].intValue);
            break;

        case Opcode::MUL:
            frame.slots[instr.dest] = Value::fromInt(
                frame.slots[instr.left].intValue * frame.slots[instr.right].intValue);
            break;

        case Opcode::DIV: {
            int divisor = frame.slots[instr.right].intValue;
            if (divisor == 0) {
                throw std::runtime_error("Çalışma hatası: sıfıra bölme");
            }
            frame.slots[instr.dest] = Value::fromInt(
                frame.slots[instr.left].intValue / divisor);
            break;
        }

        case Opcode::MOD: {
            int divisor = frame.slots[instr.right].intValue;
            if (divisor == 0) {
                throw std::runtime_error("Çalışma hatası: sıfıra bölme (mod)");
            }
            frame.slots[instr.dest] = Value::fromInt(
                frame.slots[instr.left].intValue % divisor);
            break;
        }

        // ── Karşılaştırma (sonuç: 1=doğru, 0=yanlış) ────────────────────
        case Opcode::LESS:
            frame.slots[instr.dest] = Value::fromInt(
                frame.slots[instr.left].intValue < frame.slots[instr.right].intValue ? 1 : 0);
            break;

        case Opcode::LESS_EQUAL:
            frame.slots[instr.dest] = Value::fromInt(
                frame.slots[instr.left].intValue <= frame.slots[instr.right].intValue ? 1 : 0);
            break;

        case Opcode::GREATER:
            frame.slots[instr.dest] = Value::fromInt(
                frame.slots[instr.left].intValue > frame.slots[instr.right].intValue ? 1 : 0);
            break;

        case Opcode::GREATER_EQUAL:
            frame.slots[instr.dest] = Value::fromInt(
                frame.slots[instr.left].intValue >= frame.slots[instr.right].intValue ? 1 : 0);
            break;

        case Opcode::EQUAL_EQUAL:
            frame.slots[instr.dest] = Value::fromInt(
                frame.slots[instr.left].intValue == frame.slots[instr.right].intValue ? 1 : 0);
            break;

        case Opcode::NOT_EQUAL:
            frame.slots[instr.dest] = Value::fromInt(
                frame.slots[instr.left].intValue != frame.slots[instr.right].intValue ? 1 : 0);
            break;

        // ── Kontrol akışı ─────────────────────────────────────────────────

        // Koşulsuz atlama
        case Opcode::JMP:
            frame.instructionPointer = instr.jumpTarget;
            break;

        // Koşullu atlama: slot[cond] == 0 (yanlış) ise atla
        case Opcode::JIF_FALSE:
            if (!frame.slots[instr.cond].isTruthy()) {
                frame.instructionPointer = instr.jumpTarget;
            }
            break;

        // ── Fonksiyon çağrısı ─────────────────────────────────────────────
        // Yeni frame oluştur, argümanları parametre slotlarına kopyala,
        // stack'e ekle. `continue` ile döngü başına dön — yeni frame çalışmaya başlar.
        case Opcode::CALL: {
            IRFunction* callee = program_.findFunction(instr.functionName);
            if (!callee) {
                throw std::runtime_error(
                    "Çalışma hatası: '" + instr.functionName + "' fonksiyonu bulunamadı");
            }

            // Yeni frame hazırla
            CallFrame newFrame;
            newFrame.function           = callee;
            newFrame.instructionPointer = 0;
            newFrame.slots.resize(callee->slotCount, Value::fromInt(0));
            newFrame.returnDestSlot     = instr.dest; // sonuç bu slota yazılacak

            // Argümanları parametre slotlarına kopyala (slot 0, 1, 2, ...)
            for (int i = 0; i < (int)instr.argSlots.size(); i++) {
                newFrame.slots[i] = frame.slots[instr.argSlots[i]];
            }

            // Frame'i stack'e ekle — SONRA `continue` ile döngü başına dön.
            // Böylece bir sonraki iterasyonda bu yeni frame çalışmaya başlar.
            callStack_.push_back(std::move(newFrame));
            continue; // ← frame referansı burada yenilenir, dangling pointer yok
        }

        // ── Dönüş ─────────────────────────────────────────────────────────
        // Dönüş değerini caller'ın beklediği slota yaz, bu frame'i kapat.
        case Opcode::RETURN: {
            Value returnValue    = frame.slots[instr.src];
            int   returnDestSlot = frame.returnDestSlot;

            // Bu frame'i kapat
            callStack_.pop_back();

            // Caller varsa dönüş değerini onun slotuna yaz
            if (!callStack_.empty() && returnDestSlot != -1) {
                callStack_.back().slots[returnDestSlot] = returnValue;
            }

            // main fonksiyonu döndü → program bitti
            if (callStack_.empty()) {
                return returnValue.intValue;
            }

            continue; // ← bir sonraki iterasyonda caller frame tazeden alınır
        }

        // ── FFI: Host fonksiyon çağrısı ───────────────────────────────────
        case Opcode::CALLHOST:
            executeHostFunction(instr.functionName, frame.slots, instr.argSlots);
            break;
        }
    }

    return 0; // Normal çıkış
}

// ─────────────────────────────────────────────────────────────────────────────
// executeHostFunction — C++ tarafında tanımlı fonksiyonları çağır
// ─────────────────────────────────────────────────────────────────────────────

void Interpreter::executeHostFunction(const std::string&      name,
                                       const std::vector<Value>& slots,
                                       const std::vector<int>&   argSlots) {
    if (name == "print") {
        // print(değer) — stdout'a değeri yazdır
        if (!argSlots.empty()) {
            std::cout << slots[argSlots[0]].intValue << "\n";
        }
        return;
    }

    // Bilinmeyen host fonksiyon
    throw std::runtime_error("Çalışma hatası: bilinmeyen host fonksiyonu '" + name + "'");
}
