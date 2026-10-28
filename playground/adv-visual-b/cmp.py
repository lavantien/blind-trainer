import hashlib
import json
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[2]
bases = {
    p.name: hashlib.sha256(p.read_bytes()).hexdigest()
    for p in sorted((root / "tests/visual/baselines").glob("*.png"))
}


def hashes(shots_dir):
    d = Path(shots_dir)
    return {
        p.name: hashlib.sha256(p.read_bytes()).hexdigest()
        for p in sorted(d.glob("*.png"))
    }


def compare(label, shots_dir):
    got = hashes(shots_dir)
    verdict = "MATCH" if got == bases else "DIFFER"
    rows = []
    for name in sorted(set(bases) | set(got)):
        if bases.get(name) != got.get(name):
            rows.append(name)
    print(json.dumps({"label": label, "verdict": verdict, "diff": rows}))


if __name__ == "__main__":
    compare(sys.argv[1], sys.argv[2])
