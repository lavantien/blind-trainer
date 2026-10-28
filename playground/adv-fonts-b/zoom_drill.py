import sys
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[2]
SHOTS = REPO / "playground" / "adv-fonts-b" / "shots"
DOCS = REPO / "docs" / "screenshots"
OUT = REPO / "playground" / "adv-fonts-b" / "crops"


def grid(src, tag):
    img = Image.open(src).convert("RGB")
    for r, y0 in enumerate((238, 280, 321)):
        for c, x0 in enumerate((205, 240, 278)):
            if x0 + 38 > img.width:
                continue
            tile = img.crop((x0 - 2, y0 - 2, x0 + 36, y0 + 34))
            tile = tile.resize((tile.width * 6, tile.height * 6), Image.NEAREST)
            tile.save(OUT / f"tile-{tag}-{r}-{c}.png")


def main():
    grid(SHOTS / "drill-card.png", "mine")
    grid(DOCS / "drill-card.png", "docs")
    print("tiles written")
    return 0


if __name__ == "__main__":
    sys.exit(main())
