#!/usr/bin/env python3
import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile
import textwrap
import time
from pathlib import Path


SEED_ALPHABET = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
TEMP_PREFIX = "saqut-value-string-pressure-"


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def make_seed(size_kb: int) -> str:
    target = max(1, size_kb) * 1024
    repeats = (target // len(SEED_ALPHABET)) + 1
    return (SEED_ALPHABET * repeats)[:target]


def write_program(path: Path, size_kb: int) -> None:
    seed = make_seed(size_kb)
    rounds = max(16, min(96, size_kb // 2 + 16))
    source = f'''
string churn(string seed, int rounds) {{
    string acc = seed;
    int i = 0;
    while (i < rounds) {{
        string left = acc.substring(0, 64);
        string right = acc.substring(acc.length() - 64, 64);
        acc = left + "::" + acc + "::" + right;
        if (acc.length() > 4096) {{
            acc = acc.substring(17, 2048);
        }}
        i = i + 1;
    }}
    return acc;
}}

int main() {{
    string result = churn("{seed}", {rounds});
    print(result.length());
    print(":");
    print(result.substring(0, 16));
    print(":");
    print(result.substring(result.length() - 16, 16));
    return 0;
}}
'''
    path.write_text(textwrap.dedent(source).lstrip(), encoding="utf-8")


def parse_ms(stderr: str, stage: str) -> str:
    match = re.search(rf"{re.escape(stage)}\s+(?:\d+\s+\w+\s+)?([0-9]+(?:\.[0-9]+)?)\s*ms", stderr)
    return match.group(1) if match else "unavailable"


def sizeof_value(root: Path) -> str:
    compiler = shutil.which("g++") or shutil.which("c++")
    if not compiler:
        return "unavailable:no-cxx"
    with tempfile.TemporaryDirectory(prefix=TEMP_PREFIX + "sizeof-") as td:
        src = Path(td) / "sizeof_value.cpp"
        exe = Path(td) / "sizeof_value"
        src.write_text(
            '#include <iostream>\n#include "vm/value.hpp"\n'
            'int main() { std::cout << sizeof(Value) << "\\n"; }\n',
            encoding="utf-8",
        )
        proc = subprocess.run(
            [compiler, "-std=c++20", f"-I{root / 'src'}", str(src), "-o", str(exe)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        if proc.returncode != 0:
            return "unavailable:cxx-failed"
        out = subprocess.run([str(exe)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if out.returncode != 0:
            return "unavailable:run-failed"
        return out.stdout.strip()


def run_once(binary: Path, program: Path) -> dict:
    start = time.perf_counter()
    proc = subprocess.run(
        [str(binary), "run", "--profile", f"file:{program}"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=False,
    )
    elapsed_ms = (time.perf_counter() - start) * 1000.0
    stderr_text = proc.stderr.decode("utf-8", errors="replace")
    return {
        "exit": proc.returncode,
        "stdout_sha256": hashlib.sha256(proc.stdout).hexdigest(),
        "stdout_bytes": len(proc.stdout),
        "stderr_bytes": len(proc.stderr),
        "elapsed_ms": f"{elapsed_ms:.3f}",
        "vm_exec_ms": parse_ms(stderr_text, "vm-exec"),
        "total_ms": parse_ms(stderr_text, "toplam"),
    }


def residue_count() -> int:
    tmp = Path(tempfile.gettempdir())
    return sum(1 for p in tmp.iterdir() if p.name.startswith(TEMP_PREFIX))


def measure(args: argparse.Namespace) -> int:
    binary = Path(args.binary).resolve()
    if not binary.exists():
        print(f"error=binary-not-found path={binary}", file=sys.stderr)
        return 2

    root = repo_root()
    before_residue = residue_count()
    with tempfile.TemporaryDirectory(prefix=TEMP_PREFIX) as td:
        program = Path(td) / "value_string_pressure.sqt"
        write_program(program, args.size_kb)
        source_sha = hashlib.sha256(program.read_bytes()).hexdigest()

        print(f"binary={binary}")
        print(f"source_sha256={source_sha}")
        print(f"size_kb={args.size_kb}")
        print(f"repeat={args.repeat}")
        print(f"sizeof_value={sizeof_value(root)}")

        ok = True
        for i in range(args.repeat):
            result = run_once(binary, program)
            ok = ok and result["exit"] == 0
            fields = " ".join(f"{k}={v}" for k, v in result.items())
            print(f"run={i + 1} {fields}")

    after_residue = residue_count()
    print(f"temp_residue_before={before_residue}")
    print(f"temp_residue_after={after_residue}")
    return 0 if ok and after_residue <= before_residue else 1


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", required=True)
    parser.add_argument("--mode", choices=["measure"], default="measure")
    parser.add_argument("--size-kb", type=int, default=128)
    parser.add_argument("--repeat", type=int, default=2)
    args = parser.parse_args()

    if args.repeat < 1:
        print("error=repeat-must-be-positive", file=sys.stderr)
        return 2
    if args.size_kb < 1:
        print("error=size-kb-must-be-positive", file=sys.stderr)
        return 2
    return measure(args)


if __name__ == "__main__":
    raise SystemExit(main())
