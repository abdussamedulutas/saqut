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

def func_body(mod_idx: int, func_idx: int, use_imports: list[str]) -> str:
    """
    Her fonksiyon: birkaç yerel int, bir for döngüsü, if/else, sonuçta return.
    İmport edilen fonksiyonlar varsa çağrılır.
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

    # Import edilen fonksiyonları kullan (DAG bağlantısı — sembol tablosunu zorla)
    for imp_fn in use_imports:
        lines.append(f"    int tmp_{imp_fn} = {imp_fn}(acc);")
        lines.append(f"    acc = acc + tmp_{imp_fn};")

    lines.append(f"    return acc;")
    return "\n".join(lines)


def make_module(mod_idx: int, total: int, funcs_per_mod: int) -> str:
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
    for fi in range(funcs_per_mod):
        fn_name   = f"fn_{mod_idx:03d}_{fi}"
        # Sadece ilk iki fonksiyon import edilenleri çağırır (bağ kurulsun)
        use_imps = imported_fns if fi < 2 else []
        body      = func_body(mod_idx, fi, use_imps)
        decl      = f"export int {fn_name}(int x) {{\n{body}\n}}"
        parts.append(decl)
        parts.append("")

    return "\n".join(parts)


def make_main(last_mod: int, funcs_per_mod: int) -> str:
    parts = [HEADER]

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
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)

    total_bytes = 0
    for i in range(args.modules):
        content  = make_module(i, args.modules, args.funcs)
        fname    = os.path.join(args.out, f"module_{i:03d}.sqt")
        with open(fname, "w", encoding="utf-8") as f:
            f.write(content)
        total_bytes += len(content.encode("utf-8"))

        if (i + 1) % 50 == 0:
            print(f"  {i+1}/{args.modules} modül yazıldı...", file=sys.stderr)

    main_src = make_main(args.modules - 1, args.funcs)
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
