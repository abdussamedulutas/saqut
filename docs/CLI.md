# saQut CLI — Parametre Formatı Tasarımı

> Durum: **tasarım belgesi** — bu belge yazıldığı turda kod değişmedi.
> Uygulaması 0.8.0'da yapılır (bkz. `PLAN.md`).
>
> v3 — v2'de "ortam/çalışma-zamanı yapılandırması" tek bir `--with` bayrağı
> altında toplanmıştı (capability + GC ayarları + servis ayarları hepsi
> bir arada). Kullanıcı geri bildirimi: **capability (permission sistemi)
> ile GC ayarı kavramsal olarak apayrı şeyler** — aynı bayrağa karıştırmak
> yanlış. Ayrıca GC yakında büyük bir yeniden tasarım geçirecek (ADR-022'nin
> ötesinde); bugünden GC bayrak şemasına yatırım yapmanın anlamı yok. Bu
> sürüm ikisini ayırıyor.

## Üç ayrı eksen, üç ayrı mekanizma

CLI'de büyüyecek üç FARKLI kategori var, hepsini tek kalıba zorlamak yanlıştı:

1. **Permission (capability)** — "bu programın dış dünyaya erişimine izin
   var mı?" sorusu. Güvenlik sınırı, ADR-035. Kendi bayrağı: `--allow`.
2. **Kaynak/runtime ayarı** (GC eşiği, ileride belki bellek limiti gibi) —
   davranışı ince ayarlayan, güvenlikle ilgisi olmayan sayısal/boolean
   parametreler. **Bunlar zaten kendi bayraklarıyla var** (`--gc-threshold`,
   `--gc-stats`) ve öyle kalıyor — GC yeniden tasarlanınca (yakında) bu
   bayraklar da o tasarımla birlikte gözden geçirilir; bugün önceden
   soyutlamaya gerek yok.
3. **Servis/komut yapılandırması** (MCP'nin `--port`/`--host` gibi) — belirli
   bir komuta ait, o komutun kendi bayrağı olarak kalır (`saqut mcp --port
   8080`) — bunun için de genel bir kap gerekmiyor, `mcp` zaten tek komut.

**Sonuç:** v2'deki tek `--with` kabı kaldırıldı. Yalnızca permission
kategorisi gerçekten "büyüyen bir aile" (fs, net, sys, ileride time, ...) —
o yüzden yalnızca O kategori kendi toplu bayrağını hak ediyor: `--allow`.

## Karar: permission'lar için `--allow`

```
--allow <spec>[,<spec>...]
spec := <capability adı>        (fs, net, sys, ileride time, ...)
```

```
saqut run --allow fs prog.sqt
saqut run --allow fs,net prog.sqt
saqut run --allow fs --allow net prog.sqt   (tekrarlanan bayrak da olur)
```

Yalnızca capability adları girer — boolean açma dışında bir şey taşımaz
(port/host gibi tipli değerler burada YOK, çünkü onlar permission değil).
Bu netlik `--allow`'u okuyan kişiye "bu satır güvenlik sınırını gösteriyor"
garantisini verir — karışık bir yapılandırma torbası değil.

## GC ve servis ayarları — kendi bayrakları, değişmedi

```
saqut run --allow fs --gc-threshold=1000 --gc-stats prog.sqt
saqut mcp --port 8080 --host 0.0.0.0
```

Bunlar bugünkü düz `--flag value` / `--flag=value` modelinde kalıyor.
GC tarafı özellikle: **büyük bir yeniden tasarım bekleniyor** (bkz. proje
notları) — bugünden bir "genel config kabı" icat edip GC bayraklarını oraya
taşımanın getirisi yok, tasarım değişince zaten yeniden yazılacak.

## Alt-komut gruplaması (değişmedi)

Bayrak tasarımından bağımsız bir eksen: yeni komutların ne zaman grup
(`saqut project create`) ne zaman düz (`saqut mcp`, `saqut query`) olacağı.
Kural: **komutun altında en az 2 farklı eylem varsa VE ortak bir "ad"
paylaşıyorsa** → grup; tek eylemse → düz komut. Bugünkü düz komutlar (`run`,
`tokens`, `ast`, `symbols`, `check`, `ir`, `exec`, `bench`, `lsp`, `dap`)
hiç değişmez.

## Örnekler (nihai format)

| Senaryo | Komut |
|---|---|
| Çalıştır, fs izniyle | `saqut run --allow fs prog.sqt` |
| Çalıştır, fs+net izni + GC ayarı | `saqut run --allow fs,net --gc-threshold=1000 prog.sqt` |
| Proje oluştur | `saqut project create myapp` |
| MCP servisi, port | `saqut mcp --port 8080` |
| Sembol sorgusu | `saqut query ./project -only-struct -name 'Person*'` |
| Statik capability raporu | `saqut ir --capabilities prog.sqt` |

## Mevcut `--allow-fs`/`--allow-net`/`--allow-sys`'in geleceği

Pre-1.0 olduğumuz için geriye dönük uyumluluk yükü taşımaya gerek yok —
üç ayrı bayrak (`--allow-fs`, `--allow-net`, `--allow-sys`) tek toplu
bayrağa (`--allow fs`, `--allow net`, `--allow sys`) **değiştirilir**
(deprecated alias yok).

## Ayrıştırıcı tasarımı (`src/cli/`, 0.8.0'da uygulanacak)

```cpp
// src/cli/allow_spec.hpp — yalnızca permission ayrıştırır, başka bir şey değil
std::set<Capability> parseAllow(const std::vector<std::string>& allowArgs);
```

`CliArgs::allowedCaps` (zaten `std::set<Capability>` olarak var, ADR-035) —
yalnızca `--allow-fs`/`--allow-net`/`--allow-sys` üç ayrı `if`'i tek
`--allow <liste>` ayrıştırmasına indirger, veri modeli değişmez. GC/servis
bayrakları (`--gc-threshold`, `--gc-stats`, gelecekteki `--port`/`--host`)
mevcut düz bayrak modelinde kalır — `CliArgs`'a dokunmuyoruz, sadece
`parseArgs`'taki `--allow-fs`/`--allow-net`/`--allow-sys` üç `if`'i
`--allow`'u ayrıştıran tek bir bloğa indirgiyoruz.

Bu, önceki `--with` tasarımına göre **çok daha küçük bir değişiklik** —
tek bir bayrağın (permission) sözdizimini sadeleştiriyor, geri kalan CLI'ye
dokunmuyor. Yeni komut ailesi eklendiğinde (örn. gelecekte bir "resource
limit" kategorisi gerçekten birden fazla komutta tekrarlanan bir yapılandırma
ailesi haline gelirse) o kategori kendi adıyla, kendi bayrağıyla eklenir
(`--allow` deseninin kopyası, ama farklı isim) — tek bir evrensel kap yerine
her kategori kendi adını taşır.
