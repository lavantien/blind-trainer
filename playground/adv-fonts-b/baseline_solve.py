import sys
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[2]
OUT = REPO / "playground" / "adv-fonts-b" / "crops"


def bands(img, box, min_lit=2, gap_merge=2):
    x0, y0, x1, y1 = box
    crop = img.crop(box)
    p = crop.load()
    w, h = crop.size
    rows = []
    for y in range(h):
        n = 0
        for x in range(w):
            if p[x, y] != (23, 26, 31):
                n += 1
        rows.append(n)
    spans = []
    at = 0
    while at < len(rows):
        if rows[at] >= min_lit:
            s = at
            while at < len(rows) and rows[at] >= min_lit:
                at += 1
            spans.append([s + y0, at - 1 + y0])
        else:
            at += 1
    merged = []
    for sp in spans:
        if merged and sp[0] - merged[-1][1] - 1 <= gap_merge:
            merged[-1][1] = sp[1]
        else:
            merged.append(sp)
    return merged


def main():
    src = REPO / "playground" / "adv-fonts-b" / "e2e-b" / "solve.png"
    img = Image.open(src).convert("RGB")
    merged = bands(img, (586, 90, 1280, 780))
    print("baseline solve rail bands:", merged)
    c = img.crop((586, 85, 1100, 460))
    c = c.resize((c.width * 2, c.height * 2), Image.NEAREST)
    c.save(OUT / "baseline-solve-rail.png")
    print("crop saved")
    return 0


if __name__ == "__main__":
    sys.exit(main())
