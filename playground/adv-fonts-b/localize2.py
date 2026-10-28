import sys
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[2]
R1 = REPO / "playground" / "adv-fonts-b" / "shots"
R2 = REPO / "playground" / "adv-fonts-b" / "shots2"


def diff_map(path_a, path_b):
    a = Image.open(path_a).convert("RGB")
    b = Image.open(path_b).convert("RGB")
    pa, pb = a.load(), b.load()
    w, h = a.size
    total = 0
    minx, maxx, miny, maxy = w, 0, h, 0
    for y in range(h):
        for x in range(w):
            if pa[x, y] != pb[x, y]:
                total += 1
                minx = min(minx, x)
                maxx = max(maxx, x)
                miny = min(miny, y)
                maxy = max(maxy, y)
    return total, (minx, miny, maxx, maxy)


def main():
    for name in ["drill-card.png", "drill-reps.png", "drill-rate-a.png", "drill-rate-b.png",
                 "menu.png", "stats.png", "settings.png"]:
        total, bbox = diff_map(R1 / name, R2 / name)
        print(f"run1 vs run2 {name}: {total} px bbox={bbox}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
