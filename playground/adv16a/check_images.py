import re
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[2]

rust = (root / "crates/bt-app/src/views/images.rs").read_text(encoding="utf-8")
body = rust.split("static PAIR_IMAGES", 1)[1]
body = body.split("];", 1)[0]
rust_pairs = re.findall(r'\(\s*"([^"]+)"\s*,\s*"((?:[^"\\]|\\.)*)"\s*,?\s*\)', body)

chead = (root / "src/app/views/images_table.h").read_text(encoding="utf-8")
c_pairs = re.findall(r'\{\s*"([^"]+)"\s*,\s*"((?:[^"\\]|\\.)*)"\s*\}', chead)

print(f"rust={len(rust_pairs)} c={len(c_pairs)}")
ok = True
if len(rust_pairs) != len(c_pairs):
    ok = False
    print("COUNT MISMATCH")
    rk = [k for k, _ in rust_pairs]
    ck = [k for k, _ in c_pairs]
    only_rust = [k for k in rk if k not in ck]
    only_c = [k for k in ck if k not in rk]
    print(f"only rust keys: {only_rust[:10]}")
    print(f"only c keys: {only_c[:10]}")
for i, ((rk, rv), (ck, cv)) in enumerate(zip(rust_pairs, c_pairs)):
    if rk != ck or rv != cv:
        ok = False
        print(f"ROW {i}: rust ({rk!r}, {rv!r}) vs c ({ck!r}, {cv!r})")
        if i > 40:
            break
keys = [k for k, _ in c_pairs]
if keys != sorted(keys):
    ok = False
    print("C TABLE NOT SORTED")
if len(set(keys)) != len(keys):
    ok = False
    dupes = [k for k in keys if keys.count(k) > 1]
    print(f"C DUPLICATE KEYS: {sorted(set(dupes))}")
words = [v for _, v in c_pairs]
if any(not w for w in words):
    ok = False
    print("C HAS EMPTY WORD")
sys.exit(0 if ok else 1)
