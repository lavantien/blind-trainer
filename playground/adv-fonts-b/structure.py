import subprocess
import sys
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[2]
OLD = REPO / "playground" / "adv-fonts-b" / "old"
OLD.mkdir(exist_ok=True)
NEW = REPO / "playground" / "adv-fonts-b" / "shots"


def fetch(name):
    p = OLD / name
    if not p.is_file():
        blob = subprocess.run(
            ["git", "-C", str(REPO), "show", f"f53951f:docs/screenshots/{name}"],
            capture_output=True, check=True).stdout
        p.write_bytes(blob)
    return p


def diff(a_path, b_path, box):
    a = Image.open(a_path).convert("RGB").crop(box)
    b = Image.open(b_path).convert("RGB").crop(box)
    pa, pb = a.load(), b.load()
    w, h = a.size
    n = 0
    for y in range(h):
        for x in range(w):
            if pa[x, y] != pb[x, y]:
                n += 1
    return n, w * h


def main():
    checks = [
        ("solve-ready.png", (20, 90, 565, 780), "solve net region"),
        ("solve-review.png", (20, 90, 565, 780), "solve net region"),
        ("algs.png", (20, 90, 510, 780), "algs rows region"),
        ("case-drill-show.png", (400, 90, 1270, 780), "case drill grid region"),
    ]
    for name, box, label in checks:
        old = fetch(name)
        n, total = diff(old, NEW / name, box)
        print(f"{name} {label} {box}: {n} of {total} px differ old(hollow) vs new")
    return 0


if __name__ == "__main__":
    sys.exit(main())
