import sys
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[2]
OUT = REPO / "playground" / "adv-fonts-b" / "crops"


def main():
    img = Image.open(REPO / "playground/adv-fonts-b/shots/solve-review.png").convert("RGB")
    c = img.crop((586, 280, 1000, 440))
    c = c.resize((c.width * 3, c.height * 3), Image.NEAREST)
    c.save(OUT / "solve-review-collision.png")
    print("saved")
    return 0


if __name__ == "__main__":
    sys.exit(main())
