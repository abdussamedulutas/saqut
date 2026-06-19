#include "vm/interpreter.hpp"
#include <iostream>
#include <stdexcept>

int Interpreter::run() {
    IRFunction* mainFunction = program_.findFunction("main");
    if (!mainFunction)
        throw std::runtime_error("Çalışma hatası: 'main' fonksiyonu bulunamadı");

    CallFrame mainFrame;
    mainFrame.function           = mainFunction;
    mainFrame.instructionPointer = 0;
    mainFrame.slots.resize(mainFunction->slotCount, Value::fromInt(0));
    mainFrame.returnDestSlot     = -1;
    callStack_.push_back(std::move(mainFrame));

    while (!callStack_.empty()) {
        CallFrame& frame = callStack_.back();

        if (frame.instructionPointer >= (int)frame.function->instructions.size()) {
            int destSlot = frame.returnDestSlot;
            callStack_.pop_back();
            if (!callStack_.empty() && destSlot != -1)
                callStack_.back().slots[destSlot] = Value::fromInt(0);
            continue;
        }

        const Instruction& instr = frame.function->instructions[frame.instructionPointer];
        frame.instructionPointer++;

        switch (instr.opcode) {

        case Opcode::LOAD_CONST:
            frame.slots[instr.dest] = Value::fromInt(instr.intValue);
            break;

        case Opcode::LOAD_STRING:
            frame.slots[instr.dest] = Value::fromString(instr.stringValue);
            break;

        case Opcode::LOAD_SLOT:
            frame.slots[instr.dest] = frame.slots[instr.src];
            break;

        // ── Aritmetik ─────────────────────────────────────────────────────
        // TypeChecker derleme zamanında tipleri doğruladı — burada sadece hesap yapılır.
        // İstisna: sıfıra bölme gerçek bir çalışma zamanı koşuludur, kontrol edilir.
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
            int d = frame.slots[instr.right].intValue;
            if (d == 0) throw std::runtime_error("Çalışma hatası: sıfıra bölme");
            frame.slots[instr.dest] = Value::fromInt(frame.slots[instr.left].intValue / d);
            break;
        }
        case Opcode::MOD: {
            int d = frame.slots[instr.right].intValue;
            if (d == 0) throw std::runtime_error("Çalışma hatası: sıfıra bölme (mod)");
            frame.slots[instr.dest] = Value::fromInt(frame.slots[instr.left].intValue % d);
            break;
        }

        // ── Karşılaştırma ─────────────────────────────────────────────────
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
        case Opcode::EQUAL_EQUAL: {
            auto& lv = frame.slots[instr.left]; auto& rv = frame.slots[instr.right];
            int r = (lv.kind == ValueKind::String)
                ? (lv.stringValue == rv.stringValue ? 1 : 0)
                : (lv.intValue    == rv.intValue    ? 1 : 0);
            frame.slots[instr.dest] = Value::fromInt(r);
            break;
        }
        case Opcode::NOT_EQUAL: {
            auto& lv = frame.slots[instr.left]; auto& rv = frame.slots[instr.right];
            int r = (lv.kind == ValueKind::String)
                ? (lv.stringValue != rv.stringValue ? 1 : 0)
                : (lv.intValue    != rv.intValue    ? 1 : 0);
            frame.slots[instr.dest] = Value::fromInt(r);
            break;
        }

        // ── Kontrol akışı ─────────────────────────────────────────────────
        case Opcode::JMP:
            frame.instructionPointer = instr.jumpTarget;
            break;
        case Opcode::JIF_FALSE:
            if (!frame.slots[instr.cond].isTruthy())
                frame.instructionPointer = instr.jumpTarget;
            break;
        case Opcode::JIF_TRUE:
            if (frame.slots[instr.cond].isTruthy())
                frame.instructionPointer = instr.jumpTarget;
            break;

        // ── Fonksiyon çağrısı ─────────────────────────────────────────────
        case Opcode::CALL: {
            IRFunction* callee = program_.findFunction(instr.functionName);
            if (!callee)
                throw std::runtime_error(
                    "Çalışma hatası: '" + instr.functionName + "' fonksiyonu bulunamadı");

            CallFrame newFrame;
            newFrame.function           = callee;
            newFrame.instructionPointer = 0;
            newFrame.slots.resize(callee->slotCount, Value::fromInt(0));
            newFrame.returnDestSlot     = instr.dest;

            for (int i = 0; i < (int)instr.argSlots.size(); i++)
                newFrame.slots[i] = frame.slots[instr.argSlots[i]];

            callStack_.push_back(std::move(newFrame));
            continue;
        }

        // ── Dönüş ─────────────────────────────────────────────────────────
        case Opcode::RETURN: {
            Value returnValue    = frame.slots[instr.src];
            int   returnDestSlot = frame.returnDestSlot;
            callStack_.pop_back();

            if (!callStack_.empty() && returnDestSlot != -1)
                callStack_.back().slots[returnDestSlot] = returnValue;

            if (callStack_.empty())
                return returnValue.intValue;

            continue;
        }

        // ── Bitsel ────────────────────────────────────────────────────────
        case Opcode::BIT_AND:
            frame.slots[instr.dest] = Value::fromInt(
                frame.slots[instr.left].intValue & frame.slots[instr.right].intValue);
            break;
        case Opcode::BIT_OR:
            frame.slots[instr.dest] = Value::fromInt(
                frame.slots[instr.left].intValue | frame.slots[instr.right].intValue);
            break;
        case Opcode::BIT_SHL:
            frame.slots[instr.dest] = Value::fromInt(
                frame.slots[instr.left].intValue << frame.slots[instr.right].intValue);
            break;
        case Opcode::BIT_SHR:
            frame.slots[instr.dest] = Value::fromInt(
                frame.slots[instr.left].intValue >> frame.slots[instr.right].intValue);
            break;
        case Opcode::BIT_NOT:
            frame.slots[instr.dest] = Value::fromInt(~frame.slots[instr.src].intValue);
            break;
        case Opcode::NOT_UNARY:
            frame.slots[instr.dest] = Value::fromInt(
                frame.slots[instr.src].intValue == 0 ? 1 : 0);
            break;

        // ── FFI ───────────────────────────────────────────────────────────
        case Opcode::CALLHOST:
            executeHostFunction(instr.functionName, frame.slots, instr.argSlots);
            break;
        }
    }

    return 0;
}

void Interpreter::executeHostFunction(const std::string&       name,
                                       const std::vector<Value>& slots,
                                       const std::vector<int>&   argSlots) {
    if (name == "print") {
        if (!argSlots.empty()) {
            const Value& val = slots[argSlots[0]];
            if (val.kind == ValueKind::String) std::cout << val.stringValue << "\n";
            else                               std::cout << val.intValue    << "\n";
        }
        return;
    }
    throw std::runtime_error("Çalışma hatası: bilinmeyen host fonksiyonu '" + name + "'");
}
