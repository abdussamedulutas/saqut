#include "vm/interpreter.hpp"
#include "vm/object.hpp"
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <cmath>
#include <climits>

// ── buildTrace ─────────────────────────────────────────────────────────────────
// Mevcut callStack_'i en içten dışa gezerek stacktrace string'i üretir.
// pendingThrow_ set edilmeden (unwind olmadan) önce çağrılmalıdır.
std::string Interpreter::buildTrace() const {
    std::string result;
    for (int i = (int)callStack_.size() - 1; i >= 0; --i) {
        const CallFrame& f = callStack_[i];
        if (!f.function) continue;
        // ip zaten artırılmış olduğundan şu an çalışan instruction = ip - 1
        int ip = f.instructionPointer - 1;
        if (ip >= 0 && ip < (int)f.function->instructions.size()) {
            const Instruction& ins = f.function->instructions[ip];
            if (ins.sourceLine > 0) {
                const std::string& file =
                    program_.moduleRegistry.filePath(f.function->moduleId);
                result += f.function->name + " (" + file + ":" +
                          std::to_string(ins.sourceLine) + ":" +
                          std::to_string(ins.sourceCol)  + ")\n";
            } else {
                result += f.function->name + " (?)\n";
            }
        } else {
            result += f.function->name + " (?)\n";
        }
    }
    return result;
}

// ── makeErrorValue ─────────────────────────────────────────────────────────────
// ADR-025: Error struct oluşturur — alan sırası: [line, col, message, trace, code]
Value Interpreter::makeErrorValue(const std::string& message,
                                   const std::string& code,
                                   int line, int col) {
    StructObject* obj = heap_.allocStruct(5);
    obj->fields[0] = Value::fromInt(line);
    obj->fields[1] = Value::fromInt(col);
    obj->fields[2] = Value::fromString(message);
    obj->fields[3] = Value::fromString(buildTrace());
    obj->fields[4] = Value::fromString(code);
    return Value::fromRef(obj);
}

int Interpreter::run() {
    // Her modülün global slot vektörünü başlat (key = int moduleId)
    for (auto& [id, count] : program_.moduleGlobalCounts)
        moduleSlots_[id].assign(count, Value::fromInt(0));
    // Geriye dönük uyumluluk: moduleGlobalCounts boşsa globalCount'u kullan
    if (program_.moduleGlobalCounts.empty() && program_.globalCount > 0)
        moduleSlots_[ModuleRegistry::INVALID_ID].assign(
            program_.globalCount, Value::fromInt(0));

    IRFunction* mainFunction = program_.findFunction("main");
    if (!mainFunction)
        throw std::runtime_error("runtime error: 'main' function not found");

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

        case Opcode::LOAD_NULL:
            frame.slots[instr.dest] = Value::null();
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
            if (d == 0) { pendingThrow_ = makeErrorValue("division by zero", "E_DIVZERO"); break; }
            frame.slots[instr.dest] = Value::fromInt(frame.slots[instr.left].intValue / d);
            break;
        }
        case Opcode::MOD: {
            int d = frame.slots[instr.right].intValue;
            if (d == 0) { pendingThrow_ = makeErrorValue("division by zero (modulo)", "E_DIVZERO"); break; }
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
            frame.slots[instr.dest] =
                moduleSlots_[frame.function->moduleId][instr.intValue];
            break;
        case Opcode::STORE_GLOBAL:
            moduleSlots_[frame.function->moduleId][instr.intValue] =
                frame.slots[instr.src];
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
            // ADR-021/027: null kind ayrı işlenir — null yalnızca null'a eşittir
            if (lv.kind == ValueKind::Null && rv.kind == ValueKind::Null)
                r = 1;
            else if (lv.kind == ValueKind::Null || rv.kind == ValueKind::Null)
                r = 0;
            else if (lv.kind == ValueKind::Ref || rv.kind == ValueKind::Ref)
                r = (lv.ref == rv.ref ? 1 : 0); // ADR-023: array/struct kimlik
            else if (lv.kind == ValueKind::String)
                r = (lv.stringValue == rv.stringValue ? 1 : 0);
            else if (lv.kind == ValueKind::Float || rv.kind == ValueKind::Float)
                r = (lv.floatValue == rv.floatValue ? 1 : 0);
            else
                r = (lv.intValue == rv.intValue ? 1 : 0);
            frame.slots[instr.dest] = Value::fromInt(r);
            break;
        }
        case Opcode::NOT_EQUAL: {
            auto& lv = frame.slots[instr.left]; auto& rv = frame.slots[instr.right];
            int r;
            if (lv.kind == ValueKind::Null && rv.kind == ValueKind::Null)
                r = 0;
            else if (lv.kind == ValueKind::Null || rv.kind == ValueKind::Null)
                r = 1;
            else if (lv.kind == ValueKind::Ref || rv.kind == ValueKind::Ref)
                r = (lv.ref != rv.ref ? 1 : 0);
            else if (lv.kind == ValueKind::String)
                r = (lv.stringValue != rv.stringValue ? 1 : 0);
            else if (lv.kind == ValueKind::Float || rv.kind == ValueKind::Float)
                r = (lv.floatValue != rv.floatValue ? 1 : 0);
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
                    "runtime error: '" + instr.functionName + "' function not found");

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
        //
        // GC safepoint: frame pop'landıktan SONRA, dönüş değeri caller'a
        // yazıldıktan SONRA tetiklenir — kök kümesi bu noktada kararlıdır.
        // Döngüsel referanslar dahil erişilemeyen her nesne burada toplanır.
        case Opcode::RETURN: {
            Value returnValue    = frame.slots[instr.src];
            int   returnDestSlot = frame.returnDestSlot;
            callStack_.pop_back();

            if (!callStack_.empty() && returnDestSlot != -1)
                callStack_.back().slots[returnDestSlot] = returnValue;

            // Tüm modüllerin global slotlarını kök olarak ver
            std::vector<Value> allGlobals;
            for (auto& [id, slots] : moduleSlots_)
                allGlobals.insert(allGlobals.end(), slots.begin(), slots.end());
            heap_.collect(allGlobals, callStack_);

            if (callStack_.empty())
                return returnValue.intValue;

            continue;
        }

        // ── Float aritmetik (#44) ─────────────────────────────────────────
        case Opcode::LOAD_FLOAT:
            frame.slots[instr.dest] = Value::fromFloat(instr.floatValue);
            break;
        case Opcode::FADD:
            frame.slots[instr.dest] = Value::fromFloat(
                frame.slots[instr.left].floatValue + frame.slots[instr.right].floatValue);
            break;
        case Opcode::FSUB:
            frame.slots[instr.dest] = Value::fromFloat(
                frame.slots[instr.left].floatValue - frame.slots[instr.right].floatValue);
            break;
        case Opcode::FMUL:
            frame.slots[instr.dest] = Value::fromFloat(
                frame.slots[instr.left].floatValue * frame.slots[instr.right].floatValue);
            break;
        case Opcode::FDIV: {
            double r = frame.slots[instr.right].floatValue;
            if (r == 0.0) { pendingThrow_ = makeErrorValue("float division by zero", "E_DIVZERO"); break; }
            frame.slots[instr.dest] = Value::fromFloat(frame.slots[instr.left].floatValue / r);
            break;
        }
        case Opcode::FNEG:
            frame.slots[instr.dest] = Value::fromFloat(-frame.slots[instr.src].floatValue);
            break;
        case Opcode::INT_TO_FLOAT:
            frame.slots[instr.dest] = Value::fromFloat((double)frame.slots[instr.src].intValue);
            break;
        case Opcode::FLOAT_TO_INT:
            frame.slots[instr.dest] = Value::fromInt((int)frame.slots[instr.src].floatValue);
            break;

        // ── Struct (ADR-020: referans semantiği) ──────────────────────────
        case Opcode::STRUCT_NEW: {
            StructObject* obj = heap_.allocStruct(instr.intValue);
            frame.slots[instr.dest] = Value::fromRef(obj);
            break;
        }
        case Opcode::FIELD_GET: {
            Value& objVal = frame.slots[instr.src];
            if (objVal.kind != ValueKind::Ref || !objVal.ref)
                throw std::runtime_error("runtime error: not a struct");
            auto* obj = (StructObject*)objVal.ref;
            int idx = instr.intValue;
            if (idx < 0 || idx >= (int)obj->fields.size())
                throw std::runtime_error("runtime error: invalid struct field index " + std::to_string(idx));
            frame.slots[instr.dest] = obj->fields[idx];
            break;
        }
        case Opcode::FIELD_SET: {
            Value& objVal = frame.slots[instr.dest];
            if (objVal.kind != ValueKind::Ref || !objVal.ref)
                throw std::runtime_error("runtime error: not a struct");
            auto* obj = (StructObject*)objVal.ref;
            int idx = instr.intValue;
            if (idx < 0 || idx >= (int)obj->fields.size())
                throw std::runtime_error("runtime error: invalid struct field index " + std::to_string(idx));
            obj->fields[idx] = frame.slots[instr.right];
            break;
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
            if (arrVal.kind != ValueKind::Ref || !arrVal.ref) {
                pendingThrow_ = makeErrorValue("expected array, got different type", "E_TYPE"); break;
            }
            auto* arr = (ArrayObject*)arrVal.ref;
            int idx = frame.slots[instr.right].intValue;
            if (idx < 0 || idx >= (int)arr->elements.size()) {
                pendingThrow_ = makeErrorValue(
                    "array index out of bounds (index=" + std::to_string(idx) +
                    ", length=" + std::to_string(arr->elements.size()) + ")", "E_OOB");
                break;
            }
            frame.slots[instr.dest] = arr->elements[idx];
            break;
        }
        case Opcode::ARRAY_SET: {
            Value& arrVal = frame.slots[instr.dest];
            if (arrVal.kind != ValueKind::Ref || !arrVal.ref) {
                pendingThrow_ = makeErrorValue("expected array, got different type", "E_TYPE"); break;
            }
            auto* arr = (ArrayObject*)arrVal.ref;
            int idx = frame.slots[instr.left].intValue;
            if (idx < 0 || idx >= (int)arr->elements.size()) {
                pendingThrow_ = makeErrorValue(
                    "array index out of bounds (index=" + std::to_string(idx) +
                    ", length=" + std::to_string(arr->elements.size()) + ")", "E_OOB");
                break;
            }
            arr->elements[idx] = frame.slots[instr.right];
            break;
        }
        case Opcode::ARRAY_LEN: {
            Value& arrVal = frame.slots[instr.src];
            if (arrVal.kind != ValueKind::Ref || !arrVal.ref)
                throw std::runtime_error("runtime error: not an array");
            auto* arr = (ArrayObject*)arrVal.ref;
            frame.slots[instr.dest] = Value::fromInt((int)arr->elements.size());
            break;
        }

        // ── Tip dönüşümleri (ADR-026: as operatörü) ─────────────────────
        case Opcode::CAST_INT_TO_STR: {
            frame.slots[instr.dest] = Value::fromString(
                std::to_string(frame.slots[instr.src].intValue));
            break;
        }
        case Opcode::CAST_FLOAT_TO_STR: {
            std::ostringstream oss;
            double fv = frame.slots[instr.src].floatValue;
            oss << fv;
            frame.slots[instr.dest] = Value::fromString(oss.str());
            break;
        }
        case Opcode::CAST_BOOL_TO_STR:
            frame.slots[instr.dest] = Value::fromString(
                frame.slots[instr.src].intValue ? "true" : "false");
            break;

        case Opcode::CAST_STR_TO_INT: {
            const std::string& s = frame.slots[instr.src].stringValue;
            try {
                size_t pos;
                long long v = std::stoll(s, &pos);
                if (pos != s.size()) throw std::invalid_argument("incomplete parse");
                if (v < INT_MIN || v > INT_MAX) throw std::out_of_range("overflow");
                frame.slots[instr.dest] = Value::fromInt((int)v);
            } catch (...) {
                if (instr.left == 1) frame.slots[instr.dest] = Value::null();
                else pendingThrow_ = makeErrorValue(
                    "'" + s + "' cannot convert to int", "E_CAST");
            }
            break;
        }
        case Opcode::CAST_STR_TO_FLOAT: {
            const std::string& s = frame.slots[instr.src].stringValue;
            try {
                size_t pos;
                double v = std::stod(s, &pos);
                if (pos != s.size()) throw std::invalid_argument("incomplete parse");
                frame.slots[instr.dest] = Value::fromFloat(v);
            } catch (...) {
                if (instr.left == 1) frame.slots[instr.dest] = Value::null();
                else pendingThrow_ = makeErrorValue(
                    "'" + s + "' cannot convert to float", "E_CAST");
            }
            break;
        }
        case Opcode::CAST_FLOAT_TO_INT_CHECKED: {
            double fv = frame.slots[instr.src].floatValue;
            if (!std::isfinite(fv) || fv < (double)INT_MIN || fv > (double)INT_MAX) {
                if (instr.left == 1) frame.slots[instr.dest] = Value::null();
                else pendingThrow_ = makeErrorValue(
                    "float value out of int range or NaN/Inf", "E_CAST");
            } else {
                frame.slots[instr.dest] = Value::fromInt((int)fv); // truncate to zero
            }
            break;
        }

        // ── String (ADR-024: immutable değer-tipi, içerik ==) ────────────
        case Opcode::STRING_CONCAT:
            frame.slots[instr.dest] = Value::fromString(
                frame.slots[instr.left].stringValue +
                frame.slots[instr.right].stringValue);
            break;

        // ── Hata yönetimi (ADR-025) ──────────────────────────────────────
        case Opcode::ENTER_TRY:
            tryStack_.push_back({callStack_.size(), instr.jumpTarget, instr.dest});
            break;

        case Opcode::LEAVE_TRY:
            if (!tryStack_.empty()) tryStack_.pop_back();
            break;

        case Opcode::THROW: {
            Value errVal = frame.slots[instr.src];
            // If user throws Error struct, fill trace field (fields[3])
            if (errVal.kind == ValueKind::Ref && errVal.ref &&
                errVal.ref->type == ObjectType::Struct) {
                auto* errObj = static_cast<StructObject*>(errVal.ref);
                if ((int)errObj->fields.size() >= 4)
                    errObj->fields[3] = Value::fromString(buildTrace());
            }
            pendingThrow_ = errVal;
            break;
        }

        // ── FFI ───────────────────────────────────────────────────────────
        case Opcode::CALLHOST:
            executeHostFunction(instr.functionName, frame.slots, instr.argSlots);
            break;
        }

        // ── pendingThrow_ işle: try varsa catch'e unwind, yoksa fırlat ───
        if (pendingThrow_.has_value()) {
            Value errVal = std::move(*pendingThrow_);
            pendingThrow_.reset();

            if (!tryStack_.empty()) {
                TryFrame tf = tryStack_.back();
                tryStack_.pop_back();
                // catch bloğunun bulunduğu frame'e unwind
                while (callStack_.size() > tf.callStackDepth)
                    callStack_.pop_back();
                // Error'ı catch değişkenine bağla ve catch etiketine atla
                callStack_.back().slots[tf.errorSlot] = errVal;
                callStack_.back().instructionPointer  = tf.catchTarget;
            } else {
                // Uncaught error — extract message and raise as C++ exception
                std::string msg = "uncaught error";
                if (errVal.kind == ValueKind::Ref && errVal.ref) {
                    auto* s = static_cast<StructObject*>(errVal.ref);
                    if ((int)s->fields.size() > 2 &&
                        s->fields[2].kind == ValueKind::String)
                        msg = s->fields[2].stringValue;
                } else if (errVal.kind == ValueKind::String) {
                    msg = errVal.stringValue;
                }
                throw std::runtime_error(msg);
            }
            continue;
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
            std::cout << val.toString() << "\n";
        }
        return;
    }
    throw std::runtime_error("runtime error: unknown host function '" + name + "'");
}
