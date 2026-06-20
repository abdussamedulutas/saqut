#include "vm/interpreter.hpp"
#include "vm/object.hpp"
#include <iostream>
#include <stdexcept>

int Interpreter::run() {
    // Global slot'ları sıfırla
    globalSlots_.assign(program_.globalCount, Value::fromInt(0));

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

        // ── Bitsel ────────────────────────────────────────────────────────
        case Opcode::BAND:
            frame.slots[instr.dest] = Value::fromInt(
                frame.slots[instr.left].intValue & frame.slots[instr.right].intValue);
            break;
        case Opcode::BOR:
            frame.slots[instr.dest] = Value::fromInt(
                frame.slots[instr.left].intValue | frame.slots[instr.right].intValue);
            break;
        case Opcode::SHL:
            frame.slots[instr.dest] = Value::fromInt(
                frame.slots[instr.left].intValue << frame.slots[instr.right].intValue);
            break;
        case Opcode::SHR:
            frame.slots[instr.dest] = Value::fromInt(
                frame.slots[instr.left].intValue >> frame.slots[instr.right].intValue);
            break;
        case Opcode::BNOT:
            frame.slots[instr.dest] = Value::fromInt(~frame.slots[instr.src].intValue);
            break;

        // ── Global değişken erişimi ────────────────────────────────────────
        case Opcode::LOAD_GLOBAL:
            frame.slots[instr.dest] = globalSlots_[instr.intValue];
            break;
        case Opcode::STORE_GLOBAL:
            globalSlots_[instr.intValue] = frame.slots[instr.src];
            break;

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
            int r;
            if (lv.kind == ValueKind::Ref || rv.kind == ValueKind::Ref)
                r = (lv.ref == rv.ref ? 1 : 0); // ADR-023: array/struct kimlik karşılaştırması
            else if (lv.kind == ValueKind::String)
                r = (lv.stringValue == rv.stringValue ? 1 : 0);
            else
                r = (lv.intValue == rv.intValue ? 1 : 0);
            frame.slots[instr.dest] = Value::fromInt(r);
            break;
        }
        case Opcode::NOT_EQUAL: {
            auto& lv = frame.slots[instr.left]; auto& rv = frame.slots[instr.right];
            int r;
            if (lv.kind == ValueKind::Ref || rv.kind == ValueKind::Ref)
                r = (lv.ref != rv.ref ? 1 : 0);
            else if (lv.kind == ValueKind::String)
                r = (lv.stringValue != rv.stringValue ? 1 : 0);
            else
                r = (lv.intValue != rv.intValue ? 1 : 0);
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

        // ── Array (ADR-020: referans semantiği) ───────────────────────────
        case Opcode::ARRAY_NEW: {
            ArrayObject* arr = heap_.allocArray(instr.intValue);
            arr->elements.resize(instr.intValue, Value::fromInt(0));
            frame.slots[instr.dest] = Value::fromRef(arr);
            break;
        }
        case Opcode::ARRAY_GET: {
            Value& arrVal = frame.slots[instr.left];
            if (arrVal.kind != ValueKind::Ref || !arrVal.ref)
                throw std::runtime_error("Çalışma hatası: dizi değil");
            auto* arr = (ArrayObject*)arrVal.ref;
            int idx = frame.slots[instr.right].intValue;
            if (idx < 0 || idx >= (int)arr->elements.size())
                throw std::runtime_error(
                    "Çalışma hatası: dizi sınır dışı (indeks=" + std::to_string(idx) +
                    ", uzunluk=" + std::to_string(arr->elements.size()) + ")");
            frame.slots[instr.dest] = arr->elements[idx];
            break;
        }
        case Opcode::ARRAY_SET: {
            Value& arrVal = frame.slots[instr.dest];
            if (arrVal.kind != ValueKind::Ref || !arrVal.ref)
                throw std::runtime_error("Çalışma hatası: dizi değil");
            auto* arr = (ArrayObject*)arrVal.ref;
            int idx = frame.slots[instr.left].intValue;
            if (idx < 0 || idx >= (int)arr->elements.size())
                throw std::runtime_error(
                    "Çalışma hatası: dizi sınır dışı (indeks=" + std::to_string(idx) +
                    ", uzunluk=" + std::to_string(arr->elements.size()) + ")");
            arr->elements[idx] = frame.slots[instr.right];
            break;
        }
        case Opcode::ARRAY_LEN: {
            Value& arrVal = frame.slots[instr.src];
            if (arrVal.kind != ValueKind::Ref || !arrVal.ref)
                throw std::runtime_error("Çalışma hatası: dizi değil");
            auto* arr = (ArrayObject*)arrVal.ref;
            frame.slots[instr.dest] = Value::fromInt((int)arr->elements.size());
            break;
        }

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
