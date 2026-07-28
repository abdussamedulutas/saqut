# Public Documentation System

> Consolidated knowledge-base document. Each numbered section preserves
> the responsibility and content of one former focused Markdown file.
> Former filenames remain recorded for traceability; internal links point
> to their new section locations.

## Section Map

- [54 Public Documentation System](#kb-54-publicdocs)
- [55 Astro](#kb-55-astro)
- [56 DocsContent](#kb-56-docscontent)
- [57 Documentation Synchronization](#kb-57-docssync)
- [61 DocsVersions](#kb-61-docsversions)
- [62 DocsTesting](#kb-62-docstesting)

---

<a id="kb-54-publicdocs"></a>
## 54 Public Documentation System

_Former file: `54_PublicDocs.md`._

### Model

`saqutwebside/` is a **public documentation system**, not merely a frontend and
not part of the compiler implementation tree. It is a nested Git repository
built with Astro and Starlight. It owns:

- the official saqut.com guides, tutorials, setup, CLI, builtin/stdlib, FFI,
  diagnostics, compiler internals, and release-facing content;
- English root routes and Turkish `/tr/` routes;
- navigation, presentation, sitemap/SEO inputs, public examples, and future web
  tools;
- machine-consumption surfaces such as generated Markdown, `llms.txt`,
  `llms-full.txt`, `.well-known` resources, Link headers, and
  `Accept: text/markdown` delivery intent.

This system describes the compiler to users and agents. It does not own compiler
semantics, runtime code, or backend behavior.

### Authoritative Source Rule

**Public docs, compiler semantics için authoritative source değildir.**

Compiler behavior source order:

`tests > compiler source > ADR > language spec > public docs > old AI/TODO notes`

When a public page conflicts with stronger evidence, label the page as stale or
correct it. Never modify an implementation assumption solely because the site
states it.

### Internal Layers

1. **Content source:** `src/content/docs/` Markdown/MDX, with Turkish content
   under `tr/`.
2. **Platform/config source:** `astro.config.mjs`, `src/content.config.ts`,
   components/styles, middleware, package metadata, static endpoint source,
   nginx and header configuration.
3. **Generated artifacts:** `dist/`, `.astro/`, generated Markdown copies, and
   `public/llms.txt`/`public/llms-full.txt`.
4. **Observed deployment:** actual Cloudflare/nginx/origin HTTP responses.

Only layers 1 and 2 are source. Layer 3 is reproducible output. Layer 4 must be
measured separately and is `Not Verified` in the current review.

### Compiler Relationship

Public docs consume a versioned view of these compiler-owned contracts:

- syntax, AST-visible forms, types, modules, errors, and diagnostics;
- CLI command/flag/output behavior;
- VM/JIT/backend status and limitations;
- builtin, curated FFI, capability, and eventual stdlib APIs;
- installation artifacts and release/version support.

Changes flow from compiler evidence to docs. The reverse flow is a feature
proposal, not an implementation change. [57_DocsSync.md](08_PublicDocs.md#kb-57-docssync)
defines the synchronization contract.

### Current Static Status

- Astro/Starlight, bilingual content directories, explicit sidebar, sitemap
  integration, middleware, build-asset generation, nginx configuration, and
  static machine endpoints exist in source: `Implemented`.
- Whether the site builds, all EN/TR slugs match, links resolve, or generated
  outputs are current: `Not Verified` in this review.
- Link headers and `Accept: text/markdown` behavior are represented by intended
  configurations, but observed deployed behavior is `Not Verified`.
- The asset script generates `llms.txt`, `llms-full.txt`, and Markdown copies.
  These outputs are not semantic sources.
- Automatic extraction and compiler testing of public code fences is `Planned`.
- Versioned docs, playground, and future web tools are `Planned` or `Unknown`
  until dedicated source is established.

### Scope Separation

- SEO concerns discovery by search engines and page metadata.
- Agent discovery concerns machine interfaces and content negotiation.
- Web delivery concerns which layer actually applies headers/routes.
- Content governance concerns factual correctness and bilingual maintenance.

Keep these responsibilities in `09_WebPlatform.md#kb-60-seo`, `09_WebPlatform.md#kb-59-agentdocs`,
`09_WebPlatform.md#kb-58-webdelivery`, and `08_PublicDocs.md#kb-56-docscontent`; do not collapse them back into a
generic “Website” file.

---

<a id="kb-55-astro"></a>
## 55 Astro

_Former file: `55_Astro.md`._

### Amacı
`saqutwebside/` içindeki Astro ve Starlight uygulamasının teknik yapısını tanımlamak.

### İçereceği başlıklar
- Astro/Starlight sürümleri ve proje giriş noktaları
- Content collection ve frontmatter şeması
- Routing, sidebar ve locale yapılandırması
- Markdown/MDX, CSS ve static asset sınırları
- Kaynak, cache ve üretilmiş dizinler

### Tahmini uzunluk
500-800 kelime.

### Bağımlı olduğu diğer markdown dosyaları
`00_Orientation.md#kb-07-repository`, `07_Engineering.md#kb-38-build`, `08_PublicDocs.md#kb-54-publicdocs`, `08_PublicDocs.md#kb-56-docscontent`, `09_WebPlatform.md#kb-58-webdelivery`.

### Bu dosyanın neden gerekli olduğu
Genel dokümantasyon politikasını belirli bir framework uygulamasına bağlamadan site kodunun nasıl organize edildiğini açıklar.

---

<a id="kb-56-docscontent"></a>
## 56 DocsContent

_Former file: `56_DocsContent.md`._

### Amacı
Public docs içerik mimarisini, yazım kurallarını, çeviri düzenini ve kullanıcıya açık örneklerin editoryal sözleşmesini tanımlamak.

### İçereceği başlıklar
- Sayfa türleri ve bilgi mimarisi
- İngilizce/Türkçe slug ve içerik eşlemesi
- Frontmatter, sidebar ve internal link kuralları
- Dil rehberi, tutorial, API ve release içeriği kuralları
- Üslup, terimler ve planlanmış özellik işaretleme

### Tahmini uzunluk
650-1000 kelime.

### Bağımlı olduğu diğer markdown dosyaları
`02_Language.md#kb-09-language`, `02_Language.md#kb-10-syntax`, `01_Governance.md#kb-49-docpolicy`, `01_Governance.md#kb-53-glossary`, `08_PublicDocs.md#kb-54-publicdocs`, `08_PublicDocs.md#kb-57-docssync`.

### Bu dosyanın neden gerekli olduğu
Kullanıcı içeriği üretme kurallarını Astro yapılandırmasından ve genel belge yönetişiminden ayrı tutar.

---

<a id="kb-57-docssync"></a>
## 57 Documentation Synchronization

_Former file: `57_DocsSync.md`._

### Purpose

Keep public documentation aligned with compiler evidence without allowing docs
to become a second language specification.

### Source Mapping

| Public topic | Stronger compiler evidence |
|---|---|
| Syntax and language constructs | Tracked parser/semantic tests, tokenizer/parser/AST source, accepted-and-implemented ADRs, canonical spec when available |
| Types, casts, nullability | Tracked type/golden tests, `src/core/type.hpp`, TypeChecker, IR/VM handling |
| CLI | `src/main.cpp`, `src/cli/args.hpp`, actual command handlers, tracked CLI/golden tests |
| Diagnostics | Tracked negative tests, emit sites, diagnostic data structures/catalog |
| Builtins | Registry, TypeChecker lookup, IR lowering, VM dispatch, tracked golden tests |
| FFI/capabilities | Embedded declarations, catalog, symbol binding, `CALLHOST`, host table, tracked tests |
| Backends/performance | Active VM/JIT source, differential/benchmark definitions, executed results when available |
| Memory/GC | VM value/object/interpreter source and tracked GC tests; JIT must be assessed separately |

Public pages must state `Planned`, `Partially Implemented`, or `Not Verified`
where stronger evidence does not support an unqualified claim.

### Change Triggers

A compiler change requires docs impact review when it changes:

- accepted syntax, semantics, type rules, diagnostics, or module behavior;
- a CLI command, option, output schema, exit contract, LSP, or DAP capability;
- builtin/FFI/stdlib names, signatures, return values, side effects, or
  capabilities;
- backend coverage, fallback/rejection behavior, memory model, or supported
  platform/version;
- a public example or tutorial's expected result.

Update the compiler source/test/ADR first, then derived knowledge-base facts, then
EN/TR public pages and navigation. Planned pages must remain visibly planned.

### Conflict Handling

1. Apply the authority order from [01_Sources.md](00_Orientation.md#kb-01-sources).
2. Confirm that the apparently stronger source is active and tracked.
3. Preserve ADR state; accepted target architecture may still be unimplemented.
4. Correct or relabel public docs.
5. Regenerate artifacts from source; never patch generated output as the fix.

### Automatically Testable Code Fences

This is a **planned contract**. A general extraction/test pipeline was not found
in the inspected source.

Every saQut code fence that is intended to become executable should be
machine-classifiable. The future schema must support these classes:

- `parse`: syntax must parse.
- `check`: semantic analysis must succeed.
- `run`: program must execute with declared output.
- `compile-error`: a declared diagnostic/failed exit is expected.
- `runtime-error`: compilation succeeds and a declared runtime failure is expected.
- `illustrative`: intentionally incomplete; excluded with an explicit reason.
- `planned`: documents a future surface and must never enter current success tests.

The extractor should preserve:

- page path, locale, stable example ID, and code-fence class;
- required wrapper/entry point where the snippet is not a full program;
- expected stdout, stderr/diagnostic code, and exit code;
- required capability flags and program arguments;
- parser/runtime/backend selection and minimum documented version.

Extracted fixtures should link back to the source page and stable example ID.
Parser classes should feed parser/semantic fixtures; `run` and runtime-error
classes should feed the appropriate VM/runtime tests. JIT participation must be
explicit because JIT coverage is partial and whole-program gated.

### EN/TR and Version Synchronization

- Equivalent EN/TR slugs should describe the same status and code contract.
- Code example IDs should match across locales when the program is semantically
  identical.
- Translation wording may differ; expected program behavior may not.
- When versioned docs are introduced, examples must declare the compiler version
  they target. Current pages must not retroactively redefine old behavior.

### Completion Gate

A docs-affecting compiler change is complete only when:

- stronger compiler evidence is updated;
- affected knowledge-base files are updated;
- affected EN/TR source pages are updated or explicitly deferred;
- code fences are correctly classified;
- generated artifacts are regenerated by their owning workflow;
- required docs checks are run and recorded.

The current review did not run any of these checks. See `08_PublicDocs.md#kb-62-docstesting` for
the planned test categories.

---

<a id="kb-61-docsversions"></a>
## 61 DocsVersions

_Former file: `61_DocsVersions.md`._

### Amacı
Public belgelerin compiler sürümleriyle eşleşmesini, release notes ve geçmiş sürüm erişimini tasarlamak.

### İçereceği başlıklar
- Güncel durum ve henüz uygulanmamış alanlar
- Docs sürümü ile compiler sürümü eşlemesi
- Release notes ve compatibility bannerları
- Eski sürüm URL, arşiv ve redirect politikası
- Planlanmış ve kaldırılmış özelliklerin gösterimi

### Tahmini uzunluk
500-800 kelime.

### Bağımlı olduğu diğer markdown dosyaları
`00_Orientation.md#kb-04-status`, `07_Engineering.md#kb-45-versioning`, `07_Engineering.md#kb-46-release`, `01_Governance.md#kb-52-roadmap`, `08_PublicDocs.md#kb-56-docscontent`, `08_PublicDocs.md#kb-57-docssync`, `09_WebPlatform.md#kb-60-seo`.

### Bu dosyanın neden gerekli olduğu
Tek bir “latest” belge kümesinin geçmiş compiler sürümleri için yanlış davranış iddia etmesini önler.

---

<a id="kb-62-docstesting"></a>
## 62 DocsTesting

_Former file: `62_DocsTesting.md`._

### Amacı
Public docs kaynakları, üretilmiş artifactler ve canlı dağıtım için otomatik doğrulama katmanlarını tanımlamak.

### İçereceği başlıklar
- Test ortamı ve fixture sahipliği
- Yerel, CI ve canlı smoke test sınırları
- Başarısızlık sınıflandırması
- Compiler release ve docs deployment kapıları

### Test Kategorileri

#### Build test
Astro ve post-build artifact üretiminin doğrulanması.

#### Broken link test
İç ve dış bağlantıların doğrulanması.

#### EN/TR slug parity test
İngilizce ve Türkçe sayfa kümelerinin eşleşmesinin doğrulanması.

#### Code fence extraction test
Sınıflandırılmış saQut kod bloklarının çıkarılması ve compiler testlerine hazırlanması.

#### Live HTTP header smoke test
Canlı Content-Type, Vary, Link, cache ve güvenlik başlıklarının doğrulanması.

#### `Accept: text/markdown` smoke test
HTML URL'lerinin Markdown negotiation davranışının doğrulanması.

### Tahmini uzunluk
650-950 kelime.

### Bağımlı olduğu diğer markdown dosyaları
`02_Language.md#kb-27-determinism`, `07_Engineering.md#kb-38-build`, `07_Engineering.md#kb-39-testing`, `08_PublicDocs.md#kb-55-astro`, `08_PublicDocs.md#kb-57-docssync`, `09_WebPlatform.md#kb-58-webdelivery`, `09_WebPlatform.md#kb-59-agentdocs`, `09_WebPlatform.md#kb-60-seo`.

### Bu dosyanın neden gerekli olduğu
İçerik doğruluğu, site build'i ve canlı HTTP davranışının tek bir başarılı Astro build'iyle doğrulanmış sayılmasını önler.
