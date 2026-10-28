import sys
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[2]
SHOTS = REPO / "playground" / "adv-fonts-b" / "shots"

BANDS = {
    "count": (90, 102),
    "letters": (123, 147),
    "covered": (182, 194),
    "alg1": (211, 230),
    "alg2": (262, 284),
    "alg3": (313, 332),
    "small1": (379, 386),
    "expand": (415, 433),
    "meta": (466, 488),
    "r1": (512, 524),
    "r2": (536, 548),
    "r3": (560, 571),
}


def main():
    for name in ["algs.png", "algs-search.png"]:
        img = Image.open(SHOTS / name).convert("RGB")
        p = img.load()
        bg = (23, 26, 31)
        print(name)
        for label, (a, b) in BANDS.items():
            right = left = None
            for y in range(a, b + 1):
                for x in range(img.width - 1, 515, -1):
                    if p[x, y] != bg:
                        right = x if right is None else max(right, x)
                        break
                for x in range(516, img.width):
                    if p[x, y] != bg:
                        left = x if left is None else min(left, x)
                        break
            print(f"  {label} y{a}-{b}: left={left} right={right}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
