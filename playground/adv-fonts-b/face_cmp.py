import sys
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[2]
SHOTS = REPO / "playground" / "adv-fonts-b" / "shots"
DOCS = REPO / "docs" / "screenshots"
OUT = REPO / "playground" / "adv-fonts-b" / "crops"

BOX = (200, 232, 290, 360)


def grab(src):
    return Image.open(src).convert("RGB").crop(BOX)


def main():
    mine = grab(SHOTS / "drill-card.png")
    docs = grab(DOCS / "drill-card.png")
    pm, pd = mine.load(), docs.load()
    tiles = {}
    for y in range(mine.height):
        for x in range(mine.width):
            if pm[x, y] != pd[x, y]:
                tiles.setdefault((y // 12, x // 12), 0)
                tiles[(y // 12, x // 12)] += 1
    print("diff per 12px cell (row,col):count")
    for k in sorted(tiles):
        print(f"  {k}: {tiles[k]}")
    w, h = mine.size
    pair = Image.new("RGB", (w * 2 + 10, h), (255, 0, 0))
    pair.paste(mine.resize((w * 3, h * 3), Image.NEAREST), (0, 0))
    pair.paste(docs.resize((w * 3, h * 3), Image.NEAREST), (w * 3 + 10, 0))
    pair.save(OUT / "drill-face-side-by-side.png")
    print("side by side saved")
    return 0


if __name__ == "__main__":
    sys.exit(main())
