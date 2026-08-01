#!/usr/bin/env python3
"""
embed_internal.py — src/internal/*.sqt dosyalarını C++ başlığına gömer.

Node.js'in js2c.py'siyle aynı işi yapar: gerçek kaynak dosyaları derleme
sırasında binary'ye gömülü veriye çevirir. Kimse elle gömmez.

    src/internal/date.sqt
          ↓  (bu script, cmake tarafından çağrılır)
    build/generated/internal_sources.hpp
          ↓
    ModuleLoader::overlay_ → diskten değil bellekten okur

Kullanım:
    embed_internal.py <kaynak-dizini> <çıktı-başlığı>

Üretilen başlık git'e girmez; her derlemede yeniden üretilir.
"""

import os
import sys


def cpp_raw_string(text: str, name: str) -> str:
    """
    Kaynağı ham string literal olarak sar.

    Ayraç dosya adından türetilir ki içerikte geçme olasılığı olmasın.
    İçerikte ayraç yine de geçerse (teorik) hata veririz — sessizce bozuk
    C++ üretmek yerine.
    """
    delim = "SQT_" + "".join(c if c.isalnum() else "_" for c in name).upper()
    if f'){delim}"' in text:
        raise SystemExit(f"HATA: {name} içeriği ayraç ){delim}\" barındırıyor")
    return f'R"{delim}(\n{text}\n){delim}"'


def main():
    if len(sys.argv) != 3:
        raise SystemExit("kullanım: embed_internal.py <kaynak-dizini> <çıktı-başlığı>")

    src_dir, out_path = sys.argv[1], sys.argv[2]

    # Sıralı gez: üretilen çıktı deterministik olsun (aynı girdi → aynı bayt).
    entries = []
    for root, _dirs, files in os.walk(src_dir):
        for fn in sorted(files):
            if not fn.endswith(".sqt"):
                continue
            full = os.path.join(root, fn)
            # Modül adı kaynak dizinine göreli yoldur: alt dizinler korunur,
            # böylece gömülü modüller birbirini göreli import edebilir.
            rel = os.path.relpath(full, src_dir).replace(os.sep, "/")
            with open(full, encoding="utf-8") as f:
                entries.append((rel, f.read()))
    entries.sort()

    parts = ['''// ============================================================================
// ÜRETİLMİŞ DOSYA — elle düzenlemeyin.
// Kaynak: src/internal/*.sqt   Üretici: scripts/embed_internal.py
//
// saQut ile yazılmış çekirdek modüller, derleyici binary'sine gömülü hâlde.
// Çalışma zamanında dosya sisteminden OKUNMAZ (ModuleLoader::overlay_).
// ============================================================================

#ifndef SAQUT_INTERNAL_SOURCES
#define SAQUT_INTERNAL_SOURCES

#include <string>
#include <unordered_map>

// Gömülü modüller: "date.sqt" → kaynak metni.
// Anahtar, src/internal/ altındaki göreli yoldur.
inline const std::unordered_map<std::string, const char*>& internalSources() {
    static const std::unordered_map<std::string, const char*> sources = {''']

    for rel, text in entries:
        parts.append(f'        {{ "{rel}",\n          {cpp_raw_string(text, rel)} }},')

    parts.append('''    };
    return sources;
}

// Gömülü modül sayısı (test ve tanılama için).
inline int internalSourceCount() { return (int)internalSources().size(); }

#endif // SAQUT_INTERNAL_SOURCES
''')

    out = "\n".join(parts) + "\n"

    # Değişmediyse dosyaya DOKUNMA: aksi halde her cmake çalışmasında
    # tüm bağımlılar yeniden derlenir.
    if os.path.exists(out_path):
        with open(out_path, encoding="utf-8") as f:
            if f.read() == out:
                print(f"embed_internal: {len(entries)} modül (değişmedi)", file=sys.stderr)
                return

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(out)
    print(f"embed_internal: {len(entries)} modül gömüldü → {out_path}", file=sys.stderr)


if __name__ == "__main__":
    main()
