# Orientation and Project Map

> Consolidated knowledge-base document. Each numbered section preserves
> the responsibility and content of one former focused Markdown file.
> Former filenames remain recorded for traceability; internal links point
> to their new section locations.

## Ten-File Map

| File | Responsibility |
|---|---|
| `00_Orientation.md` | Evidence order, status, architecture map, repository boundaries, and AI-agent rules |
| `01_Governance.md` | Principles, invariants, ADR governance, coding/documentation policy, contribution, and roadmap |
| `02_Language.md` | Language surface, syntax, type system, and determinism contract |
| `03_Frontend.md` | Pipeline, lexer, parser, AST, symbols, semantics, modules, and optimizer |
| `04_IR_Backends.md` | Active IR, reference VM, MIR JIT, ABI boundaries, and future binary format |
| `05_Runtime.md` | Runtime, memory, GC, errors, diagnostics, serialization, FFI, builtins, stdlib, security, and concurrency |
| `06_Tooling.md` | CLI, LSP, DAP, formatter status, and package-management status |
| `07_Engineering.md` | Build, testing, benchmarks, performance, versioning, and release |
| `08_PublicDocs.md` | Public docs platform, Astro/content ownership, synchronization, versions, and docs testing |
| `09_WebPlatform.md` | Delivery configuration, agent interfaces, SEO, and future web tools |

Start with this file. For compiler implementation work, continue with the
focused language/frontend/backend/runtime file. For public documentation work,
read `08_PublicDocs.md` and `09_WebPlatform.md` after the governance rules.

## Sections in This File

- [00 Knowledge Base Index](#kb-00-index)
- [01 Sources](#kb-01-sources)
- [02 Project](#kb-02-project)
- [04 Status](#kb-04-status)
- [05 Architecture](#kb-05-architecture)
- [07 Repository](#kb-07-repository)
- [51 AI Agent Instructions](#kb-51-ai-agent)

---

<a id="kb-00-index"></a>
## 00 Knowledge Base Index

_Former file: `00_Index.md`._

### Purpose

This directory is the compact technical memory for AI agents working on saQut.
It maps repository evidence; it does not replace source, tracked tests, ADRs, or
a canonical language specification.

Last core static review: 2026-07-24. No build or test was run for that review.

### First Read

Read these before making broad assumptions:

1. [01_Sources.md](00_Orientation.md#kb-01-sources): evidence authority and status labels.
2. [04_Status.md](00_Orientation.md#kb-04-status): implemented, partial, planned, and unverified
   snapshot.
3. [05_Architecture.md](00_Orientation.md#kb-05-architecture): active compiler flow and system
   boundaries.
4. [06_Invariants.md](01_Governance.md#kb-06-invariants): contracts a change must preserve.
5. [07_Repository.md](00_Orientation.md#kb-07-repository): ownership and file map.
6. [51_AI_Agent.md](00_Orientation.md#kb-51-ai-agent): agent workflow and prohibited shortcuts.

For public docs work also read [49_DocPolicy.md](01_Governance.md#kb-49-docpolicy),
[54_PublicDocs.md](08_PublicDocs.md#kb-54-publicdocs), [57_DocsSync.md](08_PublicDocs.md#kb-57-docssync), and the
nested repository's `saqutwebside/AGENTS.md`.

### Status Labels

- **Implemented:** active source structure exists.
- **Partially Implemented:** integrated code exists with explicit gaps.
- **Planned:** ADR target, TODO, roadmap item, or stub only.
- **Not Verified:** requires execution or deployment observation.
- **Unknown:** current evidence does not establish an answer.

Compiler behavior authority:

`tests > compiler source > ADR > language spec > public docs > old AI/TODO notes`

Only tracked tests count. Public docs are not authoritative for compiler
semantics.

### Topic Map

| Area | Consolidated sections |
|---|---|
| Project and architecture | `00_Orientation.md#kb-02-project`, `01_Governance.md#kb-03-principles`, `00_Orientation.md#kb-04-status`, `00_Orientation.md#kb-05-architecture`, `01_Governance.md#kb-06-invariants`, `03_Frontend.md#kb-08-pipeline` |
| Language frontend | `02_Language.md#kb-09-language` through `03_Frontend.md#kb-17-modules` |
| Optimization and execution | `03_Frontend.md#kb-18-optimizer` through `05_Runtime.md#kb-24-gc` |
| Errors and determinism | `05_Runtime.md#kb-25-errors` through `02_Language.md#kb-27-determinism` |
| Formats and native boundaries | `05_Runtime.md#kb-28-serialization` through `04_IR_Backends.md#kb-30-abi` |
| Libraries and security | `05_Runtime.md#kb-31-builtins` through `05_Runtime.md#kb-33-security` |
| User/tool interfaces | `06_Tooling.md#kb-34-cli` through `06_Tooling.md#kb-37-formatter` |
| Engineering and lifecycle | `07_Engineering.md#kb-38-build` through `01_Governance.md#kb-50-contributing` |
| Agent memory and roadmap | `00_Orientation.md#kb-51-ai-agent`, `01_Governance.md#kb-52-roadmap`, `01_Governance.md#kb-53-glossary` |
| Public docs platform | `08_PublicDocs.md#kb-54-publicdocs` through `09_WebPlatform.md#kb-63-webtools` |

### Current Critical Boundaries

- Active pipeline: tokenizer/parser -> module graph -> symbols -> semantics ->
  optional AST optimization -> typed slot IR -> VM or MIR JIT.
- Two code-backed execution backends exist: default/reference VM and partial
  whole-program MIR JIT. AOT is planned.
- Active IR is not `src/ir/ir.hpp`; that file is a stale unused prototype.
- VM GC and JIT boxed-value lifetime are different systems.
- Builtins, curated FFI, and planned self-hosted stdlib are distinct layers.
- CLI registration does not imply implementation.
- `saqutwebside/` is an independent nested repository and a public
  documentation system, not a compiler frontend folder.
- Intended web delivery configuration is not observed deployed behavior.

### Maintenance Rule

Keep each consolidated section focused. Update the owning technical section when source changes,
then adjust this index only when reading order, section responsibility, or a
critical cross-system boundary changes. Do not paste large source inventories
into multiple files.

---

<a id="kb-01-sources"></a>
## 01 Sources

_Former file: `01_Sources.md`._

### Purpose

Define which repository evidence an agent may use, how conflicts are resolved,
and how implementation claims are labelled.

### Compiler Behavior Authority

Use this order exactly:

`tests > compiler source > ADR > language spec > public docs > old AI/TODO notes`

- **Tests:** Only tracked tests and their expected outputs count. `tests/general/`
  is currently untracked and is not authoritative. A test that was not executed
  is evidence of an intended contract, not proof that the current binary passes.
- **Compiler source:** Prefer active call paths over comments, help text, unused
  prototypes, or generated output. For example, the active IR is
  `IRGenerator`/`IRProgram`/`Instruction`; `src/ir/ir.hpp` is an unused legacy
  prototype whose comments are stale.
- **ADR:** An ADR records a decision. Preserve its own `Accepted`,
  `Implemented`, or `Planned` state. Acceptance alone does not prove code exists.
- **Language spec:** No complete canonical specification was found in the
  inspected repository. Existing design documents must not be silently promoted
  to a finished specification.
- **Public docs:** User-facing, derived descriptions. They do not define compiler
  semantics.
- **Old AI/TODO notes:** Discovery hints only. Re-verify every claim against
  stronger evidence.

Within the same level, prefer the active code path and newer evidence that
explicitly supersedes older material. Do not resolve a conflict by silently
merging incompatible claims.

### Evidence Classes

- **Implemented:** A concrete structure and active integration path are visible
  in source.
- **Partially Implemented:** Source exists, but the declared surface or backend
  coverage is explicitly incomplete.
- **Planned:** Present only as an accepted target, TODO, roadmap item, stub, or
  proposed contract.
- **Not Verified:** Static source suggests behavior, but execution, deployment,
  platform behavior, or generated output was not checked.
- **Unknown:** The inspected sources do not establish an answer.

Always distinguish implementation structure from runtime verification. In this
review, no build or test was run, so behavioral success remains `Not Verified`
even where implementation code is present.

### Source, Artifact, and Deployment

- Compiler sources live primarily in `src/`; tracked tests live in `tests/`.
- Compiler build directories, binaries, caches, and generated reports are
  artifacts, not sources.
- Public docs sources live under
  `saqutwebside/src/content/docs/` plus Astro/configuration source.
- `saqutwebside/dist/`, `.astro/`, `node_modules/`, and generated
  `public/llms.txt`/`public/llms-full.txt` are not authoritative inputs.
- `nginx.conf`, `public/_headers`, and Astro middleware describe intended
  delivery. They do not prove live HTTP behavior. Live behavior is a separate
  observation and is `Not Verified` here.

### Primary Files Consulted

Compiler truth should normally be traced from `CMakeLists.txt`, `src/main.cpp`,
`src/cli/commands/`, the active pipeline subsystems under `src/`, tracked tests,
and `docs/adr/`. Public docs truth should be traced from the nested repository's
`AGENTS.md`, `astro.config.mjs`, `src/content/docs/`, `src/middleware.ts`,
`scripts/build-assets.py`, `public/_headers`, and `nginx.conf`.

Last static review: 2026-07-24. See [04_Status.md](00_Orientation.md#kb-04-status) for the resulting
snapshot and [49_DocPolicy.md](01_Governance.md#kb-49-docpolicy) for maintenance rules.

---

<a id="kb-02-project"></a>
## 02 Project

_Former file: `02_Project.md`._

### Amacı
saQut'un ürün kimliğini, hedef kitlesini, kapsamını ve başarı ölçütünü tanımlamak.

### İçereceği başlıklar
- Projenin kısa tanımı
- Çözdüğü temel problem
- Hedef kullanıcılar ve kullanım biçimleri
- Compiler ürünü ile public documentation system ilişkisi
- Kapsam içi ve kapsam dışı alanlar

### Tahmini uzunluk
500-800 kelime.

### Bağımlı olduğu diğer markdown dosyaları
`01_Governance.md#kb-03-principles`, `00_Orientation.md#kb-05-architecture`, `01_Governance.md#kb-52-roadmap`, `08_PublicDocs.md#kb-54-publicdocs`.

### Bu dosyanın neden gerekli olduğu
Yeni özelliklerin projenin “cam kutu derleyici” kimliğiyle uyumunu değerlendirmek için sabit bağlam sağlar.

---

<a id="kb-04-status"></a>
## 04 Status

_Former file: `04_Status.md`._

### Reading This Snapshot

- **Implemented:** active source structure exists.
- **Partially Implemented:** source exists with explicit gaps or restricted
  coverage.
- **Planned:** decision, design, TODO, or stub only.
- **Not Verified:** requires build, execution, test, platform, or deployment
  observation.

This is a static snapshot dated 2026-07-24. No build or test was run.

### Compiler Core

| Area | Status | Static evidence and boundary |
|---|---|---|
| Lexer/tokenizer | Implemented | Character scanner and token production are separate source layers |
| Parser/AST | Implemented | Pratt expressions, recursive-descent declarations/statements, recovery nodes, polymorphic AST |
| Module system | Implemented | Recursive file loading, graph ownership, overlays, imports, cycle detection |
| Symbols/scopes | Implemented | Multi-pass name/layout/body collection and module-aware bindings |
| Semantic analysis | Implemented | TypeChecker and StructuralValidator are integrated into `run` |
| Type system | Implemented | Primitive/array/struct/enum/function/error types and nullability exist in code |
| Diagnostics | Partially Implemented | Structured engine and renderers exist; some parser callers still omit the engine and catalog/comments can lag emissions |
| Optimizer | Implemented | Constant folding and dead-code elimination with bounded fixpoint |
| Active IR | Implemented | Typed slot IR, functions, globals, module metadata, source/debug data |
| Legacy `src/ir/ir.hpp` | Obsolete/unused | No active consumer found; do not use it as current architecture |

All runtime correctness claims above remain `Not Verified` in this review.

### Backends and Runtime

| Area | Status | Static evidence and boundary |
|---|---|---|
| VM backend | Implemented | Default IR interpreter with frames, globals, errors, host/builtin calls, and debugger controls |
| VM memory management | Implemented | Heap objects and non-moving mark-sweep code exist; roots include globals, frames, and pending throw |
| MIR JIT backend | Partially Implemented | Whole-program gate and lowering exist for a subset including several scalar/string/decimal operations |
| JIT GC integration | Planned/Partial | JIT boxed values use host-side program-lifetime arenas; VM heap/shadow-stack GC integration is not present |
| Embedded-runtime AOT | Planned | ADR-032 accepted direction; no working backend/CLI implementation found |
| WASM/LLVM | Planned/Unknown | Architecture horizon only; no implementation counted |
| Thread/concurrency model | Planned | Current compiler/VM code is principally single-threaded; no shipped language concurrency model established |

**Backend count:** two code-backed execution backends, VM and MIR JIT. AOT is
not counted because it is planned.

### CLI and Tooling

- **Implemented command handlers:** `run`, `tokens`, `ast`, `symbols`, `check`,
  `ir`, `exec`, `lsp`, `dap`, and `bench`.
- **Stub/TODO commands:** `compile`, `parse`, `transpile`, and `interpret`.
- **Stub/TODO input mode:** stdin (`saqut -`).
- Flags can exist before all described behavior is current. For example, stale
  `--jit` comments still mention fallback, while active handlers reject
  unsupported JIT programs without fallback.
- **LSP:** Partially Implemented as a real server with sync, diagnostics,
  definition, hover, references, symbols, highlights, completion, rename, and
  signature help handlers. Protocol behavior is `Not Verified`.
- **DAP:** Partially Implemented as a VM-backed adapter with launch,
  breakpoints, continue/step/pause, stack/scopes/variables/evaluate, termination,
  and disconnect. Global expression lookup is explicitly TODO; several advanced
  DAP capabilities are declared unsupported. Behavior is `Not Verified`.

### Libraries and External Boundaries

- **Builtin methods:** Implemented registry plus semantic/IR/VM/LSP integration
  for array, string, and struct-value method categories.
- **Curated FFI:** Implemented source seam for `core`, `math`, `caps`, `fs`,
  `sys`, and `date`; it is not arbitrary native-library loading.
- **Capabilities:** Implemented in source for FFI binding/instruction dispatch;
  enforcement behavior is `Not Verified`.
- **Self-hosted stdlib:** Planned. ADR-041 is accepted/locked architecture, not
  evidence that `std/*.sqt` exists.
- **Package manager, public ABI, binary format, formatter:** no implemented core
  surface was established in this review; treat as `Planned` or `Unknown` until
  their dedicated files are researched.

### Public Documentation System

- **Astro/Starlight bilingual source tree:** Implemented.
- **Sidebar, sitemap integration, content sources, build asset generator:**
  Implemented in source; build result `Not Verified`.
- **Generated Markdown and `llms.txt`/`llms-full.txt`:** generation code exists;
  checked-in/generated artifacts are not authority.
- **Accept negotiation, Link headers, `.well-known` endpoints:** intended
  configurations exist across middleware, `_headers`, static files, and nginx.
  Observed deployed behavior is `Not Verified`.
- **Automatic docs/compiler example synchronization:** Planned. No general
  extraction-and-test gate was established from the inspected sources.
- **Versioned documentation and playground/web tools:** Planned or `Unknown`;
  no shipped implementation established.

### Known High-Risk Ambiguities

1. Accepted ADRs can describe target architecture that current code has not
   reached.
2. MIR JIT comments and public docs can understate or overstate its changing
   opcode coverage.
3. “Standard library” currently mixes public product terminology, builtin
   methods, and host FFI; self-hosted stdlib is still a target.
4. VM GC does not imply equivalent JIT GC.
5. Registered CLI names do not imply implemented handlers.
6. Intended web headers do not prove live CDN/nginx behavior.

---

<a id="kb-05-architecture"></a>
## 05 Architecture

_Former file: `05_Architecture.md`._

### System Context

The workspace contains two related systems:

- **Compiler system:** parses, analyzes, lowers, inspects, and executes saQut
  programs.
- **Public documentation system:** the nested Astro/Starlight repository that
  publishes human and machine-readable descriptions derived from compiler truth.

They share subject matter, not authority or repository ownership. Public docs
must follow the compiler; they must not redefine it.

### Active Compiler Pipeline

The `run` command exposes the clearest integrated path:

```text
source files
  -> ModuleLoader
  -> Tokenizer (using Lexer)
  -> Parser
  -> ModuleGraph of ASTs and tokens
  -> SymbolCollector pass 1a: names
  -> SymbolCollector pass 1b: layouts/import validation
  -> SymbolCollector pass 2: bodies and bindings
  -> TypeChecker + StructuralValidator
  -> optional AST optimization
  -> IRGenerator
  -> IRProgram
  -> VM by default, or whole-program MIR JIT with --jit
```

Diagnostic gates stop the integrated pipeline after module loading, symbol
collection, and semantic analysis when errors exist. This is an implementation
structure found in `src/cli/commands/run.hpp`; successful behavior was not
executed in this review.

### Frontend Boundaries

- `Lexer` is the character-level reader and source-position utility.
- `Tokenizer` owns a `Lexer` and emits token objects.
- `Parser` consumes tokens. It combines Pratt parsing for expressions with
  recursive descent for declarations/statements and can emit `ErrorNode` during
  panic-mode recovery when a `DiagnosticEngine` is supplied.
- The AST is a polymorphic node hierarchy with `ASTKind` dispatch, parent/child
  links, semantic annotations such as resolved expression type, and mostly
  manual pointer ownership.
- `ModuleLoader` parses the entry file and imports into a move-only
  `ModuleGraph`. It also provides a source-overlay seam used by LSP.
- `SymbolCollector` establishes names, layouts, scopes, imports, and bindings in
  multiple passes before type checking.
- `TypeChecker` annotates/checks expression types; `StructuralValidator` checks
  contextual rules such as `break`, `continue`, `return`, and nested
  declarations.

### Optimization and IR

Optimization currently operates on AST, after semantic analysis and before IR
generation. `OptimizationManager` registers constant folding and dead-code
elimination and repeats passes to a bounded fixpoint. Single-use commands may
optimize in place; the AST inspection path can clone to preserve before/after
views.

The active IR is a typed, slot-based instruction program:
`IRProgram -> IRFunction -> Instruction`. It carries function order, module IDs,
global slots, source locations, slot names, and slot types. Both VM and MIR JIT
consume this shared IR.

`src/ir/ir.hpp` defines an older virtual-register `CodeGenerator` prototype. No
active include/use was found outside that file. Do not infer current IR limits
from its stale comments.

### Execution Backends

There are **two code-backed execution backends**:

1. **VM:** default and reference backend. It interprets IR with call frames,
   globals, heap values, exception state, host calls, builtin dispatch, and DAP
   execution controls. A VM mark-sweep collector exists in source.
2. **MIR JIT:** optional `--jit` backend. It performs a whole-program support
   check, then lowers supported IR through vendored MIR. Source includes scalar
   integer, long integer, float, float32, string, decimal, control-flow, call,
   return, and restricted host-print paths. Reference/aggregate operations,
   nullable fallible cast targets, general host calls, and other opcodes can
   reject the entire program. There is no silent VM fallback in the active
   `run`/`exec` paths.

Embedded-runtime AOT is an accepted architectural direction in ADR-032, but no
implemented compile-to-binary backend or real `compile` command was found.
WASM/LLVM are future design space, not current backends.

### Runtime, Builtins, FFI, and Stdlib

- Core VM operations and aggregate storage live in `src/vm/`.
- Builtin method signatures live in `BuiltinMethodRegistry`; semantic analysis,
  IR generation, VM dispatch, and LSP consume the registry.
- The curated FFI seam is declared as embedded saQut text in
  `src/ffi/root_sqt.hpp`, parsed once into `FfiCatalog`, lowered to `CALLHOST`,
  and implemented by the host-function table.
- Capabilities are attached to FFI symbols/instructions and checked in symbol
  collection and VM host dispatch.
- A self-hosted `std/*.sqt` layer is described by ADR-041 and
  `docs/architecture.md`, but no such implemented source tree was found.
  Current `fs`, `sys`, `math`, and `date` surfaces are host FFI, not proof of a
  completed self-hosted standard library.

### Tools

CLI, LSP, and DAP reuse compiler data structures rather than forming alternate
language implementations. LSP has document overlays and analysis-backed
handlers. DAP controls the VM, not the MIR JIT. Protocol/runtime behavior is
`Not Verified` in this static review.

See [04_Status.md](00_Orientation.md#kb-04-status), [06_Invariants.md](01_Governance.md#kb-06-invariants), and
[54_PublicDocs.md](08_PublicDocs.md#kb-54-publicdocs).

---

<a id="kb-07-repository"></a>
## 07 Repository

_Former file: `07_Repository.md`._

### Repository Boundary

The workspace contains two Git boundaries:

1. The compiler repository at the workspace root.
2. `saqutwebside/`, ignored by the root `.gitignore` and containing its own
   `.git` directory.

Treat `saqutwebside/` as a nested, independently owned public documentation
system. Root-repository Git status does not describe its changes. Never clean,
reset, stage, or rewrite one repository while intending to operate on the other.

### Root Map

| Path | Responsibility | Notes |
|---|---|---|
| `src/` | Compiler, tools, and runtime source | Primary implementation evidence |
| `tests/` | Tracked unit, golden, negative, differential, LSP, and DAP fixtures | `tests/general/` is currently untracked and excluded from authority |
| `docs/` | Architecture notes and ADRs | Decision evidence; status varies by ADR |
| `examples/` | Example saQut programs | Useful evidence, but below tests/source |
| `cmake/` | CMake test drivers and support | Infrastructure source |
| `scripts/` | Bench and project utility scripts | Read purpose before use |
| `editor/` | Editor integration, including VS Code files | Separate from LSP implementation |
| `wiki/` | Additional prose | Lower authority unless linked to a current decision |
| `knowledge-base/` | Agent-oriented technical map | Derived, compact, and maintained |
| `saqutwebside/` | Nested Astro/Starlight docs repository | Not a compiler source directory |

Build trees, caches, binaries, screenshots, bundles, and generated web output are
not architectural sources.

### Compiler Source Map

- `src/lexer/`: character cursor/scanner utilities and literal reading. It does
  not itself own the final token stream.
- `src/tokenizer/`: converts source text into tokens using `Lexer`.
- `src/parser/`: Pratt expression parsing, recursive-descent declarations and
  statements, AST node types, and parser recovery.
- `src/module/`: file/module loading, canonical paths, module graph ownership,
  overlays for tools, and cycle detection.
- `src/symbol/`: scopes, symbols, module bindings, and multi-pass collection.
- `src/semantic/`: type checking and structural validation.
- `src/opt/`: AST cloning and optimization passes. Current manager registers
  constant folding and dead-code elimination.
- `src/ir/`: active slot-based IR structures and AST-to-IR generation. The
  standalone `src/ir/ir.hpp` is a legacy, unused prototype.
- `src/vm/`: reference interpreter, values, heap objects, host/builtin dispatch,
  GC, and debugger execution controls.
- `src/mir/`: optional MIR JIT backend and vendored MIR source.
- `src/ffi/`: embedded FFI declarations, catalog, and C++ host functions.
- `src/builtin/`: shared builtin method metadata consumed by semantic, IR, VM,
  and LSP paths.
- `src/diagnostic/`: structured diagnostics and renderers.
- `src/lsp/`, `src/dap/`: protocol servers and handlers.
- `src/cli/`: argument parsing, command dispatch, and command handlers.
- `src/core/`: shared types, locations, configuration, capabilities, and module
  registry.
- `src/profiling/`, `src/bench/`: measurement support.
- `src/vendor/`: vendored dependencies. Modify only with explicit vendor intent.

### Code Organization and Style

The build configuration requires C++20. The codebase is header-heavy: several
CLI handlers, registries, and small facilities are inline. Larger subsystems use
paired headers and `.cpp` files. Common patterns are enum-based dispatch,
explicit phase objects, plain structs, and central data records.

Ownership is mixed. Module graphs and some managers use RAII/move ownership,
while tokens and AST nodes still use raw pointers with explicit deletion and
tree-owned children. Do not introduce a local ownership refactor without tracing
all destructors and callers. Comments are extensive but can be stale; active
calls outrank comments.

### Public Docs Source Map

- `saqutwebside/src/content/docs/`: English source pages and Turkish pages under
  `tr/`.
- `saqutwebside/astro.config.mjs`: site URL, locales, sidebar, and integrations.
- `saqutwebside/src/middleware.ts`: intended response headers at the Astro layer.
- `saqutwebside/scripts/build-assets.py`: generation of agent-facing artifacts
  and Markdown copies.
- `saqutwebside/public/`: static assets and machine endpoints. Generated
  `llms*.txt` files are outputs.
- `saqutwebside/nginx.conf` and `public/_headers`: alternative intended delivery
  configurations. Their live application is `Not Verified`.
- `saqutwebside/AGENTS.md`: local editing and public writing rules; it is not a
  compiler semantics source.

See [54_PublicDocs.md](08_PublicDocs.md#kb-54-publicdocs) for the bounded context.

---

<a id="kb-51-ai-agent"></a>
## 51 AI Agent Instructions

_Former file: `51_AI_Agent.md`._

### Required First Read

For compiler work, read in order:

1. [00_Index.md](00_Orientation.md#kb-00-index)
2. [01_Sources.md](00_Orientation.md#kb-01-sources)
3. [04_Status.md](00_Orientation.md#kb-04-status)
4. [05_Architecture.md](00_Orientation.md#kb-05-architecture)
5. [06_Invariants.md](01_Governance.md#kb-06-invariants)
6. [07_Repository.md](00_Orientation.md#kb-07-repository)
7. The focused subsystem file and relevant active code/tests/ADRs

For public docs work, additionally read [49_DocPolicy.md](01_Governance.md#kb-49-docpolicy),
[54_PublicDocs.md](08_PublicDocs.md#kb-54-publicdocs), [57_DocsSync.md](08_PublicDocs.md#kb-57-docssync), and
`saqutwebside/AGENTS.md` before editing.

### Evidence Discipline

Use this behavior order exactly:

`tests > compiler source > ADR > language spec > public docs > old AI/TODO notes`

- Count only tracked tests. Exclude `tests/general/` while it is untracked.
- Do not treat an unexecuted test as proof that behavior currently passes.
- Follow active call paths. Comments, CLI help, unused prototypes, and old plans
  can be stale.
- Preserve `Accepted`, `Implemented`, and `Planned` ADR states.
- Public docs are never authoritative for compiler semantics.
- Generated outputs, binaries, `dist/`, `.astro/`, and generated `llms*.txt`
  are not sources.
- Use `Implemented`, `Partially Implemented`, `Planned`, `Not Verified`, or
  `Unknown`; do not fill gaps with inference.

### Mandatory Architectural Checks

Before changing compiler code, identify:

- the phase that owns the behavior;
- token, AST, symbol, type, IR, VM, JIT, LSP, DAP, and diagnostic consumers;
- tracked tests that state the current contract;
- relevant ADR status and whether the code has reached its target;
- ownership/lifetime impact of raw AST/token pointers;
- whether the change affects public docs or examples.

Do not create a parallel semantics path for CLI, LSP, or DAP. Reuse the same
module, symbol, semantic, and IR contracts where the codebase already does.

### High-Risk Project Facts

- `Lexer` is character-level; `Tokenizer` emits tokens.
- The active IR is `IRProgram`/`IRFunction`/`Instruction`.
  `src/ir/ir.hpp` is an unused legacy prototype.
- The VM is the default/reference backend.
- MIR JIT is partial and whole-program gated. Unsupported input fails; there is
  no silent fallback in active `run`/`exec`.
- There are two code-backed execution backends. Embedded-runtime AOT is planned,
  not implemented.
- VM mark-sweep GC does not prove JIT GC integration.
- `compile`, `parse`, `transpile`, `interpret`, and stdin mode are TODO/stub
  surfaces despite being recognized by CLI parsing/help.
- LSP and DAP have real handlers, but are partial; DAP is VM-backed.
- FFI is curated host binding, not arbitrary native library loading.
- ADR-041 defines a target self-hosted stdlib architecture; current host modules
  do not mean that migration is complete.

### Repository Safety

- The root and `saqutwebside/` are separate Git repositories.
- Inspect status in the repository being changed.
- Preserve user/uncommitted changes and do not clean generated or untracked files
  without explicit authorization.
- Never edit vendored source as a shortcut for compiler behavior.
- For website work, modify source rather than `dist/`, `.astro/`, or generated
  `public/llms*.txt`.
- Intended nginx/Astro/Cloudflare configuration and observed deployment are
  separate. Do not claim live behavior without an explicit observation.

### Change Completion

For implementation work, completion normally requires focused tests scaled to
risk, backend parity review, tool/diagnostic updates, and docs impact review.
If execution is prohibited or unavailable, state that clearly and leave behavior
`Not Verified`.

For public docs, maintain EN/TR contract parity, navigation/source mappings, code
fence classification, and generated-artifact ownership. SEO and agent discovery
are separate concerns.

### Prohibited Shortcuts

- Do not implement a planned ADR merely because architecture prose describes it
  in present tense.
- Do not use public examples as a substitute for tracked tests.
- Do not infer backend support from an opcode's existence; inspect the backend's
  support gate and lowering.
- Do not infer a command works because it is registered.
- Do not silently update compiler semantics to match stale docs.
- Do not present static inspection as runtime verification.
