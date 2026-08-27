# saQut — Context Handoff for Advisory Review

**Role of the reader (you):** You are an independent compiler/GC consultant. Your job is
ADVISORY and READ-ONLY: scan the repository and the work described below, and give a verdict —
are we progressing correctly? Flag architectural mistakes, correctness hazards, and better
alternatives with evidence (file:line). Do NOT modify code. Do not treat this document's claims
as proof; verify against the source. Repo discipline is defined in `AGENTS.md` (read it first) —
in particular: evidence over assertions, VM is the normative backend, no unverified claims.

---

## 1. Project snapshot

- **saQut**: a compiled language with its own toolchain. Pipeline: ModuleLoader → SymbolCollector
  → TypeChecker → IRGenerator → flat IR → **VM (normative)** or **MIR JIT (experimental)**.
- Current branch: `0.9.6`. Recent commit chain discussed below. Push is intentionally withheld.
- Product-owner direction (decided 2026-08-27): **0.9.6–0.9.8 theme = threading + GC, with
  infrastructure-first priority**. Real multithreading/fibers land **after 1.0**. GitHub issues
  are frozen for this phase ("close the project, ship 0.9.6"); tracking happens in
  `docs/gc-threading-altyapi-denetimi.md` (audit doc, Turkish) and commit messages.
- Note: `src/parser/token.hpp` in the working tree belongs to ANOTHER agent working in parallel
  (an operator-precedence fix for `^`). It is intentionally uncommitted by us — do not touch it.
- Test safety net: `bash tests/run.sh` — golden (124), differential VM≡JIT (105 pass / 0 fail /
  1 skip), GC gates incl. root-narrowing counter assertion (`freed >= 2046`).

## 2. Locked architectural decisions (product-owner approved)

1. **Flat IR is the canonical storage/execution contract.** CFG is the canonical representation
   *during optimization* (built per-function, passes run on blocks, `linearize()` writes back).
   Backends consume flat IR only. Rationale: differential parity stays cheap; VM dispatch,
   breakpoints, DAP stepping are index-based. Passes must be written against block structure,
   not instruction indices — keeps a future SSA migration non-breaking. (This matches the
   HotSpot model: bytecode executes flat, CFG/SSA derived internally.)
2. **Existing AST-level optimizations stay** (constant folding, DCE). The CFG/liveness work is
   GC *infrastructure*, not an optimizer package. First IR-level pass will be slot-DCE, later.
   A full optimizer suite is explicitly out of 0.9.x scope (AGENTS.md §9).
3. **`nogc` is dynamically scoped** (decided from the call stack, not the signature): suppression
   counter incremented on nogc-frame entry, decremented on exit; GC paused while > 0; deferred
   sweep runs when the outermost nogc frame closes. Suppression deliberately leaks into callees.
   `gc_collect()` will be a callhost builtin (like `print`): full mark+sweep immediately,
   independent of suppression depth. A `free`/`delete` keyword was considered and **REJECTED** —
   principle: *promise on timing, never on reachability*. (NOT YET IMPLEMENTED — design only.)
4. **Unified string model (implemented):** strings are GC'd `StringObject` on the VM heap in BOTH
   backends. `Value` no longer embeds `std::string`.
5. **Value representation (implemented):** `kind` + a single union (i64 / double /
   DecimalValue / Object*). 112 → 24 bytes, trivially copyable. Writing fields is possible only
   via factory functions (`fromInt`/`fromString`/...) — verified that no external field writers
   exist. `static_assert(sizeof(Value) <= 24)` guards it.
6. **Phase 5 allocator = Option A (owner's final answer):** non-moving bump + free-list.
   A moving/compacting (generational, semi-space) GC is **Option B**, deferred: it would require
   revising ADR-022 ("non-moving"), an FFI pinning/safepoint contract, and `jitData` view
   invalidation. Option A preserves determinism guarantees with zero new risk surface.

## 3. Completed work (phase by phase, with evidence commits)

| Phase | Work | Commit | Key facts |
|---|---|---|---|
| 0 | CFG hardening | `8f9616b` | ENTER_TRY→catch **exception edges** modeled (were missing entirely — catch blocks looked unreachable); leader split after JMP; `removeUnreachableBlocks()`; dominance (Cooper-Harvey-Kennedy, deterministic RPO); natural loops. `linearize()` rewrites ENTER_TRY jump targets across block removal. |
| 1 | Slot liveness + root narrowing | `4390056` (VM), `fbd7bb8` (JIT) | `src/ir/ir_liveness.*`: block gen/kill fixpoint + per-instruction backward walk. Functions containing ENTER_TRY are conservatively `exact=false` (all slots live) until try-region analysis exists — wrong "dead" marks would delete live objects. VM `maybeCollect` roots only slots live at the current IP (dead ref/string slots nulled at collection to avoid dangling pointers for DAP). JIT: `emitShadowSet` skips emission for slots dead after the defining instruction (compile-time decision, zero runtime cost). Counter proof: `narrow_proof.sqt` freed 2036→2046 (+10). |
| 2 | Value narrowing + unified string | `4ec9c2e`, `2862123` | See decisions 4–5. Allocation hook `allocValueString` (object.cpp, thread_local heap pointer bound by `Interpreter::run`/`initForDebug` and the JIT entry). Proof: string churn (3000 concats) → GC runs=2 freed=2046 (strings now collectable for the first time). |
| 3 | Object header cleanup | `b7610e3` | `marked` bool removed (markState is the single liveness source); intrusive list is **doubly linked** (`prev`) with `Heap::unlink` O(1) removal; virtual `markChildren`/dtor removed → type-switch (`markObjectChildren`/`deleteObject`), no vptr per object. **Bug found & fixed:** markChildren previously rooted only `Ref` children — struct/array fields holding *strings* could be swept → dangling pointer (emerged from the string model change). Regression fixture `tests/golden/gc/struct_string.sqt`. |
| 4 | Global state closure | `94882ee`, `3e52a6c` | All ~17 scattered `g_jit*` globals consolidated into `struct JitRuntime` + single accessor `rt()` (process-lifetime today; becomes `thread_local` in one line when threads arrive). `ShadowStack` storage made `thread_local`. `sys_random`/`sys_randomInt` RNG made thread_local. `FileRegistry` boundary note already adequate (compile-time single-threaded). `globalSlots_` ownership intentionally open — tied to product decision K2 (cross-thread object sharing). |

Audit document: `docs/gc-threading-altyapi-denetimi.md` — the original findings (A1–A6),
decisions, and phase status. Read it for the "why" behind everything above.

## 4. Phase 5 — Option A design (approved; to be implemented next)

**Goal:** the Heap owns its memory; `new`/`delete` (global malloc) stop being the allocation path
for object shells. Non-moving (ADR-022 untouched).

Design sketch (v8/JVM-inspired hybrid, adapted to our non-moving constraint):

1. **Segments:** Heap allocates large blocks ("segments") from the OS, geometric growth
   (e.g. 2.4 MB → 4.8 MB → ..., owner-specified sizes). Large blocks are effectively mmap anyway;
   we take explicit control.
2. **Bump allocation:** fresh object shells are carved from the current segment by advancing a
   pointer (the fastest known allocation: one pointer increment, no locks, no size-class search).
3. **Free-list reuse:** on sweep, dead objects' memory goes to a per-size-class free list (or a
   best-fit list); new allocations satisfy from the free list first, bump second. Deterministic
   order (single-threaded, fixed traversal) — counters stay reproducible.
4. **Whole-segment reclamation:** per-segment live counter; a segment that becomes fully empty is
   returned to the OS in one piece.
5. **What stays on malloc for now:** `std::vector` payloads inside ArrayObject/StructObject
   (elements buffers, string data). Routing STL allocations through the arena is a separate,
   later step (pairs with the pending "7 parallel vectors → union" cleanup, audit A4.2).
6. **GC logic unchanged:** mark/sweep/unlink keep their contracts; only the
   allocate/deallocate substrate changes. Differential + golden suites are the acceptance gate.
7. **Explicit non-goals:** no object movement, no `jitData` changes (views keep pointing at stable
   addresses — that is why Option A is safe), no generational logic.

Known follow-ups queued after Phase 5: LOAD_STRING interning cache (currently one allocation per
execution), UTF-8 processing optimizations (length caching; owner requires all string processing
to be UTF-8-aware), ArrayObject 7-vectors → union, frame pooling, try-region liveness analysis
(to de-conservatize `exact=false`).

## 5. What we want you (the consultant) to check

Priority order:

1. **Liveness correctness (highest risk):** `src/ir/ir_liveness.cpp` def/use derivation vs
   `OPCODE_LIST` semantics in `src/ir/instruction.hpp` (esp. FIELD_SET/ARRAY_SET where `dest` is
   a USE; ENTER_TRY error slot; CALLHOST argSlots). Is the ENTER_TRY conservatism sufficient, or
   are there paths where a "live" slot is wrongly considered dead (→ use-after-free class)? Also
   review `Interpreter::maybeCollect` narrowing + dead-slot nulling in `src/vm/interpreter.cpp`.
2. **Unified string model soundness:** all creation paths go through `allocValueString` /
   `Heap::allocString`; all rooting paths mark String kind (`markValue`, `markObjectChildren`);
   any raw `std::string` ownership assumptions left anywhere (DAP, FFI, transpile, data/*.cpp)?
   `Value` union: any read of an inactive union member (kind-gated reads only)?
3. **CFG/dominance implementation:** `src/ir/ir_cfg.cpp` — CHK algorithm, RPO determinism,
   self-loop handling in `computeLoops`, `linearize()` jump-target rewriting after
   `removeUnreachableBlocks()`.
4. **Phase A design review:** is non-moving bump + free-list coherent as described? Failure
   modes we should design for before writing code (fragmentation of size-classes, segment
   threshold tuning, interaction with incremental marking #217)?
5. **Threading-readiness claims:** is `JitRuntime& rt()` + thread_local shadow stack + RNG
   actually sufficient isolation for future per-thread heaps, or did we miss shared state?
   (Known open: `globalSlots_`, MIR context per compilation, host ABI statics.)

## 6. Ground rules for your review

- Cite evidence as `file:line` from the current tree. Distinguish *verified fact* / *suspicion* /
  *recommendation*.
- VM behavior is normative; anything that would change observable output (stdout/exit/stderr)
   is a red line. GC timing may change; collected set may not (ADR-038).
- If you believe Option B (moving GC) is genuinely necessary earlier than we assume, argue it
  with the cost of the FFI pinning/safepoint contract and ADR-022 revision — not just "V8 does it".
- End with: **verdict** (proceed / proceed with fixes / stop and rethink) + prioritized fix list.
