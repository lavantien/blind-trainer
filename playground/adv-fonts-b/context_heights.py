import sys
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[2]
SHOTS = REPO / "playground" / "adv-fonts-b" / "shots"


def band_height(img, box):
    x0, y0, x1, y1 = box
    crop = img.crop(box)
    p = crop.load()
    w, h = crop.size
    bg = (23, 26, 31)
    rows = []
    for y in range(h):
        n = 0
        for x in range(w):
            if p[x, y] != bg:
                n += 1
        rows.append(n)
    spans = []
    at = 0
    while at < len(rows):
        if rows[at] >= 1:
            s = at
            while at < len(rows) and rows[at] >= 1:
                at += 1
            spans.append((s + y0, at - 1 + y0))
        else:
            at += 1
    return spans


def main():
    box = (640, 40, 1275, 75)
    for name in sorted(SHOTS.glob("*.png")):
        img = Image.open(name).convert("RGB")
        spans = band_height(img, box)
        print(f"{name.name}: context band {spans}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
