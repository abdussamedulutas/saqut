# Claude Entry Point for saQut

Bu dosya bağımsız bir kural seti değildir. Repository'deki tek genel otorite
`AGENTS.md` dosyasıdır.

Claude ile her yeni oturumda:

1. `AGENTS.md` dosyasını tamamen oku ve uygula.
2. `knowledge-base/` altındaki bütün dosyaları oku.
3. `docs/v1.0-kapsam-bildirgesi.md`,
   `docs/adr/ADR-042-v1-feedback-mvp-ve-surumleme.md`,
   `docs/v0.9-v1.0-yol-haritasi.md` ve
   `docs/v1.0-issue-disposition.md` dosyalarını oku.
4. Kullanıcının verdiği tek rol prompt'unu oku:
   - Ağır mimar: `prompts/heavy-chief-architect.md`
   - Hafif teslimat yöneticisi: `prompts/light-delivery-manager.md`
   - Hafif uygulayıcı: `prompts/light-implementer.md`
   - İzole hafif testçi: `prompts/light-adversarial-tester.md`
5. Rol dışı eylem yapma. Bir oturumda rol değiştirme.

## Değişmez ürün bağlamı

- `0.8.0` yayınlanmış baseline'dır.
- Aktif hedefler `0.9.0` ve `1.0.0`'dır.
- Derleyiciyi stabil hale getirmek tek başına sürüm numarası atlama gerekçesi
  değildir. Yeni milestone, kullanıcı tarafından gözlenen yeni/iyileşmiş ürün
  davranışı ve bunun kanıt programlarıyla gerekçelendirilir.
- `0.9.0` sessiz stabilizasyon release'i değildir; ürün sahibinin dili gerçek
  CLI/veri-işleme akışlarında daha hareketli kullanabildiği preview olmalıdır.
- v1.0 production-ready vaat değil, Feedback MVP'dir.
- VM stabil referanstır; JIT `[EXPERIMENTAL]`dır.
- AOT ve public concurrency v1 dışıdır.
- DoD: `Tasarlandı → Uygulandı → Test Edildi → Release Edildi`.

Claude, issue/ADR/coder raporundaki başarı cümlelerini kanıt olarak kabul etmez.
Ürün sahibinin onayı gereken GC, concurrency, CLI, FFI, platform ve dil
ergonomisi kararlarında seçenekleri sunup durur.

Model adı rolü belirlemez. Ağır veya hafif rol kullanıcı tarafından oturum
başında atanır. Aynı model coder ve tester olacaksa ayrı, context paylaşmayan
oturumlar zorunludur.

`.claude/` altında gizli agent, hook veya yerel override kullanılmaz. Bu dosya
ve `AGENTS.md` ile çelişen geçmiş prompt'lar geçersizdir.
