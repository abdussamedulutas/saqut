# Principles and Governance

> Consolidated knowledge-base document. Each numbered section preserves
> the responsibility and content of one former focused Markdown file.
> Former filenames remain recorded for traceability; internal links point
> to their new section locations.

## Section Map

- [03 Principles](#kb-03-principles)
- [06 Invariants](#kb-06-invariants)
- [47 Architecture Decision Records](#kb-47-adr)
- [48 Coding](#kb-48-coding)
- [49 Documentation Policy](#kb-49-docpolicy)
- [50 Contributing](#kb-50-contributing)
- [52 Roadmap](#kb-52-roadmap)
- [53 Glossary](#kb-53-glossary)

---

<a id="kb-03-principles"></a>
## 03 Principles

_Former file: `03_Principles.md`._

### Amacı
Tek tek tasarım kararlarının arkasındaki kalıcı mühendislik ilkelerini toplamak.

### İçereceği başlıklar
- Cam kutu ve incelenebilirlik
- Dar IR beli ve backend bağımsızlığı
- Determinizm ve açık davranış
- Basitlik, dikey dilim ve dış kapsam sınırları
- Public belgelerde açıklık, doğruluk ve pazarlama dili sınırı

### Tahmini uzunluk
500-800 kelime.

### Bağımlı olduğu diğer markdown dosyaları
`00_Orientation.md#kb-02-project`, `00_Orientation.md#kb-05-architecture`, `01_Governance.md#kb-06-invariants`, `01_Governance.md#kb-47-adr`, `01_Governance.md#kb-49-docpolicy`, `08_PublicDocs.md#kb-54-publicdocs`.

### Bu dosyanın neden gerekli olduğu
ADR ayrıntılarını ezberlemeden yeni kararların proje felsefesine uyup uymadığını sınamayı sağlar.

---

<a id="kb-06-invariants"></a>
## 06 Invariants

_Former file: `06_Invariants.md`._

### Authority Invariants

1. Compiler behavior is resolved in this order:
   `tests > compiler source > ADR > language spec > public docs > old AI/TODO notes`.
2. Only tracked tests count as authoritative tests. `tests/general/` is excluded
   while untracked.
3. Public docs never define compiler semantics.
4. Generated artifacts never override their source inputs.
5. `Accepted`, `Implemented`, and `Planned` ADR states are not interchangeable.

### Pipeline Invariants

- Token production belongs to `Tokenizer`; `Lexer` remains the character-level
  scanning utility.
- Parsing creates the AST. Symbol collection establishes bindings/layouts before
  type checking relies on them.
- The integrated pipeline must not lower to IR or execute while earlier
  diagnostic gates contain errors.
- Structural validation and type checking are distinct semantic responsibilities.
- Optimization occurs after semantic analysis and before active IR generation.
- An optimization may change representation, not observable program semantics.
- Active execution IR is `IRProgram`/`IRFunction`/`Instruction`.
  `src/ir/ir.hpp` must not silently become the model for current backend work.
- Module identity/path handling and module graph ownership must remain explicit;
  LSP overlays must not create a second compiler semantics path.

These are implementation-aligned rules, but their runtime enforcement is `Not
Verified` in the current static review.

### Ownership and Data Invariants

- The `ModuleGraph` owns the parsed units and token collections transferred into
  it.
- AST/token code still contains raw pointers and manual deletion. A refactor must
  trace creator, parent/child ownership, custom destructors, clone behavior, and
  every CLI/tool cleanup path.
- `IRProgram` is complete before VM/JIT consumers retain pointers into its
  function map.
- Slot types, module IDs, globals, source locations, and debug names are
  cross-layer contracts. Adding an opcode or type requires updating every
  relevant producer, dumper, VM handler, JIT support gate/lowering path, and
  debugger/tool consumer.
- Do not claim every AST node has a valid source location; source comments mark
  coverage as incomplete. Preserve valid locations and label missing coverage.

### Backend Invariants

- The VM is the default/reference execution backend.
- MIR JIT support is whole-program. Unsupported IR must fail clearly; active
  `run` and `exec` paths must not silently fall back to VM.
- Backend parity concerns observable behavior, not identical internal
  representation.
- New backend-visible semantics belong in shared IR/contracts first, unless an
  ADR explicitly establishes a backend-specific operation.
- VM/JIT equivalence requires tracked differential tests. A configured but
  unexecuted test is not proof of equivalence.
- VM mark-sweep collection must trace all live VM roots before sweep.
- VM GC must not be generalized to JIT: JIT boxed strings/decimals currently
  have a different lifetime model and no verified shared GC integration.

### Builtin, FFI, and Runtime Invariants

- Builtin signatures have one registry; semantic, IR, VM, and LSP behavior must
  not grow independent signature tables.
- FFI is curated through embedded declarations, catalog binding, `CALLHOST`, and
  the host table. It is not arbitrary C/C++ dynamic loading.
- Capability requirements travel with the bound symbol/instruction and must be
  enforced at the host boundary.
- Pure self-hosted stdlib described by ADR-041 is a target layer. Until source
  exists, do not report host-implemented functions as migrated.
- DAP controls VM execution. JIT debugging must not be inferred from the
  existence of DAP.

### Public Docs Invariants

- `saqutwebside/` remains an independent nested repository.
- Source pages, generated Markdown, delivery configuration, and observed live
  behavior are four different evidence classes.
- Every documented compiler feature must be traceable to stronger compiler
  evidence and carry an honest status.
- English/Turkish pages should describe the same compiler contract, while prose
  need not be literal translation.
- Executable examples must eventually be classified and extractable as defined
  in [57_DocsSync.md](08_PublicDocs.md#kb-57-docssync).
- SEO metadata and agent discovery/machine interfaces remain separate concerns.

See [01_Sources.md](00_Orientation.md#kb-01-sources) and [54_PublicDocs.md](08_PublicDocs.md#kb-54-publicdocs).

---

<a id="kb-47-adr"></a>
## 47 Architecture Decision Records

_Former file: `47_ADR.md`._

### Status

**ADR content exists and influences the architecture, but repository-wide ADR
governance is inconsistent.** Decision state and implementation state must be
tracked independently. An accepted or “locked” ADR is not proof of code.

### Source Map

| Range | Location | Notes |
|---|---|---|
| ADR-001..005 | `docs/fikirler.md` | Early combined record; explicitly contains historical/stale backend and IR material |
| ADR-006..028 | `docs/adr-frontend-analiz.md` | Combined frontend/runtime decision record with aggregate implementation claims |
| ADR-008 | `docs/adr/ADR-008-kisa-devre-mantiksal-operatorler.md` | Separate accepted short-circuit decision |
| ADR-029..041 | `docs/adr/ADR-*.md` | Mostly one file per decision |

There is an ID collision: ADR-008 means optimizer placement in the combined
frontend record and short-circuit logical evaluation in the standalone file.
Always cite filename plus title, not number alone, until the collision is
resolved by an explicit migration/index decision.

### Existing Status Reality

Status vocabulary is not normalized:

- standalone ADR-008 and ADR-029/030/032 say accepted;
- ADR-031 and ADR-033 label themselves implemented;
- ADR-034 says accepted/design locked;
- ADR-041 explicitly says accepted/locked;
- ADR-035..040 contain decisions but no consistent top-level status field;
- combined records mix decision, implementation summary, revisions, rejected
  alternatives, and roadmap prose.

Known supersession/revision links include ADR-032 revising ADR-015 and early
ADR-001/005 backend direction. The combined frontend record also marks parts of
ADR-014 as invalidated by ADR-020. Historical text remains useful rationale, but
must not be read as current architecture without following those links.

Some ADR implementation narratives are stale or broader than active code. For
example, ADR-030 describes heavy/light IR as though two IR products exist, while
the active optimized path selects an optimized AST and produces one in-memory
IR program. ADR-032 contains upstream/performance estimates, not executed Saqut
benchmarks. ADR-041 is a target self-hosted stdlib architecture, not current
runtime completion.

### Interpretation Model

Use two independent fields:

1. **Decision state:** Proposed, Accepted, Rejected, Superseded.
2. **Implementation state:** Planned, Partially Implemented, Implemented, Not
   Verified.

For existing ADRs, preserve the text's explicit state. If it is absent, write
`Decision state: Not explicitly recorded`; do not infer “Accepted” merely from a
`Karar` heading. Verify implementation against active source and tracked tests.
Unexecuted tests establish intended contracts, not passing behavior.

Compiler behavior authority remains:

`tests > compiler source > ADR > language spec > public docs > old AI/TODO notes`

An ADR can require future code, but cannot make absent behavior true. Conversely,
an accidental code path does not silently supersede an accepted architectural
decision; reconcile it through code correction or a new ADR.

### New ADR Requirements

Create or revise an ADR for changes to language semantics, pipeline ownership,
typed IR/opcodes, backend/reference behavior, memory/GC/concurrency model,
serialization/binary/ABI contracts, package resolution, security/capability
boundaries, determinism/versioning, or public compatibility policy.

A new ADR should contain: unique ID/title, date, decision state, context,
constraints, decision, alternatives, consequences, affected contracts,
supersedes/superseded-by links, implementation plan, implementation status, and
verification requirements. Local refactors that preserve all contracts normally
need no ADR.

Supersession must name the exact replaced clauses and update an ADR index; never
rewrite history silently. Implementation completion requires source integration
and appropriate tracked tests, with execution results recorded separately.

### Knowledge Base and Public Docs

ADRs own rationale. The knowledge base maps the current verified impact and
uncertainty. Public docs explain user-facing behavior and remain non-authoritative
for compiler semantics. A public-doc conflict is fixed in the derived docs; it
does not amend an ADR.

Evidence: all tracked ADR sources, active subsystem code, and
[01_Sources.md](00_Orientation.md#kb-01-sources). Static review: 2026-07-25.

See [03_Principles.md](01_Governance.md#kb-03-principles), [06_Invariants.md](01_Governance.md#kb-06-invariants),
[49_DocPolicy.md](01_Governance.md#kb-49-docpolicy), and [52_Roadmap.md](01_Governance.md#kb-52-roadmap).

---

<a id="kb-48-coding"></a>
## 48 Coding

_Former file: `48_Coding.md`._

### Amacı
C++ uygulama standartlarını, yerel kalıpları ve değişiklik disiplini kurallarını tanımlamak.

### İçereceği başlıklar
- Dil standardı ve biçimlendirme
- Sahiplik, hata ve include kuralları
- Header/source organizasyonu
- Refactoring ve test beklentileri

### Tahmini uzunluk
600-900 kelime.

### Bağımlı olduğu diğer markdown dosyaları
`00_Orientation.md#kb-07-repository`, `00_Orientation.md#kb-05-architecture`, `07_Engineering.md#kb-39-testing`, `01_Governance.md#kb-49-docpolicy`.

### Bu dosyanın neden gerekli olduğu
AI tarafından üretilen kodun projenin mevcut C++ tarzı ve modül sınırlarıyla tutarlı olmasını sağlar.

---

<a id="kb-49-docpolicy"></a>
## 49 Documentation Policy

_Former file: `49_DocPolicy.md`._

### Document Classes

| Class | Audience | Responsibility |
|---|---|---|
| Tests and source | Compiler maintainers and tools | Actual behavioral contract and implementation |
| ADRs | Maintainers | Why a decision was made and its explicit state |
| Language specification | Implementers/users | Normative language rules when a canonical spec exists |
| Knowledge base | AI agents and maintainers | Compact map of verified architecture, status, and constraints |
| Public docs | Language users and external agents | Guides, tutorials, references, examples, and release-facing content |
| Old AI/TODO notes | Historical planning | Discovery hints only |

Public docs and this knowledge base are derived views. They must cite or point to
stronger evidence rather than become competing semantic authorities.

### Required Status Language

Use only evidence-backed labels:

- `Implemented`: active code structure is visible.
- `Partially Implemented`: active code exists with explicit coverage gaps.
- `Planned`: design/stub/TODO only.
- `Not Verified`: execution or deployment has not been observed.
- `Unknown`: sources inspected so far do not answer the question.

Do not convert an accepted ADR into an implementation claim. Do not convert a
registered CLI command, flag, public page, issue, or example into proof that the
feature works.

### Single-Source Rules

- Compiler behavior priority is defined once in [01_Sources.md](00_Orientation.md#kb-01-sources).
- Architecture summaries link to subsystem files instead of copying detailed
  opcode, grammar, or API inventories.
- ADR rationale remains in ADRs. Knowledge-base files summarize current impact
  and retain the ADR's status.
- Public docs explain user-facing behavior; internal ownership, uncertainty, and
  refactoring constraints belong in this knowledge base.
- Generated `llms*.txt`, Markdown copies, and built HTML are regenerated from
  source. Never hand-maintain them as canonical content.

Small duplication is allowed for safety-critical rules such as authority order,
nested-repository boundaries, and “no silent JIT fallback.” Duplicated rules
must link back to their owning document.

### Maintenance Triggers

Update the relevant knowledge-base and public-doc mappings when any of these
change:

- grammar, AST shape, types, diagnostics, modules, or observable semantics;
- IR opcode/data contracts, optimizer behavior, or backend support;
- CLI commands, flags, outputs, exit behavior, LSP, or DAP capabilities;
- builtin/FFI/stdlib signatures or capability requirements;
- repository ownership, generated artifacts, or build/release layout;
- public docs routes/locales, machine interfaces, or delivery configuration.

If code and docs disagree, fix the derived document or label the conflict. Do not
change compiler behavior merely to make a public page true without a deliberate
technical decision.

### Review Record

Each populated technical file should state the evidence basis and review date
when freshness matters. A static inspection can confirm source structure, not
runtime success. A live deployment observation must record the URL, date, request
conditions, and response evidence in the appropriate delivery document.

### Repository-Specific Rules

- Compiler documentation changes belong to the root repository.
- Public site changes belong to the nested `saqutwebside/` repository and must
  follow its `AGENTS.md`.
- Preserve local/uncommitted work in both repositories.
- Website content work must update EN/TR and navigation mappings as required by
  the site rules.

See [54_PublicDocs.md](08_PublicDocs.md#kb-54-publicdocs) and
[57_DocsSync.md](08_PublicDocs.md#kb-57-docssync).

---

<a id="kb-50-contributing"></a>
## 50 Contributing

_Former file: `50_Contributing.md`._

### Amacı
İnsan katkıcıların geliştirme ortamını, iş akışını ve değişiklik kabul koşullarını tanımlamak.

### İçereceği başlıklar
- Ortam kurulumu
- Compiler ve `saqutwebside/` repository sınırları
- Issue, branch ve commit akışı
- Kod, test ve belge beklentileri
- Review ve karar yükseltme süreci

### Tahmini uzunluk
500-800 kelime.

### Bağımlı olduğu diğer markdown dosyaları
`07_Engineering.md#kb-38-build`, `07_Engineering.md#kb-39-testing`, `01_Governance.md#kb-47-adr`, `01_Governance.md#kb-48-coding`, `01_Governance.md#kb-49-docpolicy`, `08_PublicDocs.md#kb-55-astro`, `08_PublicDocs.md#kb-62-docstesting`.

### Bu dosyanın neden gerekli olduğu
Teknik sözleşmeleri tekrar etmeden katkı sürecinin operasyonel giriş noktasını sağlar.

---

<a id="kb-52-roadmap"></a>
## 52 Roadmap

_Former file: `52_Roadmap.md`._

### Amacı
Doğrulanmış mevcut durumdan ayrı olarak gelecek hedefleri, bağımlılıkları ve karar kapılarını tutmak.

### İçereceği başlıklar
- Yakın dönem hedefler
- Bağımlılık ve blokaj haritası
- Karar bekleyen konular
- Docs versioning, playground ve diğer web araçlarının plan durumu
- Uzak vizyon ve açıkça kapsam dışı fikirler

### Tahmini uzunluk
600-900 kelime.

### Bağımlı olduğu diğer markdown dosyaları
`00_Orientation.md#kb-01-sources`, `00_Orientation.md#kb-02-project`, `00_Orientation.md#kb-04-status`, `07_Engineering.md#kb-45-versioning`, `01_Governance.md#kb-47-adr`, `08_PublicDocs.md#kb-61-docsversions`, `09_WebPlatform.md#kb-63-webtools`.

### Bu dosyanın neden gerekli olduğu
Planları uygulanmış gerçeklerden ayırır ve eski TODO'ların kalıcı mimari bilgiye dönüşmesini engeller.

---

<a id="kb-53-glossary"></a>
## 53 Glossary

_Former file: `53_Glossary.md`._

### Amacı
Projeye özgü terimleri, kısaltmaları ve benzer görünen kavramların kesin anlamlarını tanımlamak.

### İçereceği başlıklar
- Derleyici aşaması terimleri
- saQut'a özgü kavramlar
- Backend ve runtime terimleri
- Public docs, source page, generated Markdown ve deployed artifact terimleri
- Karıştırılmaması gereken terim çiftleri

### Tahmini uzunluk
400-700 kelime.

### Bağımlı olduğu diğer markdown dosyaları
`00_Orientation.md#kb-02-project`, `00_Orientation.md#kb-05-architecture`, `04_IR_Backends.md#kb-19-ir`, `01_Governance.md#kb-49-docpolicy`, `08_PublicDocs.md#kb-54-publicdocs`.

### Bu dosyanın neden gerekli olduğu
Özellikle lexer/tokenizer, builtin/stdlib/FFI ve runtime/VM gibi terimlerin tutarlı kullanılmasını sağlar.
