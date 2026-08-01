#!/usr/bin/env python3
"""
gen_compile_suite.py — saQut derleme benchmark suite üreticisi

Üretilen yapı:
  scripts/bench/compile_suite/
    module_000.sqt ... module_NNN.sqt   (modüller)
    main_bench.sqt                      (giriş noktası)

Tasarım kararları:
  - Her modül 12 export fonksiyon içerir (int aritmetiği, for döngüsü, if/else).
  - Modül i, modül i-1 ve modül i-4'ten birer fonksiyon import eder (DAG, döngü yok).
  - main_bench.sqt üst seviye modülden birkaç fonksiyon import eder ve çalıştırır.
  - Yalnızca desteklenen özellikler: int, for, if/else, fonksiyon çağrısı.
    (struct/array/float IR eksik olduğu için kullanılmaz.)

Hedef boyut: ~5-10 MB (gerçekçi frontend yükü için).
İleride: fonksiyon gövdelerini büyüterek 100 MB hedefine yaklaşılabilir.

Kullanım:
  python3 gen_compile_suite.py [--modules=N] [--out=dizin]
"""

import os
import sys
import argparse
import textwrap

HEADER = """\
// ÜRETİLMİŞ DOSYA — gen_compile_suite.py tarafından üretildi.
// Derleme hızı benchmark'ı için: saQut parser/tip-denetleyici yükü test eder.
// Bu dosyayı elle düzenleme.
"""

def func_body(mod_idx: int, func_idx: int, use_imports: list[str],
              heavy: int = 0) -> str:
    """
    Her fonksiyon: birkaç yerel int, bir for döngüsü, if/else, sonuçta return.
    İmport edilen fonksiyonlar varsa çağrılır.

    heavy > 0 ise gövdeye `heavy` adet ek ifade bloğu eklenir — dosya başına
    daha fazla AST düğümü/IR talimatı üretir. Kaynak boyutunu değil, DÜĞÜM
    YOĞUNLUĞUNU artırmak için: derleyici bellek profili düğüm sayısına bağlı,
    bayt sayısına değil.
    """
    lines = []
    iters = 8 + (func_idx % 5) * 2          # 8..16 iterasyon (sabit, tahmin edilebilir)
    seed  = (mod_idx * 13 + func_idx * 7)    # fonksiyon-özgü sabit

    lines.append(f"    int acc = {seed % 100};")
    lines.append(f"    int step = {1 + (seed % 4)};")
    lines.append(f"    for (int k = 0; k < {iters}; k = k + step) {{")
    lines.append(f"        acc = acc + k;")
    lines.append(f"        if (acc > {seed % 500 + 50}) {{")
    lines.append(f"            acc = acc - {seed % 30 + 10};")
    lines.append(f"        }}")
    lines.append(f"    }}")

    # heavy: ek ifade blokları — düğüm yoğunluğunu artırır (saf hesaplama)
    for h in range(heavy):
        base = (seed + h * 31) % 97
        lines.append(f"    int h{h}a = {base} + {h + 1};")
        lines.append(f"    int h{h}b = h{h}a * {2 + (h % 3)} - {base % 7};")
        lines.append(f"    int h{h}c = (h{h}a + h{h}b) / {1 + (h % 5)};")
        lines.append(f"    if (h{h}c > {base}) {{")
        lines.append(f"        acc = acc + h{h}c - h{h}b;")
        lines.append(f"    }} else {{")
        lines.append(f"        acc = acc + h{h}a;")
        lines.append(f"    }}")

    # Import edilen fonksiyonları kullan (DAG bağlantısı — sembol tablosunu zorla)
    for imp_fn in use_imports:
        lines.append(f"    int tmp_{imp_fn} = {imp_fn}(acc);")
        lines.append(f"    acc = acc + tmp_{imp_fn};")

    lines.append(f"    return acc;")
    return "\n".join(lines)


def make_module(mod_idx: int, total: int, funcs_per_mod: int,
                reachable: bool = True, heavy: int = 0) -> str:
    parts = [HEADER]

    # Import bildirimleri (DAG: mod i → mod i-1, mod i-4)
    imported_fns: list[str] = []
    deps = []
    if mod_idx >= 1:   deps.append(mod_idx - 1)
    if mod_idx >= 4:   deps.append(mod_idx - 4)

    for dep in deps:
        # Her bağımlı modülden ilk iki fonksiyonu import et
        fn0 = f"fn_{dep:03d}_0"
        fn1 = f"fn_{dep:03d}_1"
        parts.append(f'import {{{fn0}, {fn1}}} from "module_{dep:03d}.sqt";')
        imported_fns += [fn0, fn1]

    if deps:
        parts.append("")

    # Fonksiyon tanımları
    #
    # ÇAĞRI ZİNCİRİ (reachable=True, varsayılan):
    #   Modül içindeki fonksiyonlar ZİNCİRLENİR: fn_i çağırır fn_{i+1}.
    #   Zincirin BAŞI (fn_0) import edilenleri çağırır, SONU (fn_{n-1}) yaprak.
    #   Böylece modülün dışa açık fn_0'ı çağrıldığında modüldeki TÜM
    #   fonksiyonlar gerçekten erişilebilir olur.
    #
    #   Eski davranış (reachable=False) sadece ilk 2 fonksiyonu bağlıyordu;
    #   kalan 10'u tanımlı ama hiçbir çağrı zincirinden erişilemez kalıyordu.
    #   Derleyici onları yine de tam derliyor (ölü kod elemesi yok), ama
    #   "derleyici her yeri geziyor" senaryosu için gerçek zincir gerekir.
    for fi in range(funcs_per_mod):
        fn_name = f"fn_{mod_idx:03d}_{fi}"
        if reachable:
            # Zincirin başı import'ları çağırır; her fonksiyon bir sonrakini çağırır.
            use_imps = list(imported_fns) if fi == 0 else []
            if fi + 1 < funcs_per_mod:
                use_imps = use_imps + [f"fn_{mod_idx:03d}_{fi + 1}"]
        else:
            use_imps = imported_fns if fi < 2 else []
        body = func_body(mod_idx, fi, use_imps, heavy=heavy)
        decl = f"export int {fn_name}(int x) {{\n{body}\n}}"
        parts.append(decl)
        parts.append("")

    return "\n".join(parts)


def make_main(last_mod: int, funcs_per_mod: int, total_mods: int = 0,
              fan_out: int = 0) -> str:
    """
    fan_out > 0 ise main HER modülün zincir başını doğrudan çağırır (en fazla
    fan_out tanesini). Böylece çağrı grafiği main'den başlayarak tüm modüllere
    ulaşır — "derleyici her yeri gezsin" senaryosu.

    fan_out = 0 (eski davranış): yalnızca son modül çağrılır; zincir oradan
    geriye doğru ilerler.
    """
    parts = [HEADER]

    if fan_out > 0 and total_mods > 0:
        step = max(1, total_mods // fan_out)
        targets = list(range(0, total_mods, step))
        for m in targets:
            parts.append(f'import {{fn_{m:03d}_0}} from "module_{m:03d}.sqt";')
        parts.append("")
        parts.append("int main() {")
        parts.append("    int total = 0;")
        for m in targets:
            parts.append(f"    total = total + fn_{m:03d}_0(total);")
        parts.append("    print(total);")
        parts.append("    return 0;")
        parts.append("}")
        return "\n".join(parts)

    # Son modülden birkaç fonksiyon import et
    fn0 = f"fn_{last_mod:03d}_0"
    fn1 = f"fn_{last_mod:03d}_1"
    parts.append(f'import {{{fn0}, {fn1}}} from "module_{last_mod:03d}.sqt";')
    parts.append("")
    parts.append("int main() {")
    parts.append(f"    int a = {fn0}(42);")
    parts.append(f"    int b = {fn1}(a);")
    parts.append("    print(a);")
    parts.append("    print(b);")
    parts.append("    return 0;")
    parts.append("}")
    return "\n".join(parts)


def main():
    ap = argparse.ArgumentParser(description="saQut derleme benchmark suite üreticisi")
    ap.add_argument("--modules",  type=int, default=400,
                    help="Kaç modül üretilsin (varsayılan: 400)")
    ap.add_argument("--funcs",    type=int, default=12,
                    help="Modül başına fonksiyon sayısı (varsayılan: 12)")
    ap.add_argument("--out",      type=str,
                    default=os.path.join(os.path.dirname(__file__), "compile_suite"),
                    help="Çıktı dizini")
    ap.add_argument("--heavy",    type=int, default=0,
                    help="Fonksiyon başına ek ifade bloğu sayısı (düğüm yoğunluğu)")
    ap.add_argument("--fan-out",  type=int, default=0,
                    help="main kaç modülün zincir başını doğrudan çağırsın "
                         "(0 = sadece son modül; >0 = grafiği main'den yay)")
    ap.add_argument("--no-chain", action="store_true",
                    help="Eski davranış: modül içi fonksiyonları zincirleme "
                         "(çoğu fonksiyon erişilemez kalır)")
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)

    total_bytes = 0
    for i in range(args.modules):
        content  = make_module(i, args.modules, args.funcs,
                               reachable=not args.no_chain, heavy=args.heavy)
        fname    = os.path.join(args.out, f"module_{i:03d}.sqt")
        with open(fname, "w", encoding="utf-8") as f:
            f.write(content)
        total_bytes += len(content.encode("utf-8"))

        if (i + 1) % 50 == 0:
            print(f"  {i+1}/{args.modules} modül yazıldı...", file=sys.stderr)

    main_src = make_main(args.modules - 1, args.funcs,
                         total_mods=args.modules, fan_out=args.fan_out)
    main_path = os.path.join(args.out, "main_bench.sqt")
    with open(main_path, "w", encoding="utf-8") as f:
        f.write(main_src)
    total_bytes += len(main_src.encode("utf-8"))

    print(f"\nSuite hazır: {args.out}", file=sys.stderr)
    print(f"  {args.modules} modül + 1 main = {args.modules + 1} dosya", file=sys.stderr)
    print(f"  Toplam kaynak: {total_bytes / 1024:.1f} KB "
          f"({total_bytes / 1024 / 1024:.2f} MB)", file=sys.stderr)
    print(main_path)  # stdout'a giriş dosyası yolu


if __name__ == "__main__":
    main()
