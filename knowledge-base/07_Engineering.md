# Build, Verification, and Release

> Consolidated knowledge-base document. Each numbered section preserves
> the responsibility and content of one former focused Markdown file.
> Former filenames remain recorded for traceability; internal links point
> to their new section locations.

## Section Map

- [38 Build](#kb-38-build)
- [39 Testing](#kb-39-testing)
- [40 Benchmarks](#kb-40-benchmarks)
- [41 Performance](#kb-41-performance)
- [45 Versioning](#kb-45-versioning)
- [46 Release](#kb-46-release)

---

<a id="kb-38-build"></a>
## 38 Build

_Former file: `38_Build.md`._

### Status

**CMake build structure is implemented in source; configuration, compilation,
linking, portability, and produced artifacts are Not Verified.** No build
command was run during this review.

### Authoritative Compiler Build

Root `CMakeLists.txt` is the current build source:

- CMake minimum `3.16`;
- project `saqut` version `0.8.0`, languages C and C++;
- required C++20 for compiler sources;
- `-Wall -Wextra`;
- exact `Debug` uses `-g -O0`;
- every other/empty build type follows the Release branch with `-O3`,
  `-DNDEBUG`, frame pointers, and static runtime/link flags.

`file(GLOB_RECURSE "src/*.cpp")` feeds one `saqut` executable. This makes new
`.cpp` files automatic but can hide target-boundary mistakes and relies on
`CONFIGURE_DEPENDS` only for test fixture discovery, not compiler sources.

### MIR Dependency

Root CMake builds only vendored `mir.c` and `mir-gen.c` into static `mir_core`,
using GNU C11, signed char, PIC, and conditionally supported GCC-style
optimization-disabling flags. `saqut` links this library directly.

The vendored MIR repository has its own large `CMakeLists.txt`, but root CMake
does not add that project. Its many examples/tests are vendor facilities, not
saQut build targets. MIR's generated machine-code platform support and the
Release branch's full static linking remain platform-dependent and
**Not Verified**.

### Produced and Missing Targets

Source declares two principal targets: `mir_core` and `saqut`. Test cases are
CTest registrations, not separate maintained unit-test targets.

No install target, package target, AOT compiler target, emitted native
executable pipeline, shared library SDK, or public ABI artifact was found.
`saqut build/compile` language-product ideas must not be confused with building
the C++ compiler.

### Conflicting and Legacy Instructions

- Root CMake and `tests/run.sh` require C++20.
- `readme.md` claims C++17.
- `scripts/compile.sh` invokes C++17 and compiles only `src/main.cpp`, while the
  active project contains many `.cpp` translation units plus MIR C sources.

Treat the latter two as stale until reconciled. `build-debug.sh`,
`build-release.sh`, and `scripts/build.sh` wrap CMake/Ninja, but were not run.
Ninja is convenient in scripts/readme; root CMake does not require a specific
generator.

### Compiler vs Documentation Build

`saqutwebside/` is a nested Astro/Starlight repository with its own package and
deployment build. Root CMake does not build it. Compiler `build/` output and
website `dist/`/`.astro/`/generated agent files are artifacts, never semantic
sources.

See [07_Repository.md](00_Orientation.md#kb-07-repository), [21_MIR.md](04_IR_Backends.md#kb-21-mir),
[39_Testing.md](07_Engineering.md#kb-39-testing), and [55_Astro.md](08_PublicDocs.md#kb-55-astro).

---

<a id="kb-39-testing"></a>
## 39 Testing

_Former file: `39_Testing.md`._

### Status

**Tracked compiler, golden, differential, LSP, and DAP test infrastructure
exists. Test registration and fixture presence are Implemented; all pass/fail
results are Not Verified because no test or build was run.**

### Tracked Test Layers

#### Direct Unit Tests

`tests/test_type.cpp` and `tests/test_diagnostic.cpp` exercise header-level type
and diagnostic behavior. `tests/run.sh` compiles them directly with C++20 into
`/tmp`, then runs the existing `build/saqut` for broader suites. CMake registers
that whole script as `unit_tests`; the name is broader than its behavior.

#### Golden and Negative Tests

`tests/golden/` contains 103 tracked `.sqt` files, including dependency/helper
modules that are not necessarily standalone tests. CMake registers a fixture
only when a companion contract exists:

- `.expected`: VM stdout plus successful exit;
- `.ir_opt.expected`: optimized IR dump;
- `.run_opt.expected`: optimized execution output;
- `.compile_error`: nonzero exit plus expected stderr match;
- `.runtime_error`: the same negative runner contract;
- `.flags`: extra capability flags.

The shell runner's positive golden loop suppresses program exit status and
compares stdout only; the per-fixture CMake runner also requires exit zero.
Use CTest registrations as the stricter contract.

#### VM-MIR Differential

For each runnable golden fixture, CMake can run VM and `--jit`, comparing stdout
and exit code byte-for-byte. If MIR stderr contains the unsupported-opcode
marker, CTest marks the case skipped. Therefore:

- accepted MIR fixture mismatch is a parity failure;
- skipped fixtures prove no parity;
- stderr/diagnostics, external side effects, and internal state are not compared.

This is **Partially Implemented parity coverage**, not universal VM-JIT
equivalence.

#### Module, Semantic, and GC Checks

`tests/run.sh` also contains explicit module-cycle/self-import checks, VM GC
trigger/liveness modes, and builtin syntax/field-shadow checks. CMake's
`unit_tests` entry reaches these only after a compiler binary exists.

#### LSP and DAP JSONL

There are 21 tracked LSP and 10 tracked DAP request scenarios with expected
JSONL and Python drivers. CMake discovers them when `python3` is found.
`wip_` names can be marked `WILL_FAIL`; none should be interpreted as passing
without execution. See [35_LSP.md](06_Tooling.md#kb-35-lsp) and [36_DAP.md](06_Tooling.md#kb-36-dap).

### Non-Authoritative and Missing Coverage

`tests/general/` is untracked in the current worktree. Its recursion/nesting
stress sources and scripts are exploratory and must not be used as
authoritative test evidence.

Automatic extraction/testing of public documentation code fences is
**Planned** in [57_DocsSync.md](08_PublicDocs.md#kb-57-docssync) and
[62_DocsTesting.md](08_PublicDocs.md#kb-62-docstesting); it is not part of current compiler
CTest. Website build, link, locale parity, generated agent files, and live HTTP
smoke tests are a separate docs-system test layer.

No executed coverage, sanitizer, fuzz, race, portability, reproducible-build,
or resource-limit result was established in this review.

### Interpretation Rule

Never convert "fixture exists", "test is registered", an old TODO report, or a
historical green count into "currently passes." When changing semantics, add
the narrow regression, a VM golden/negative contract, MIR differential coverage
when supported, and tool/docs coverage when the surface is public.

See [01_Sources.md](00_Orientation.md#kb-01-sources), [27_Determinism.md](02_Language.md#kb-27-determinism), and
[38_Build.md](07_Engineering.md#kb-38-build).

---

<a id="kb-40-benchmarks"></a>
## 40 Benchmarks

_Former file: `40_Benchmarks.md`._

### Status

**Benchmark command and fixtures are Implemented; all results are Not
Verified.** No build, test, benchmark, or profiling command was run during this
review. Repository scripts and registrations describe a method, not a current
performance result.

### `saqut bench`

`src/cli/commands/bench.hpp` implements:

- `--runs=N`, clamped to at least one;
- `--compile-only`, which omits VM execution;
- per-phase samples for tokenize, parse, symbol collection, type checking plus
  structural validation, IR generation, and VM execution;
- integer average and best/minimum microseconds;
- a separate profile run collecting token/AST/symbol/IR counts, VM allocations,
  dispatch counts, and per-opcode timing trace.

Module discovery happens once, outside timing, using a bench-specific canonical
path traversal. It bypasses `ModuleLoader`, follows quoted file imports, and
does not use the normal loader's complete diagnostic/cycle behavior. File I/O
and dependency discovery are excluded from compile phase times.

The measured execution backend is the VM. `bench` does not consult `--jit` and
does not measure MIR compilation or execution. `run --profile` uses the separate
single-run `profiling::StageTimer`; it is not the N-run benchmark.

### Fixtures and Scripts

Tracked assets include:

- `scripts/bench/bench_fib.sqt` and `bench_sum.sqt`;
- Python and Java implementations intended as comparison rulers;
- `gen_compile_suite.py` and a tracked 400-module compile suite;
- `scripts/bench/run_bench.sh`, which builds Release, measures the suite and VM,
  runs language comparisons, and writes `docs/benchmark.md`;
- `examples/large.sqt` and stress-oriented examples.

The generator and report script are executable procedures, not evidence that
their generated report is current. `tests/general/` is untracked and is not an
authoritative benchmark or stress contract.

### Methodology Risks

- There is no discarded Saqut warmup run. Python performs one warmup and Java
  performs 1000, so cross-language conditions differ.
- VM timing includes `vm.run()` initialization and program output; constructor
  time is outside the timed interval.
- The reported “compile total best” sums each phase's independent best, which
  may combine different runs rather than one end-to-end sample.
- Runtime exceptions are caught and printed inside `runPipeline`, after which
  the run can still return success and contribute a timing.
- The profile run has trace overhead and is correctly reported separately; its
  timing must not be mixed with normal samples.
- `high_resolution_clock`, TSC calibration, machine load, CPU frequency, build
  flags, static linking, and platform affect comparability.
- No confidence interval, median/percentile, outlier policy, memory peak, or CI
  regression gate is implemented.

### Reporting Rule

Every published result must include commit, compiler/build mode, hardware/OS,
input identity and size, backend, run count, warmup policy, measured boundary,
and raw samples or retained report. Never copy numeric speed claims from an ADR,
comment, or old generated report into current status.

Correctness is a prerequisite: validate output/exit behavior and backend support
before comparing time. A skipped MIR fixture or unsupported whole-program gate
is not a VM-JIT performance comparison.

Evidence: `src/cli/commands/bench.hpp`, `src/bench/profile.hpp`,
`src/profiling/stage_timer.hpp`, and `scripts/bench/`. Static review:
2026-07-25.

See [39_Testing.md](07_Engineering.md#kb-39-testing) and [41_Performance.md](07_Engineering.md#kb-41-performance).

---

<a id="kb-41-performance"></a>
## 41 Performance

_Former file: `41_Performance.md`._

### Status

**Source-visible performance mechanisms exist; validated bottlenecks, budgets,
and current metrics are Not Verified.** No numeric speed, latency, throughput,
or memory claim is established by this static review.

### Priority and Goals

ADR-032/038 place correctness, deterministic observable behavior, and
inspectability ahead of aggressive optimization. MIR JIT is an accepted and
partially implemented route toward “acceptable normal speed”; it is not proof
that a performance target has been met. Vectorization, aggressive inlining, and
large backend-specific transforms are explicitly outside the stated design
direction.

No enforced compile-time, startup, runtime, memory, or pause-time budget was
found. Percentages and speed ratios written in ADR-032 describe rationale or
upstream MIR expectations, not measured Saqut results.

### Source-Visible Mechanisms

- Token dispatch uses a character switch and keyword hash lookup.
- Module, symbol, semantic, and IR phases operate as explicit passes.
- Active IR is typed, slot-based, and three-address-like; the VM uses a switch
  dispatch loop over instruction vectors.
- The AST optimizer implements local constant folding and dead-code-oriented
  passes before IR generation.
- VM GC checks an allocation threshold at instruction safepoints.
- MIR lowering maps supported slots to registers, selects MIR optimization
  level 2, and eagerly compiles accepted functions before entering `main`.
- Runtime registries use indexed tables/maps; vectors are reserved in several
  hot paths.
- `saqut bench` and `run --profile` expose different measurement layers.

These structures show engineering intent, not measured benefit.

### Candidate Costs, Not Proven Bottlenecks

Static inspection identifies areas worth measuring:

- full tokenize/parse/semantic work for each invocation and LSP update;
- repeated vector allocation for calls and runtime objects;
- VM switch dispatch and per-instruction GC threshold check;
- heavyweight `Value` representation and reference/object allocation;
- benchmark/profile trace memory growth;
- eager whole-program MIR generation and startup;
- JIT runtime string/decimal pools retained until program completion;
- absence of JIT-integrated GC and restricted MIR coverage.

Do not label any item a bottleneck without a reproducible profile.

### Optimization Rules

1. Establish a tracked correctness fixture and measurement boundary.
2. Record baseline and candidate results under identical build, input, backend,
   machine, and warmup conditions.
3. Preserve VM semantics, diagnostics, determinism, and MIR differential
   behavior; speed never justifies silent divergence.
4. Report compile, JIT warmup, execution, allocation, and GC costs separately.
5. Measure memory as well as elapsed time for changes that cache, intern, pool,
   or retain objects.
6. Keep backend-specific optimization below the shared typed IR contract.
7. Treat a benchmark script or profiler hook as instrumentation, not a result.

Evidence: active tokenizer/module/IR/VM/MIR/GC/optimizer sources,
`src/bench/`, `src/profiling/`, and ADR-032/038/041. Static review: 2026-07-25.

See [18_Optimizer.md](03_Frontend.md#kb-18-optimizer), [20_VM.md](04_IR_Backends.md#kb-20-vm),
[21_MIR.md](04_IR_Backends.md#kb-21-mir), [24_GC.md](05_Runtime.md#kb-24-gc), and
[40_Benchmarks.md](07_Engineering.md#kb-40-benchmarks).

---

<a id="kb-45-versioning"></a>
## 45 Versioning

_Former file: `45_Versioning.md`._

### Status

**Compiler product versioning is Implemented at a basic level; compatibility
governance is accepted but operational enforcement is Partially Implemented /
Not Verified.** No stable language, ABI, binary-format, or serialization-schema
version exists.

### Version Surfaces

| Surface | Source-visible value | Meaning |
|---|---|---|
| Compiler/CMake | `0.8.0` | Root project version and `SAQUT_VERSION` macro |
| CLI | `saqut --version` uses `SAQUT_VERSION` | Compiler product version |
| Language runtime | embedded `core.version()` returns `SAQUT_VERSION` | Compiler/runtime product version |
| LSP server info | hard-coded `0.1.0` | LSP server implementation identity, not language version |
| VS Code extension | `0.4.0`; tracked `saqut-0.4.0.vsix` | Editor extension package version |
| DAP | no separate Saqut server product version found | Unknown |

The current local Git branch is named `0.8.0`, and a local `0.8.0` tag exists.
Because branch and tag share a name, unqualified Git revision output can be
ambiguous. Their presence does not prove that a packaged/public release exists.

Public docs and the nested Astro site's package version are separate deployment
surfaces. They must not silently define the compiler or language version.

### ADR-038 Compatibility Policy

ADR-038 makes its strong compatibility contract binding from `1.0.0`:

- **patch:** bug fixes, with intended backport/yank hygiene;
- **minor:** deliberately frozen observable surface, unlike ordinary SemVer;
  no new syntax, function, FFI/builtin, or CLI output surface within a major;
- **major:** new observable product contract, without cross-major guarantee.

The repository declares `0.8.0`. ADR-038 explicitly says 0.x APIs are unstable,
while intending bugfix/backport/yank hygiene already. This is an accepted policy,
not evidence that backports, package yanks, or two-way minor compatibility are
operationally enforced. No package manager or release automation implements
those mechanisms.

### Missing Independent Versions

No authoritative value was found for:

- language specification/grammar version;
- typed IR or opcode version;
- binary/persistence format version;
- public native/host ABI version;
- AST/symbol/diagnostic JSON schema version;
- stdlib API version.

Compiler `0.8.0` must not be reused implicitly for all these contracts. Add an
independent version only when a persisted or external compatibility boundary is
defined.

### Change Rules

1. Classify the changed surface: compiler, language, CLI/tool JSON, LSP/DAP,
   editor, stdlib, docs, ABI, or binary format.
2. Update the owning version source; do not duplicate compiler version literals.
3. For observable compiler behavior, check ADR-038 and the 0.x/1.x distinction.
4. Coordinate public docs and examples without treating them as semantic
   authority.
5. A tag is the final identifier, not the mechanism that proves tests,
   packaging, signatures, or deployment succeeded.

Evidence: root `CMakeLists.txt`, CLI args, embedded FFI catalog/host table,
LSP initialize handler and tracked expected JSONL, VS Code package metadata,
local Git refs, and ADR-038. Static review: 2026-07-25.

See [27_Determinism.md](02_Language.md#kb-27-determinism), [30_ABI.md](04_IR_Backends.md#kb-30-abi),
[46_Release.md](07_Engineering.md#kb-46-release), and [61_DocsVersions.md](08_PublicDocs.md#kb-61-docsversions).

---

<a id="kb-46-release"></a>
## 46 Release

_Former file: `46_Release.md`._

### Status

**No complete, repeatable Saqut product release pipeline was found.** Local
release-oriented build/package pieces exist, but compiler publication,
multi-platform packaging, documentation synchronization, and post-release
verification are **Partially Implemented or Planned**.

No build, test, benchmark, package, tag, network, or deployment command was run
during this review.

### Existing Pieces

- `build-release.sh` configures and builds root CMake in Release mode. It does
  not package or publish anything.
- Root CMake defines compiler version `0.8.0` and attempts an optimized,
  statically linked compiler executable outside exact Debug builds.
- A local annotated `0.8.0` Git tag exists on the same-named branch.
- `editor/vscode/package.json` provides an extension package script:
  compile with esbuild, then `vsce package --no-dependencies`.
- A tracked `editor/vscode/saqut-0.4.0.vsix` exists.
- `scripts/bench/run_bench.sh` can generate a benchmark report, but benchmark
  generation is not release automation and was not run.

Artifact presence does not prove provenance, reproducibility, test results,
signature, compatibility, publication, or installability.

### Missing Operational Process

No root `.github/workflows` release automation, changelog, release checklist,
install/package target, compiler archive packaging, checksum/signature step,
supported-platform matrix, GitHub Release publisher, rollback/yank tool, or
post-publish smoke record was found.

The compiler, LSP/DAP executable modes, VS Code extension, public docs, and
nested Astro deployment have separate version/build surfaces. No source-visible
release orchestrator keeps them synchronized. Website deployment configuration
describes intended delivery only; live behavior remains separate and was not
observed.

### Required Future Release Gate

The following is a **proposed minimum process**, not a claim about current
practice:

1. classify the release under [45_Versioning.md](07_Engineering.md#kb-45-versioning) and update
   each affected product version;
2. freeze source and record commit/tag identity without branch/tag ambiguity;
3. run the defined compiler build/test/platform matrix and record results;
4. run differential, LSP/DAP, docs, and security checks required by changed
   surfaces;
5. run benchmarks only for a declared performance claim, with retained method
   and raw evidence;
6. build clean artifacts, record toolchain, checksums, licenses, and provenance;
7. update EN/TR public docs, examples, compatibility notes, and release notes
   from compiler-owned truth;
8. publish compiler/editor/docs artifacts through their separate owners;
9. perform install and live delivery smoke checks, then record rollback/yank
   paths.

Failed release validation must not be hidden by an existing tag or generated
artifact. ADR-038's backport/yank policy cannot be operational until distribution
and package ownership are defined.

Evidence: root CMake, `build-release.sh`, build scripts, editor package metadata
and VSIX, local Git refs, release-like tracked-file inventory, ADR-038, and docs
system boundaries. Static review: 2026-07-25.

See [38_Build.md](07_Engineering.md#kb-38-build), [39_Testing.md](07_Engineering.md#kb-39-testing),
[40_Benchmarks.md](07_Engineering.md#kb-40-benchmarks), [57_DocsSync.md](08_PublicDocs.md#kb-57-docssync), and
[58_WebDelivery.md](09_WebPlatform.md#kb-58-webdelivery).
