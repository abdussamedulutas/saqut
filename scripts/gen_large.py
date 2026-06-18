"""
saQut büyük kaynak dosyası üretici.
Sözdizimsel ve semantik olarak geçerli, çeşitli yapılar içeren
gerçekçi bir .sqt dosyası üretir.

Hedef: ~1 MB
"""

import sys
import random

random.seed(42)

lines = []

def emit(s=""):
    lines.append(s)

# ── Başlık ────────────────────────────────────────────────────────────────────
emit("// saQut büyük kaynak dosyası — otomatik üretildi (gen_large.py)")
emit("// Amaç: ast / symbols / check komutları için performans ölçümü")
emit()

# ── Struct tanımları ──────────────────────────────────────────────────────────
STRUCT_COUNT = 20
for i in range(STRUCT_COUNT):
    emit(f"struct Vec{i} {{")
    emit(f"    int x;")
    emit(f"    int y;")
    emit(f"    int z;")
    emit(f"    float w;")
    emit(f"}}")
    emit()

# ── Global değişkenler ────────────────────────────────────────────────────────
for i in range(50):
    emit(f"int g_{i} = {i * 3 + 1};")
emit()

# ── Yardımcı: rastgele aritmetik ifade (derinlik sınırlı) ─────────────────────
def expr(depth=0, vars=None):
    if vars is None:
        vars = ["a", "b", "c", "x", "n"]
    if depth >= 3 or random.random() < 0.35:
        choice = random.random()
        if choice < 0.5 and vars:
            return random.choice(vars)
        elif choice < 0.8:
            return str(random.randint(1, 100))
        else:
            return str(random.randint(1, 20))
    ops = ["+", "-", "*"]
    op  = random.choice(ops)
    return f"({expr(depth+1, vars)} {op} {expr(depth+1, vars)})"

def cond(vars=None):
    if vars is None:
        vars = ["a", "b", "n"]
    ops = ["<", ">", "<=", ">=", "==", "!="]
    op  = random.choice(ops)
    lhs = expr(0, vars)
    rhs = expr(0, vars)
    return f"{lhs} {op} {rhs}"

# ── Fonksiyon gövdesi üreteci ─────────────────────────────────────────────────
def gen_body(fn_idx, local_vars, indent=1):
    pad  = "    " * indent
    code = []

    # 2-5 lokal değişken tanımı — sadece declare edilenleri ifadelerde kullan
    n_vars   = random.randint(2, 5)
    declared = []
    for v in local_vars[:n_vars]:
        # İlk değişken sadece sabitle başlatılır (henüz başka değişken yok)
        init = expr(0, declared if declared else ["1"])
        code.append(f"{pad}int {v} = {init};")
        declared.append(v)
    local_vars = declared  # bundan sonra sadece declare edilenleri kullan

    # 1-3 if bloğu
    for _ in range(random.randint(1, 3)):
        c = cond(local_vars)
        then_val = expr(0, local_vars)
        code.append(f"{pad}if ({c}) {{")
        code.append(f"{pad}    {local_vars[0]} = {then_val};")
        if random.random() < 0.4:
            else_val = expr(0, local_vars)
            code.append(f"{pad}}} else {{")
            code.append(f"{pad}    {local_vars[0]} = {else_val};")
        code.append(f"{pad}}}")

    # 0-2 for döngüsü
    for _ in range(random.randint(0, 2)):
        bound = random.randint(3, 15)
        inner = expr(0, local_vars)
        code.append(f"{pad}for (int i = 0; i < {bound}; i = i + 1) {{")
        code.append(f"{pad}    {local_vars[0]} = {local_vars[0]} + {inner};")
        code.append(f"{pad}}}")

    # return
    ret_val = expr(0, local_vars)
    code.append(f"{pad}return {ret_val};")
    return code

# ── Fonksiyon tanımları ───────────────────────────────────────────────────────
# Toplam ~5000 fonksiyon → ~1 MB hedef
FUNC_COUNT = 5000
func_names  = [f"fn_{i:04d}" for i in range(FUNC_COUNT)]
func_params = {}   # fname → n_params (çağrı sırasında eşleştirmek için)

for i, fname in enumerate(func_names):
    # Parametre listesi (0-3 parametre)
    n_params = random.randint(0, 3)
    func_params[fname] = n_params
    params   = [f"int p{j}" for j in range(n_params)]
    param_str = ", ".join(params)

    # Lokal değişken adları
    local_vars = ["a", "b", "c", "d", "e",
                  f"v{i % 10}", f"w{i % 7}"]

    emit(f"int {fname}({param_str}) {{")
    for line in gen_body(i, local_vars):
        emit(line)
    emit("}")
    emit()

# ── Main ──────────────────────────────────────────────────────────────────────
emit("int main() {")
emit("    int result = 0;")
emit()

# Her 50 fonksiyonu bir kere çağır — argüman sayısı imzayla eşleştirildi
for i in range(0, FUNC_COUNT, 50):
    fname    = func_names[i]
    n_params = func_params[fname]
    args     = ", ".join(str(random.randint(1, 20)) for _ in range(n_params))
    emit(f"    result = result + {fname}({args});")

emit()
emit("    print(result);")
emit("    return 0;")
emit("}")

# ── Yaz ───────────────────────────────────────────────────────────────────────
output = "\n".join(lines) + "\n"
path   = sys.argv[1] if len(sys.argv) > 1 else "examples/large.sqt"

with open(path, "w", encoding="utf-8") as f:
    f.write(output)

size_kb = len(output.encode("utf-8")) / 1024
print(f"Üretildi: {path}  ({size_kb:.1f} KB,  {len(lines)} satır)")
