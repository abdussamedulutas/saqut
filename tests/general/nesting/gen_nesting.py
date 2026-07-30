#!/usr/bin/env python3
"""Üretici: derinlemesine parantezli ifade / iç-içe if / düz (flat) toplama
zinciri fixture'ları üretir. Bu klasördeki checked-in .sqt dosyaları bu
script'in belirli N değerleriyle çalıştırılmış çıktılarıdır (sonuç
tekrarlanabilir olsun diye script de burada tutuluyor).

Kullanım:
    python3 gen_nesting.py expr   <N> <out.sqt>   # (1+(1+(1+...1...)))
    python3 gen_nesting.py ifchain <N> <out.sqt>   # N iç-içe if bloğu
    python3 gen_nesting.py chain  <N> <out.sqt>   # düz N-terimli 1+1+1+...+1

Bulgu (bkz. run_nesting_stress.sh): her üç şekil de N ~ 4000-5000 civarında
SIGSEGV ile çöküyor (recursive-descent parser'ın yığın kullanımı ile ilgili
görünüyor -- tokenizer aynı N'lerde sorunsuz, bkz. run script çıktısı).

NOT: `nest_if_5000_crash.sqt` bu klasörde CHECKED-IN DEĞİL (5000 satırlık
gerçek bir dosya, tek-satır expr/chain fixture'larının aksine boyutu
elverişsiz büyüyor) -- bu script ile yeniden üretin:
    python3 gen_nesting.py ifchain 5000 /tmp/nest_if_5000.sqt
(gözlenen: SIGSEGV, exit=139, aynı N=5000 eşiği)
"""
import sys


def gen_expr(n: int) -> str:
    expr = "1"
    for _ in range(n):
        expr = f"(1+{expr})"
    return f"int main() {{\n    print({expr});\n    return 0;\n}}\n"


def gen_ifchain(n: int) -> str:
    lines = ["int main() {", "    int x = 0;"]
    lines += ["    if (1) {"] * n
    lines.append("    x = 1;")
    lines += ["}"] * n
    lines += ["    print(x);", "    return 0;", "}"]
    return "\n".join(lines) + "\n"


def gen_chain(n: int) -> str:
    return "int main() { int x = " + "+".join(["1"] * n) + "; print(x); return 0; }\n"


if __name__ == "__main__":
    kind, n, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    gens = {"expr": gen_expr, "ifchain": gen_ifchain, "chain": gen_chain}
    with open(out, "w") as f:
        f.write(gens[kind](n))
