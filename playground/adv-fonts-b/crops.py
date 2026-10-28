import sys
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[2]
SHOTS = REPO / "playground" / "adv-fonts-b" / "shots"
DOCS = REPO / "docs" / "screenshots"
OUT = REPO / "playground" / "adv-fonts-b" / "crops"
OUT.mkdir(exist_ok=True)


def crop(src, name, box, scale=2):
    img = Image.open(src).convert("RGB")
    c = img.crop(box)
    if scale != 1:
        c = c.resize((c.width * scale, c.height * scale), Image.NEAREST)
    c.save(OUT / name)
    print(name, box)


def main():
    crop(SHOTS / "drill-card.png", "drill-card-mine-alg.png", (150, 220, 340, 370))
    crop(DOCS / "drill-card.png", "drill-card-docs-alg.png", (150, 220, 340, 370))
    crop(SHOTS / "algs.png", "algs-rows.png", (20, 90, 700, 740))
    crop(SHOTS / "algs-search.png", "algs-search-rows.png", (20, 90, 700, 740))
    crop(SHOTS / "algs.png", "algs-detail.png", (500, 90, 1280, 640))
    crop(SHOTS / "algs-search.png", "algs-search-detail.png", (500, 90, 1280, 640))
    crop(SHOTS / "solve-ready.png", "solve-ready-rail.png", (586, 85, 1280, 460))
    crop(SHOTS / "solve-memo.png", "solve-memo-rail.png", (586, 85, 1280, 460))
    crop(SHOTS / "solve-exec.png", "solve-exec-rail.png", (586, 85, 1280, 460))
    crop(SHOTS / "solve-review.png", "solve-review-rail.png", (586, 85, 1280, 460))
    crop(SHOTS / "case-drill-show.png", "case-drill-show-text.png", (20, 90, 900, 240))
    crop(SHOTS / "case-drill-grade.png", "case-drill-grade-text.png", (20, 90, 900, 240))
    crop(SHOTS / "menu.png", "menu-body.png", (20, 240, 700, 640))
    crop(SHOTS / "settings.png", "settings-body.png", (20, 380, 900, 500))
    crop(SHOTS / "fatal.png", "fatal-full.png", (0, 30, 1280, 470))
    return 0


if __name__ == "__main__":
    sys.exit(main())
