# saQut CLI — Parametre Formatı Tasarımı

> Durum: **tasarım belgesi** — bu belge yazıldığı turda kod değişmedi.
> Uygulaması 0.8.0'da yapılır (bkz. `PLAN.md`).
>
> v2 — önceki sürümde üç alternatif (alt-komut / Deno-tarzı düz / hibrit)
> sunulmuştu; hepsi her yeni özelliği ayrı bir `--allow-X` bayrağına
> çeviriyordu (`--allow-fs --allow-net --allow-sys --allow-time ...`).
> Bu, CLI büyüdükçe **bayrak patlaması**na yol açar — reddedildi. Bu sürüm
> tek bir karara odaklanıyor: **tekil yapılandırılmış bayrak (`--with`)**.

## Neden bayrak-başına-özellik yanlış

Bugün her capability kendi bayrağı: `--allow-fs`, `--allow-net`, `--allow-sys`.
Üç tanesi katlanabilir ama saQut'un kendi yol haritası bunun devamını
işaret ediyor — `--allow-time` (#76'da tartışılan), ileride belki
`--allow-net-write`/`--allow-net-read` gibi ayrımlar, MCP servisinin kendi
bayrakları (`--port`, `--host`, `--max-connections`), GC ayarları
(`--gc-threshold`, `--gc-stats`) zaten var. Her yeni özellik = yeni
`--allow-X` demek, `saqut --help` çıktısı bir noktadan sonra okunmaz hale
gelir ve `src/cli/args.hpp`'ye sürekli yeni `bool`/`int` alanı + yeni `if`
eklemek gerekir (şu an 15+ bayrak birikmiş durumda — bu dosyanın kendisi
zaten "tanrı struct" belirtisi gösteriyor).

## Karar: tekil yapılandırılmış bayrak — `--with`

Tüm **ortam/çalışma-zamanı yapılandırması** (capability'ler, kaynak
limitleri, servis ayarları) tek bir bayrak altında toplanır:

```
--with <spec>[,<spec>...]
spec := <anahtar>              (boolean açma — "fs" → true)
      | <anahtar>=<değer>      (tipli değer — "port=8080")
```

### Örnekler

```
saqut run --with fs prog.sqt
saqut run --with fs,net prog.sqt
saqut run --with fs,gc-threshold=1000,gc-stats prog.sqt
saqut mcp --with port=8080
saqut mcp --with port=8080,host=0.0.0.0
```

`--with` **tekrarlanabilir de** — script'lerde okunurluk için satır satır
yazmak isteyenler `--with fs --with net --with gc-stats` de yazabilir,
ayrıştırıcı hepsini aynı yapılandırma kümesine toplar. İki söz dizimi de
(virgüllü tek bayrak / tekrarlanan bayrak) aynı sonucu üretir — kullanıcı
hangisi okunur geliyorsa onu seçer.

### Kapsam sınırı: `--with` her şeyi yutmaz

`--with`, **çapraz-kesen ortam yapılandırması** içindir (capability, kaynak
limiti, servis bağlama ayarı). Bir komuta **özgü iş mantığı bayrağı DEĞİLDİR**
— örneğin `saqut query ./project -only-struct -name 'Person*'` içindeki
`-only-struct`/`-name` sorgunun kendi filtreleridir, "ortam" değildir; normal
bayrak olarak kalır. Ayrım kuralı: **birden fazla komutta anlamlı mı?**
(`fs`/`net`/`port`/`gc-threshold` evet — hem `run` hem `mcp` hem ileride
`build` bunlara ihtiyaç duyabilir) → `--with`. **Yalnızca bir komutun kendi
semantiği mi?** (`-only-struct` yalnızca `query`'de anlamlı) → normal bayrak.

## Alt-komut gruplaması (değişmedi)

Bayrak tasarımından bağımsız bir eksen: yeni komutların ne zaman grup
(`saqut project create`) ne zaman düz (`saqut mcp`, `saqut query`) olacağı.
Kural aynı kalıyor: **komutun altında en az 2 farklı eylem varsa VE ortak
bir "ad" paylaşıyorsa** → grup; tek eylemse → düz komut. Bugünkü düz komutlar
(`run`, `tokens`, `ast`, `symbols`, `check`, `ir`, `exec`, `bench`, `lsp`,
`dap`) hiç değişmez — geriye dönük uyumluluk korunur.

## Örnekler (nihai format)

| Senaryo | Komut |
|---|---|
| Çalıştır, fs izniyle | `saqut run --with fs prog.sqt` |
| Çalıştır, fs+net+GC ayarı | `saqut run --with fs,net,gc-threshold=1000 prog.sqt` |
| Proje oluştur | `saqut project create myapp` |
| MCP servisi, port | `saqut mcp --with port=8080` |
| Sembol sorgusu | `saqut query ./project -only-struct -name 'Person*'` |
| Statik capability raporu | `saqut ir --capabilities prog.sqt` *(mod-değiştirici, tek komuta özgü → `--with` değil)* |

## Mevcut `--allow-fs`/`--allow-net`/`--allow-sys`'in geleceği

Pre-1.0 olduğumuz için (ADR-035 henüz `0.7.0`'da kuruldu) geriye dönük
uyumluluk yükü taşımaya gerek yok — `--allow-fs` doğrudan `--with fs`'e
**değiştirilir** (deprecated alias yok, tek bir CLI dili). Bu, "eski/yeni
iki söz dizimi bir arada" karmaşasından kaçınır.

## Ayrıştırıcı tasarımı (`src/cli/`, 0.8.0'da uygulanacak)

```cpp
// src/cli/with_spec.hpp
enum class ConfigValueKind { Bool, Int, String };

struct ConfigValue {
    ConfigValueKind kind;
    bool        boolVal   = true;   // bare "fs" → true
    long long   intVal    = 0;      // "port=8080"
    std::string stringVal;          // "host=0.0.0.0"
};

// "--with fs,net,port=8080" → {"fs":true, "net":true, "port":8080}
std::unordered_map<std::string, ConfigValue> parseWith(const std::vector<std::string>& withArgs);
```

`CliArgs` bugünkü tek tek `bool allowFs`/`bool allowNet`/`bool allowSys`
alanları yerine tek `std::unordered_map<std::string, ConfigValue> with;`
taşır; tüketen kod (`Interpreter::setCapabilities` vb.) `with.count("fs")`
gibi sorgular. Yeni bir capability veya servis ayarı eklemek **`args.hpp`'ye
dokunmadan**, yalnızca tüketen tarafta bir `with.count("x")` kontrolüyle
yapılabilir hale gelir — bugünkü "her yeni bayrak = yeni alan + yeni `if`"
büyüme deseni ortadan kalkar.

Komut şeması (`CommandSpec`/alt-komut ayrımı) önceki tasarımdaki gibi kalır;
tek fark bayrak listesinde artık tek tek `--allow-fs` girdisi yerine
`--with`'in kabul ettiği anahtar kümesi belgelenir (`fs`, `net`, `sys`,
`gc-threshold`, `gc-stats`, `port`, `host`, ...).

Geçiş: `run`/`ir` önce (en çok yapılandırma taşıyanlar), sonra basit
komutlar. `mcp`/`project`/`query` doğrudan yeni modelle yazılır (henüz
kod yok, eski modele geçiş maliyeti yok).
