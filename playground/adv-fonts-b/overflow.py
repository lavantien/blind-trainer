import sys
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[2]
SHOTS = REPO / "playground" / "adv-fonts-b" / "shots"


def rightmost(img, y0, y1, x0=516):
    p = img.load()
    bg = (23, 26, 31)
    hits = []
    for y in range(y0, y1):
        for x in range(img.width - 1, x0, -1):
            if p[x, y] != bg:
                hits.append(x)
                break
    return max(hits) if hits else None


def main():
    for name in ["algs.png", "algs-search.png"]:
        img = Image.open(SHOTS / name).convert("RGB")
        print(name, "window width", img.width)
        for label, (a, b) in {
            "detail line1": (88, 110),
            "detail title": (120, 150),
            "expand line": (410, 436),
            "meta line": (462, 492),
            "recon line": (512, 528),
        }.items():
            print(f"  {label} rows {a}-{b}: rightmost lit x = {rightmost(img, a, b)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
