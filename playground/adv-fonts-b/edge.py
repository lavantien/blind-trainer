import sys
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[2]
SHOTS = REPO / "playground" / "adv-fonts-b" / "shots"
NAMES = ["solve-ready.png", "solve-memo.png", "solve-exec.png", "solve-review.png",
         "algs.png", "algs-search.png", "case-drill-show.png", "case-drill-grade.png"]


def main():
    for name in NAMES:
        img = Image.open(SHOTS / name).convert("RGB")
        p = img.load()
        bg = (23, 26, 31)
        edge_rows = []
        for y in range(img.height):
            if p[1278, y] != bg or p[1279, y] != bg:
                edge_rows.append(y)
        print(f"{name}: rows touching right edge: {edge_rows[:12]}{'...' if len(edge_rows) > 12 else ''} count={len(edge_rows)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
