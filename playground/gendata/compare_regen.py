import hashlib
import json
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[2]
golden_dir = root / "data/vendor"
regen_dir = root / "playground/gendata/regen"


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compare(name, skip_fields=()):
    golden = golden_dir / name
    regen = regen_dir / name
    if not regen.exists():
        return f"{name}: MISSING in regen"
    if not skip_fields:
        if golden.read_bytes() == regen.read_bytes():
            return f"{name}: byte-identical ({regen.stat().st_size} bytes, sha256 {sha256(regen)})"
        return f"{name}: BYTE MISMATCH golden {golden.stat().st_size} vs regen {regen.stat().st_size}"
    left = json.loads(golden.read_text(encoding="utf-8"))
    right = json.loads(regen.read_text(encoding="utf-8"))
    diffs = []
    for key in sorted(set(left) | set(right)):
        if key in skip_fields:
            continue
        if left.get(key) != right.get(key):
            diffs.append(key)
    skipped = ", ".join(f"{f}: golden {left.get(f)} regen {right.get(f)}" for f in skip_fields)
    if diffs:
        return f"{name}: FIELD MISMATCH {diffs} ({skipped})"
    return f"{name}: all fields match ({skipped})"


for line in (
    compare("cases.json"),
    compare("specials.json"),
    compare("manifest.json", skip_fields=("generated_at",)),
):
    print(line)
    if "MISMATCH" in line or "MISSING" in line:
        sys.exit(1)
