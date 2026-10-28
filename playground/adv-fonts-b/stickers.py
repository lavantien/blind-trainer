import sys
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[2]
R1 = REPO / "playground" / "adv-fonts-b" / "shots"
R2 = REPO / "playground" / "adv-fonts-b" / "shots2"


def sticker_colors(src, spots):
    img = Image.open(src).convert("RGB")
    p = img.load()
    out = []
    for (x0, y0, x1, y1) in spots:
        counts = {}
        for y in range(y0, y1):
            for x in range(x0, x1):
                c = p[x, y]
                counts[c] = counts.get(c, 0) + 1
        best = sorted(counts.items(), key=lambda kv: -kv[1])[:3]
        out.append(best)
    return out


def main():
    spots = [(207, 240, 247, 278), (240, 281, 272, 315), (207, 322, 247, 352)]
    for tag, d in (("run1", R1), ("run2", R2)):
        res = sticker_colors(d / "drill-card.png", spots)
        print(tag)
        for i, spot in enumerate(res):
            print(f"  sticker{i}: {spot}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
