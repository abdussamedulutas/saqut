# saQut CLI — Parametre Formatı Tasarımı

> Durum: **tasarım belgesi** — bu belge yazıldığı turda kod değişmedi.
> Seçilen alternatif onaylandıktan sonra uygulaması 0.8.0'da yapılır
> (bkz. `PLAN.md`).

## Neden şimdi?

Bugün CLI düz `saqut <komut> [dosya] [bayrak...]` (`run`, `tokens`, `ast`,
`symbols`, `check`, `ir`, `exec`, `bench`, `lsp`, `dap`) — hepsi tek seviye,
`src/cli/args.hpp`'deki elle yazılmış tek geçişli ayrıştırıcıdan geçiyor.
Yakın gelecekte gelecek komutlar bu modelin dışına taşıyor:

- `saqut project create` — **alt-komut grubu** (`project` altında `create`,
  muhtemelen `init`, `add` vb.)
- `saqut mcp --port 8080` — uzun ömürlü servis komutu, tip'li bayrak (int)
- `saqut query ./project -only-struct -name 'Person*'` — pozisyonel +
  çok bayrak + değerli bayrak (glob pattern) + gelecekte muhtemelen
  `--format json`, `--recursive` gibi ek seçenekler

Mevcut `CliArgs` (tek düz struct, `positional: vector<string>`, birkaç
`bool`/`int` alanı) ve `CliDispatcher` (isimden `execute(args)` bulan düz
liste) bu üç örneği karşılayamaz: alt-komut kavramı yok, bayrak tipleri
(int/string/bool/liste) elle `stoi`/substring ile çözülüyor, her yeni bayrak
`args.hpp`'ye bir alan + `parseArgs`'a bir `if` demek (halihazırda `--allow-fs`,
`--allow-net`, `--allow-sys`, `--capabilities`, `--gc-threshold=N` gibi 15+
bayrak birikti — bu dosya da kendi büyüme sorununu yaşıyor).

## Bugünkü örnekler + gelecekteki örnekler (üç formatta yan yana)

| Senaryo | Bugün | Alternatif 1 (alt-komut) | Alternatif 2 (Deno-tarzı düz) | Alternatif 3 (hibrit, önerilen) |
|---|---|---|---|---|
| Çalıştır | `saqut run --allow-fs prog.sqt` | `saqut run --allow-fs prog.sqt` | `saqut run --allow-fs prog.sqt` | `saqut run --allow-fs prog.sqt` |
| Proje oluştur | *(yok)* | `saqut project create myapp` | `saqut project-create myapp` | `saqut project create myapp` |
| MCP servisi | *(yok)* | `saqut mcp start --port 8080` | `saqut mcp --port 8080` | `saqut mcp --port 8080` |
| Sembol sorgusu | *(yok)* | `saqut query run ./project --only-struct --name 'Person*'` | `saqut query ./project --only-struct --name 'Person*'` | `saqut query ./project --only-struct --name 'Person*'` |
| Çoklu capability | `--allow-fs` (tek tek) | aynı | `--allow=fs,net` (liste) | `--allow-fs --allow-net` **+** `--allow=fs,net` ikisi de |

## Alternatif 1 — Git/Docker tarzı iç içe alt-komut

Her komut kendi alt-komut ağacına sahip: `saqut <grup> <eylem> [argümanlar] [bayraklar]`.
`run`/`ir`/`ast` gibi bugünkü düz komutlar da `saqut compile run` gibi bir
gruba taşınabilir (kırılma: mevcut kullanıcı alışkanlığı bozulur).

**Artı:**
- En tanıdık model (git, docker, kubectl, cargo) — kullanıcı hiç öğrenmeden tahmin eder.
- `project create`, `project init`, `project add` gibi doğal aile büyümesi.
- Her grup kendi yardım metnini taşıyabilir (`saqut project --help`).

**Eksi:**
- Bugünkü `saqut run prog.sqt` gibi tek-kelime komutları da gruplamaya
  zorlarsa (`saqut compile run`) **kırılma** olur; gruplamazsa iki farklı
  komut stili (düz + iç içe) bir arada yaşar — tutarsız görünür.
- `query` gibi "eylem = komutun kendisi" olan durumlarda (`saqut query ...`)
  gereksiz bir ekstra seviye ister (`saqut query run ...` tuhaf durur).

## Alternatif 2 — Deno tarzı düz komut + zengin bayrak

Tüm komutlar tek seviyede kalır (`saqut mcp`, `saqut project-create`,
`saqut query`), ayrım tire ile (`kebab-case`) yapılır. Zengin bayrak
sistemi: `--allow=fs,net` gibi virgüllü liste, `--` pozisyonel ayracı
(zaten ADR-035 ile capability bayraklarında bu model kısmen kuruldu).

**Artı:**
- saQut'un capability modeli (`--allow-fs` vb.) zaten doğrudan Deno'yu
  örnek alıyor (ADR-032, ADR-035) — tutarlılık avantajı, tek zihinsel model.
- En basit ayrıştırıcı: hiç alt-komut ağacı yok, düz `command → handler` map.
- Geriye dönük uyumlu: mevcut `saqut run`/`saqut ir` hiç değişmez.

**Eksi:**
- `project-create`, `project-init`, `project-add` gibi bir aile büyüdükçe
  komut adları uzar ve gruplama görsel olarak kaybolur (`saqut project-create`
  ile `saqut project-init` arasındaki ilişki isimden başka yerde görünmez).
- `saqut mcp --port 8080` gibi **uzun ömürlü servis** komutları ile `saqut run`
  gibi **tek atımlık** komutlar aynı düzeyde durur — kavramsal karışıklık.

## Alternatif 3 — Hibrit: düz komutlar + opsiyonel alt-komut grubu (ÖNERİLEN)

Kural: **bugünkü düz komutlar (`run`, `tokens`, `ast`, `symbols`, `check`,
`ir`, `exec`, `bench`, `lsp`, `dap`) hiç değişmez** — geriye dönük uyumluluk
korunur, mevcut scriptler/dokümantasyon kırılmaz. Yalnızca **gerçekten bir
alt-komut ailesi gerektiren yeni alanlar** (`project`, muhtemelen ileride
`cache`, `config`) grup alır; tek-eylemli yeni komutlar (`mcp`, `query`) düz
kalır. Tüm bayraklar tek tip bir ayrıştırıcıdan geçer:

- `--name value` ve `--name=value` ikisi de kabul edilir (bugünküyle aynı).
- Kısa alias (`-o`, `-v`) yalnızca gerçekten sık kullanılan bayraklarda.
- `--` pozisyonel/program-argümanı ayracı (ADR-035 ile `sys::args()` için
  zaten kuruldu) korunur.
- Çok-değerli bayraklar için **iki syntax birden** kabul edilir: tekrar eden
  bayrak (`--allow-fs --allow-net`) ve virgüllü liste (`--allow=fs,net`) —
  ikisi de aynı `set<Capability>`'ye toplanır. Kullanıcı hangisini
  yazdıysa çalışır; belgeler tekrar-eden formu birincil gösterir (mevcut
  `--allow-fs` kullanıcı alışkanlığını bozmaz).
- `query` gibi filtre-ağır komutlarda `-only-struct` (tek tire, bayrak
  değeri yok — boolean switch) ile `-name 'Person*'` (tek tire, değerli)
  karışık kullanılabilir; ayrıştırıcı tek-tire ve çift-tire'yi eşdeğer kabul
  eder (yaygın CLI toleransı, `find`/`grep` alışkanlığına yakın).

**Artı:**
- Sıfır kırılma — bugünkü hiçbir komut/script/golden-test etkilenmez.
- Yeni büyüyen aileler (`project`) doğal grup alır, tek-atımlık yeni
  komutlar (`mcp`, `query`) gereksiz seviye almaz.
- Tek ayrıştırıcı çekirdeği → tutarlı hata mesajları, tutarlı `--help`.

**Eksi:**
- "Ne zaman grup, ne zaman düz komut" kararı öznel — büyüdükçe tutarsızlık
  riski var (hafifletme: bu belgeye yeni komut eklerken kısa bir kural
  yazıldı, aşağıya bak).

### Grup mu, düz komut mu? (karar kuralı)

Yeni bir komut eklerken: **komutun altında en az 2 farklı eylem varsa VE
bu eylemler aynı isim alanını (`saqut project X` gibi bir "ad") paylaşıyorsa**
→ alt-komut grubu. Aksi halde (tek eylem, doğrudan bir işi yapıyor) → düz
komut. Örnek: `project create/init/add` → grup (üç eylem, ortak "proje"
kavramı). `mcp --port 8080` → düz (tek eylem: sunucuyu başlat, `--port` bir
konfigürasyon bayrağı, ayrı bir "eylem" değil). `query` → düz (tek eylem:
sorgula; `-only-struct`/`-name` filtre bayrakları, ayrı eylemler değil).

## Önerilen hedef mimari (`src/cli/` için, 0.8.0'da uygulanacak)

Bugünkü `CliArgs` (düz struct) + `parseArgs()` (tek fonksiyon, ~15 `if`)
yerine:

```cpp
// src/cli/arg_spec.hpp — her komut kendi bayrak/pozisyonel şemasını bildirir
struct FlagSpec {
    std::string name;        // "allow-fs", "port"
    char        shortAlias;  // 'o', 0 = yok
    FlagKind    kind;        // Bool | String | Int | StringList
    std::string help;
};

struct CommandSpec {
    std::string           name;         // "run", "project"
    std::vector<std::string> subcommands; // boş = düz komut; ["create","init"] = grup
    std::vector<FlagSpec>  flags;
    std::string            help;
};
```

`CliDispatcher` bugünkü gibi `CommandSpec` listesi tutar; `--help` her
komut için `flags`'ten otomatik üretilir (bugün elle yazılan `printHelp()`
yerine). Ayrıştırma tek bir `ParsedArgs parse(CommandSpec&, argv)` üzerinden
geçer — `CliArgs`'taki 15 ayrı bayrak alanı yerine `map<string, FlagValue>`
+ tip-güvenli okuyucular (`args.getBool("allow-fs")`, `args.getInt("port")`).

Bu mimari **bugünkü tüm komutları kırmadan** kademeli geçirilebilir: önce
`ArgParser` çekirdeği yazılır, sonra komutlar tek tek (her biri kendi PR'ı)
yeni şemaya taşınır — `run` ve `ir` ilk (en çok bayrak taşıyanlar, en çok
fayda), `tokens`/`ast`/`symbols` son (basit, az bayrak).

## Sonuç

**Alternatif 3 (hibrit) önerilir.** Sıfır kırılma + gelecekteki üç örneğin
(`project create`, `mcp --port`, `query -only-struct -name`) hepsini doğal
karşılıyor. Onay sonrası uygulama işi 0.8.0'da `ArgParser` çekirdeğiyle
başlar (bkz. `PLAN.md`).
