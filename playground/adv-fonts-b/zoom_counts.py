import sys
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[2]
SHOTS = REPO / "playground" / "adv-fonts-b" / "shots"
OUT = REPO / "playground" / "adv-fonts-b" / "crops"


def zoom(src, name, box, scale=4):
    img = Image.open(src).convert("RGB")
    c = img.crop(box)
    c = c.resize((c.width * scale, c.height * scale), Image.NEAREST)
    c.save(OUT / name)
    print(name)


def main():
    zoom(SHOTS / "algs.png", "algs-count-line.png", (516, 88, 1060, 110))
    zoom(SHOTS / "algs-search.png", "algs-search-count-line.png", (516, 88, 1060, 110))
    zoom(SHOTS / "algs-search.png", "algs-search-top-rows.png", (20, 88, 400, 200))
    zoom(SHOTS / "algs.png", "algs-top-rows.png", (20, 88, 400, 200))
    return 0


if __name__ == "__main__":
    sys.exit(main())
