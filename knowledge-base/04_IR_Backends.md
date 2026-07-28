# IR and Execution Backends

> Consolidated knowledge-base document. Each numbered section preserves
> the responsibility and content of one former focused Markdown file.
> Former filenames remain recorded for traceability; internal links point
> to their new section locations.

## Section Map

- [19 Intermediate Representation](#kb-19-ir)
- [20 Virtual Machine](#kb-20-vm)
- [21 MIR JIT Backend](#kb-21-mir)
- [30 ABI](#kb-30-abi)
- [44 Binary Format](#kb-44-binary)

---

<a id="kb-19-ir"></a>
## 19 Intermediate Representation

_Former file: `19_IR.md`._

### Status and Active Files

**Implemented active slot IR; type metadata is Partially Implemented.**

The active backend contract is:

- `src/ir/instruction.hpp`
- `src/ir/ir_function.*`
- `src/ir/ir_program.*`
- `src/ir/ir_generator.*`

`src/ir/ir.hpp` is an older virtual-register/`CodeGenerator` prototype. No
active include or use of that model was found. Its comments and limitations
must not be used to describe current VM/JIT behavior.

### Program Model

`IRProgram` contains:

- functions keyed by name plus an explicit `functionOrder`;
- module registry information;
- program-wide global-slot metadata and per-module global counts;
- a declared `globalSlotTypes` map.

`IRFunction` contains:

- name, module ID, parameter count, and total slot count;
- ordered `Instruction` values;
- debug slot names and per-slot `SlotType`;
- a line-to-first-instruction map used by debugging.

Function order is significant for deterministic traversal/JIT setup and should
not be reconstructed from unordered-map iteration.

### Instruction Model

Instructions use one `Opcode` plus a generic operand record. Depending on the
opcode, fields hold destination/source/left/right/condition slots, jump target,
numeric/string/decimal constants, function name, argument slots, aggregate
field names, capability metadata, and source file/line/column.

This is a three-address-like slot IR, not a stack bytecode:

- values are produced into numbered slots;
- calls name the callee and list argument slots;
- returns name a source slot;
- branches use direct instruction indices and are backpatched by the generator;
- try/throw behavior has explicit instructions;
- globals and aggregate operations have dedicated opcodes.

### Typed Slot Model

`SlotType` currently distinguishes `Int`, `LongInt`, `Float` (double),
`Float32`, `Ref`, `Str`, `Decimal`, `Date`, and `Unknown`.

`IRGenerator::finalizeSlotTypes` fills function `slotTypes` after lowering:
parameters come from declared types; producing opcodes and `LOAD_SLOT` propagate
types through a bounded fixpoint; call results use a function-return map.

This metadata is primarily required by MIR register selection and its support
gate. The VM executes dynamically tagged `Value` objects.

Do not overstate the typing:

- `Instruction::valueType` is declared but no active assignment was found;
- `IRProgram::globalSlotTypes` is declared but no active population was found;
- field, array, and global loads can remain at the default slot type because
  result type is not encoded on those instructions;
- unknown/unsupported source types can collapse to `Int` in slot mapping.

Therefore “typed slot IR” means function slots have a generated coarse type
table, not that every IR value and global is fully and soundly typed.

### Generation

`IRGenerator` lowers a complete `ModuleGraph` into one program. It:

- creates module functions and preserves module IDs;
- allocates parameter/local/temporary slots monotonically per function;
- maintains scoped source-name-to-slot mappings for shadowing;
- flattens program globals;
- lowers control flow with labels/backpatching;
- attaches source metadata for diagnostics/debugging;
- finalizes slot metadata after emitting a function.

Both VM and MIR consume this exact active IR. MIR does not consume AST directly.

### Optimizer Relationship

The implemented optimization layer runs on AST after semantics and before IR
generation. No active IR optimization pass was identified. MIR's native
generator may optimize lowered native code, but that is a backend action, not
the compiler's AST optimizer or a shared IR pass.

### Change Contract

A new opcode requires synchronized updates to instruction naming/dumping,
IRGenerator emission and slot inference, VM dispatch, MIR support
gate/lowering or explicit rejection, source/debug metadata, diagnostics, and
tracked tests. Never let MIR's default switch silently imply support.

This document is based on static source inspection; emitted IR and backend
behavior are **Not Verified**.

See [16_Types.md](02_Language.md#kb-16-types), [20_VM.md](04_IR_Backends.md#kb-20-vm), and
[21_MIR.md](04_IR_Backends.md#kb-21-mir).

---

<a id="kb-20-vm"></a>
## 20 Virtual Machine

_Former file: `20_VM.md`._

### Status and Role

**Implemented default/reference backend; runtime behavior Not Verified in this
review.**

`src/vm/interpreter.*` consumes the active `IRProgram`. It is the normal backend
when `--jit` is absent and the semantic reference for comparing optional
backends. “Reference” means intended observable behavior, not proof that every
path is bug-free.

### Execution Model

The VM is a dispatch-loop interpreter:

- each iteration reads the current frame's instruction and advances its
  instruction pointer;
- a switch implements the opcode;
- jumps replace the instruction pointer with an IR instruction index;
- calls push a frame and returns pop it;
- pause/breakpoint/budget/error states can return control to DAP/CLI callers.

It is not an operand-stack VM. Values live in numbered function slots.
`callStack_` is a stack of frames, not a stack of arithmetic operands.

### Frames, Calls, and Globals

A `CallFrame` holds:

- a non-owning `IRFunction*`;
- instruction pointer;
- a `std::vector<Value>` sized to the function's slot count;
- the caller destination slot for a returned value.

`CALL` looks up the named IR function, copies argument-slot values into the
callee's leading parameter slots, and pushes a new frame. `RETURN` copies the
source value to the caller destination and finishes execution when the last
frame returns.

Globals use one flat program-wide `globalSlots_` vector. Module IDs remain
metadata; global access is by the flattened index produced by IR generation.

### Runtime Values and Objects

`Value` is dynamically tagged with kinds for `Int`, `LongInt`, `Float`,
`Float32`, `Decimal`, `String`, `Ref`, `Null`, and `Date`. Boolean values use
the integer kind (`0` false, nonzero true).

In the VM:

- strings and decimals are stored inline in `Value`;
- arrays and structs are `Object*` references allocated by `Heap`;
- array/struct equality compares reference identity;
- string equality compares content.

This representation differs internally from MIR's boxed string/decimal
trampolines. Backend parity concerns observable results, not identical storage.

### GC Integration

The VM heap implements non-moving mark-sweep collection for reference objects.
Allocation is tracked in an intrusive object list. `Interpreter::maybeCollect`
runs at instruction boundaries when a threshold is reached and marks roots
from:

- flat global slots;
- every live call frame's slots;
- a pending thrown value.

Objects recursively mark aggregate children, then sweep removes unmarked
objects. This is source-visible **Implemented** machinery, but threshold,
root-completeness, and collection behavior are **Not Verified** here.

VM GC must not be described as MIR JIT GC; the JIT currently uses separate host
arenas for supported boxed scalar-like values.

### Builtins, FFI, and Errors

`CALLHOST` has distinct source paths:

- builtin method dispatch through the builtin registry/runtime ID;
- curated FFI dispatch through a numeric host function ID and `HostContext`;
- the legacy/general host path including `print`.

Capabilities are attached during symbol/IR work and checked again at the host
boundary. The FFI is curated; it is not arbitrary dynamic-library loading.

Dynamic failures set/propagate the VM's pending error state for cases such as
division by zero, bounds violations, failed casts, explicit `throw`, and host
errors. `ENTER_TRY`, `LEAVE_TRY`, and `THROW` support runtime exception flow.
Uncaught behavior is a runtime contract and remains **Not Verified**.

### Debugging

The interpreter exposes breakpoints, executable-line queries, stepping,
instruction budgets, frame/slot inspection, and an output sink. DAP uses this
VM surface. No MIR JIT debugger integration should be inferred.

### Reference Backend Rule

Language semantics should first be represented in shared frontend/IR contracts
and the VM reference path. A JIT change must either match the VM for eligible
programs or reject those programs clearly. VM/JIT parity requires executed,
tracked differential tests; none were run for this review.

See [19_IR.md](04_IR_Backends.md#kb-19-ir), [21_MIR.md](04_IR_Backends.md#kb-21-mir),
[23_Memory.md](05_Runtime.md#kb-23-memory), and [24_GC.md](05_Runtime.md#kb-24-gc).

---

<a id="kb-21-mir"></a>
## 21 MIR JIT Backend

_Former file: `21_MIR.md`._

### Status

**Partially Implemented optional backend.** It compiles and executes an
explicitly limited whole-program subset. No JIT program or parity test was run
in this review.

The active interface is `src/mir/mir_backend.*`; vendored MIR is under
`src/mir/vendor/`.

### Input and Output

`tryCompileAndRunProgram(IRProgram&, outExitCode, outReason, profiler)` consumes
the same active `IRProgram` as the VM. It does not lower from AST.

On success it:

1. creates MIR functions and prototypes;
2. lowers supported IR instructions to MIR;
3. generates native code for every function in `functionOrder`;
4. invokes compiled `main`;
5. returns the native result as an exit code.

The backend uses `IRFunction::slotTypes` to choose MIR register types and call
signatures.

### Whole-Program Support Gate

Before compiling anything, `wholeProgramSupported` scans every function in
`functionOrder`, including unreachable functions. It rejects:

- any opcode outside `opcodeSupported`;
- unsupported slot kinds;
- void returns;
- host calls other than one-argument `print`;
- string ordering comparisons;
- nullable-target forms of supported fallible casts;
- a program without `main`.

Supported slot kinds in the active gate are `Int`, `LongInt`, `Float`,
`Float32`, `Str`, and `Decimal`. `Ref`, `Date`, and `Unknown` are rejected.

This is a support gate, not a proof of semantic parity. Its accuracy depends on
the coarse `slotTypes` table, whose known limitations are documented in
[19_IR.md](04_IR_Backends.md#kb-19-ir).

### Implemented Subset

Source contains gate and lowering paths for:

- integer, longint, float64, and float32 loads/arithmetic/bitwise operations;
- comparisons and conditional/unconditional control flow;
- direct saQut function calls and non-void returns;
- boxed string load, concatenation, equality, printing, and scalar conversions;
- boxed decimal arithmetic, conversions, and printing;
- selected checked/fallible scalar casts in non-nullable-result form;
- one-argument `print` host calls.

This backend is still **Partially Implemented** because active IR also contains
unsupported aggregates/references, globals, date operations, null/reference
semantics, general builtin/FFI calls, and try/throw paths.

Header comments that describe an older “int-only” slice are stale relative to
the active source switch. The source gate and lowering must be read together.

### Fallback Policy

There is **no silent VM fallback** in active `run` and `exec` JIT paths.
Unsupported IR returns `false` with a function/opcode reason, and the CLI emits
an error. Partial per-function or per-block JIT is not implemented.

Do not change this to fallback behavior incidentally. Such a policy change
affects observability, performance expectations, error behavior, and backend
testing and requires an explicit design decision.

### Runtime Trampolines and Memory

C trampolines implement print, string/decimal operations, checked casts, and
fatal runtime errors for the supported subset.

JIT strings and decimals are boxed as host-side objects and kept in process
vectors for the duration of the JIT program, then cleared after compiled
execution. They are not integrated into the VM `Heap`, do not use a shadow
stack, and are not traced by the VM mark-sweep collector. Repeated allocations
can therefore grow until program completion.

Reference/aggregate support and JIT GC integration are **Planned / not
implemented in active support**. Source comments describing future “slices” are
not implementation evidence.

### VM Parity

The intended contract is observable equivalence with the VM for programs that
pass the support gate: return value, stdout formatting, arithmetic/cast results,
and runtime failure behavior. Static source contains parity-oriented helpers,
ADR references, and differential test configuration.

Parity is **Not Verified** for this review because no tests were run. Configured
tests, matching comments, or similarly named trampolines are not proof.
Aggregate/null/exception/general-host parity is outside the currently accepted
JIT subset, not an implicit promise of support.

### Change Checklist

For each newly supported opcode/type, update and review together:

`opcodeSupported`, slot-kind gate, MIR signatures/register types, lowering,
runtime trampolines, lifetime/GC roots, failure reporting, VM semantics, and
tracked differential tests. A new gate case without matching lowering is a
correctness bug; lowering without a gate case is unreachable.

See [19_IR.md](04_IR_Backends.md#kb-19-ir), [20_VM.md](04_IR_Backends.md#kb-20-vm), [24_GC.md](05_Runtime.md#kb-24-gc), and
[27_Determinism.md](02_Language.md#kb-27-determinism).

---

<a id="kb-30-abi"></a>
## 30 ABI

_Former file: `30_ABI.md`._

### Status

**No stable public native ABI exists.** Internal VM, IR, host-call, and MIR JIT
calling representations are implemented to different degrees; ABI stability is
**Not Verified**.

### Four Distinct Contracts

#### VM Value Representation

`src/vm/value.hpp` is an internal C++ representation, not a published ABI.
`Value` contains a `ValueKind` plus separate integer, 64-bit integer, double,
decimal, `std::string`, and `Object*` fields. Bool and byte use integer storage;
date uses the 64-bit field; arrays/structs use heap references. Its size/layout
depends on C++ implementation details and may change.

Typed IR slots use `SlotType` and `IRFunction::slotTypes`. This is compiler
metadata used by MIR lowering, not an external binary interface. Slot and
global indices are internal program layout.

#### Curated Host FFI Contract

Embedded `root.sqt` declarations map symbolic host IDs to indices in the C++
`HostFn` table. Runtime dispatch passes `std::vector<Value>` and
`HostContext&` to a C++ function pointer. Numeric IDs derive from table order.
This is a curated compiler/host API, not arbitrary native symbol loading and
not a stable C ABI. Changing declaration, registry order, type mapping, or
capability metadata requires a coordinated audit.

#### MIR JIT Internal Calling Convention

MIR lowering maps scalar slots to MIR registers and pointer-shaped string,
decimal, and reference values to integer-width registers. Source contains
`extern "C"` runtime trampolines for printing, casts, strings, decimals, and
runtime errors, and invokes compiled `main` as `int64_t (*)(void)`.

These functions are private implementation seams. Their C linkage prevents
C++ name mangling but does not make them a supported public ABI. MIR support is
whole-program gated, host-call support is restricted, and aggregate/GC
integration remains incomplete.

#### Future Native/AOT ABI

ADR-032 accepts embedded-runtime AOT as a direction, but no active AOT backend,
object format, linker contract, exported symbol policy, or stable native
calling convention was found: **Planned**.

### ADR-037 Boundary

ADR-037 proposes a two-layer JIT value model and a C-compatible `MirValue` POD
for boundaries. Current source has boxed `StringObject`/`DecimalObject`,
typed MIR registers, and private trampolines, but no `MirValue`,
`mir_value_abi.hpp`, `toMir`, or `fromMir` implementation was found. Therefore:

- register-scalar/pointer lowering: **Partially Implemented**;
- the specified general JIT/VM/host boundary object: **Planned**;
- string/decimal behavior parity: **Not Verified**.

An accepted ADR records the decision; it does not upgrade missing code to
Implemented.

### Change Rule

Never call `Value`, `SlotType`, a host table index, or a MIR trampoline "the
saQut ABI" without its qualifier. A type/call change must audit:

`Type -> SlotType -> IR opcode/metadata -> VM Value -> host registry -> MIR gate/lowering/trampoline`

Public ABI/version promises require a separate accepted format, conformance
tests, target/platform matrix, and compatibility policy.

See [16_Types.md](02_Language.md#kb-16-types), [19_IR.md](04_IR_Backends.md#kb-19-ir),
[29_FFI.md](05_Runtime.md#kb-29-ffi), and [44_Binary.md](04_IR_Backends.md#kb-44-binary).

---

<a id="kb-44-binary"></a>
## 44 Binary Format

_Former file: `44_Binary.md`._

### Status

**No public Saqut bytecode, executable, object, or package binary format is
implemented.** Embedded-runtime AOT is an accepted direction in ADR-032 but
remains **Planned**. No artifact was generated or inspected in this review.

### Active Representation

`IRProgram`, `IRFunction`, and `Instruction` are in-memory C++ structures.
Functions live in a map with an explicit order vector; instructions and slot
metadata live in vectors. The VM directly interprets these typed slot
instructions. MIR consumes the same in-memory program and generates machine code
inside the current process.

The historical phrase “bytecode VM” describes the interpreter role, not a
serialized byte stream. There is no encoder/decoder, magic header, opcode-width
layout, section table, checksum, or loader. `IRProgram::dump()` is colored
human-readable debug output and has no parser.

`src/ir/ir.hpp` is an unused legacy IR prototype, not a format definition.
Vendored MIR contains upstream binary utilities, but no active Saqut path uses
them as a language artifact. The CMake-built `saqut` compiler executable is a
normal host build product, not a compiled `.sqt` binary format.

### JIT and AOT Boundary

Current MIR JIT code is process-local:

- accepted IR is lowered into an in-memory MIR context;
- supported functions are generated eagerly;
- native `main` is called through a private function-pointer convention;
- the context and temporary runtime pools are destroyed after execution.

No machine code, MIR module, relocation, or cache artifact is saved.

ADR-032 plans `saqut build` as a linkerless embedded-runtime executable: copy a
runtime and append compiled IR/bytecode for startup JIT. The repository currently
has no active `build` command, runtime-appender, reader, trailer locator, or AOT
compatibility check. ADR-041 deliberately selects embedded source for the future
self-hosted stdlib and rejects pre-serialized IR today because IR serialization
does not exist.

### Undefined Format Contracts

The following are **Unknown / Not Designed in active source**:

- magic/version fields and forward/backward compatibility;
- byte order, integer widths, alignment, and floating/decimal encoding;
- string table and source/debug information encoding;
- IR/opcode compatibility and feature negotiation;
- integrity, size limits, malformed-input validation, and signature policy;
- target architecture/runtime matching and deterministic reproduction.

`Value` layout, slot indices, host function numeric IDs, and MIR trampolines are
internal representations. They must not be written verbatim and called a stable
ABI or format.

### Change Rule

Before persistence or AOT work, accept a dedicated format ADR and independent
format version. Specify canonical encoding, validation before allocation or
execution, compatibility/migration policy, reproducibility, fuzz/negative
tests, and separation from compiler/language/ABI versions.

Evidence: `src/ir/`, `src/vm/`, `src/mir/mir_backend.cpp`, root CMake,
ADR-032/041, and CLI registration. Static review: 2026-07-25.

See [19_IR.md](04_IR_Backends.md#kb-19-ir), [28_Serialization.md](05_Runtime.md#kb-28-serialization),
[30_ABI.md](04_IR_Backends.md#kb-30-abi), and [45_Versioning.md](07_Engineering.md#kb-45-versioning).
