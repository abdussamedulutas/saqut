# Web Delivery and Machine Interfaces

> Consolidated knowledge-base document. Each numbered section preserves
> the responsibility and content of one former focused Markdown file.
> Former filenames remain recorded for traceability; internal links point
> to their new section locations.

## Section Map

- [58 WebDelivery](#kb-58-webdelivery)
- [59 AgentDocs](#kb-59-agentdocs)
- [60 SEO](#kb-60-seo)
- [63 WebTools](#kb-63-webtools)

---

<a id="kb-58-webdelivery"></a>
## 58 WebDelivery

_Former file: `58_WebDelivery.md`._

### Amacı
Public docs sisteminin build artifact üretimini, sunucu yapılandırmasını, Cloudflare katmanını ve canlı HTTP sözleşmesini tanımlamak.

### İçereceği başlıklar
- Astro build ve Python post-build aşamaları
- HTML, Markdown ve agent artifact üretimi
- nginx, `_headers`, middleware ve Cloudflare sorumlulukları
- Deployment, cache ve rollback akışı
- Canlı endpoint ve header doğrulama yöntemi

### Intended Configuration
- Repository içindeki Astro middleware, `_headers`, nginx ve build scriptlerinin amaçlanan davranışı
- Yapılandırma katmanları arasındaki öncelik ve sahiplik

### Observed Deployed Behavior
- Canlı `saqut.com` HTTP yanıtlarından doğrulanan davranış
- Amaçlanan yapılandırmadan sapmaların kaydı ve son doğrulama tarihi

### Tahmini uzunluk
650-1000 kelime.

### Bağımlı olduğu diğer markdown dosyaları
`05_Runtime.md#kb-33-security`, `07_Engineering.md#kb-38-build`, `07_Engineering.md#kb-46-release`, `08_PublicDocs.md#kb-54-publicdocs`, `08_PublicDocs.md#kb-55-astro`, `09_WebPlatform.md#kb-59-agentdocs`, `09_WebPlatform.md#kb-60-seo`, `08_PublicDocs.md#kb-62-docstesting`.

### Bu dosyanın neden gerekli olduğu
nginx, Astro middleware, Cloudflare ve canlı HTTP çıktısının birebir aynı olduğu varsayımını önler.

---

<a id="kb-59-agentdocs"></a>
## 59 AgentDocs

_Former file: `59_AgentDocs.md`._

### Amacı
Public docs sisteminin insanlar için HTML arayüzünden ve klasik SEO'dan ayrı makine tüketim yüzeylerini tanımlamak.

### İçereceği başlıklar
- Makine arayüzlerinin kapsam ve sahipliği
- Üretim kaynağı ile yayınlanan artifact ilişkisi
- Content negotiation ve discovery sözleşmeleri
- Endpoint, header ve içerik doğrulama kuralları
- Agent arayüzlerinin sürüm ve uyumluluk sınırı

### SEO Ayrımı
SEO ve agent discovery aynı sistem değildir. Sitemap, canonical metadata ve arama indeksleme `09_WebPlatform.md#kb-60-seo` kapsamındadır; aşağıdaki yüzeyler ayrı makine arayüzleridir.

#### `llms.txt`
Yapısal indeksin üretim ve yayın sözleşmesi.

#### `llms-full.txt`
Birleştirilmiş tam Markdown içeriğinin üretim ve yayın sözleşmesi.

#### `Accept: text/markdown`
HTML URL'lerinin content negotiation ile Markdown sunma sözleşmesi.

#### `.well-known` Endpointleri
API catalog, MCP card, agent skills ve authentication discovery kaynaklarının sınırı.

#### `Link` Header
RFC tabanlı agent discovery bağlantılarının amaçlanan ve canlı davranışı.

### Tahmini uzunluk
600-900 kelime.

### Bağımlı olduğu diğer markdown dosyaları
`05_Runtime.md#kb-28-serialization`, `08_PublicDocs.md#kb-54-publicdocs`, `08_PublicDocs.md#kb-56-docscontent`, `09_WebPlatform.md#kb-58-webdelivery`, `09_WebPlatform.md#kb-60-seo`, `08_PublicDocs.md#kb-62-docstesting`.

### Bu dosyanın neden gerekli olduğu
Agent discovery, Markdown negotiation ve üretilmiş makine belgelerinin SEO ayarı veya frontend ayrıntısı olarak kaybolmasını önler.

---

<a id="kb-60-seo"></a>
## 60 SEO

_Former file: `60_SEO.md`._

### Amacı
Public docs sisteminin arama motoru görünürlüğünü ve geleneksel web discovery sözleşmesini tanımlamak.

### İçereceği başlıklar
- Page title, description ve canonical URL
- Sitemap ve robots politikası
- Locale, hreflang ve çift dil indeksleme
- Structured metadata ve sosyal paylaşım alanları
- Redirect, 404 ve indekslenebilirlik kontrolleri

### Kapsam Sınırı
`llms.txt`, Markdown for Agents, `.well-known` endpointleri ve agent Link header davranışı `09_WebPlatform.md#kb-59-agentdocs` kapsamındadır.

### Tahmini uzunluk
450-700 kelime.

### Bağımlı olduğu diğer markdown dosyaları
`08_PublicDocs.md#kb-54-publicdocs`, `08_PublicDocs.md#kb-56-docscontent`, `09_WebPlatform.md#kb-58-webdelivery`, `09_WebPlatform.md#kb-59-agentdocs`, `08_PublicDocs.md#kb-61-docsversions`, `08_PublicDocs.md#kb-62-docstesting`.

### Bu dosyanın neden gerekli olduğu
Arama motoru optimizasyonu ile agent-facing makine arayüzlerinin farklı doğrulama ve evrim kurallarını korur.

---

<a id="kb-63-webtools"></a>
## 63 WebTools

_Former file: `63_WebTools.md`._

### Amacı
Planlanan playground ve diğer web tabanlı compiler araçlarının public docs sistemiyle ilişkisini ve sınırlarını tanımlamak.

### İçereceği başlıklar
- Güncel durum ve planlanmış araçlar
- Compiler/VM/WASM entegrasyon seçenekleri
- Sürüm eşleme ve davranış parity
- İzolasyon, capability ve kaynak limitleri
- Docs navigation ve deployment entegrasyonu

### Tahmini uzunluk
400-700 kelime.

### Bağımlı olduğu diğer markdown dosyaları
`04_IR_Backends.md#kb-20-vm`, `02_Language.md#kb-27-determinism`, `05_Runtime.md#kb-33-security`, `07_Engineering.md#kb-45-versioning`, `01_Governance.md#kb-52-roadmap`, `08_PublicDocs.md#kb-54-publicdocs`, `09_WebPlatform.md#kb-58-webdelivery`.

### Bu dosyanın neden gerekli olduğu
Henüz bulunmayan playground'ın mevcut özellik sanılmasını ve ileride doğrudan docs frontend koduna kontrolsüzce eklenmesini önler.
