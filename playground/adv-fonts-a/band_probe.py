import os
import sys
from pathlib import Path
from PIL import Image

TEMP = Path(os.environ.get("TEMP", "."))
BASE = TEMP / "bt-app-view_draw_b-tests"


def main():
    latest = {}
    for root, _, files in os.walk(BASE):
        for name in files:
            if name.endswith(".png"):
                p = Path(root) / name
                if name not in latest or p.stat().st_mtime > latest[name].stat().st_mtime:
                    latest[name] = p
    for name in ("solve.png", "algs.png", "case_drill.png"):
        path = latest[name]
        img = Image.open(path).convert("RGB")
        px = img.load()
        print(f"{name} {path.parent.name} {img.width}x{img.height}")
        regions = {
            "solve.png": [(304, 90, 630, 460)],
            "algs.png": [(20, 90, 250, 460), (260, 90, 630, 460)],
            "case_drill.png": [(20, 90, 430, 200)],
        }
        for x0, y0, x1, y1 in regions[name]:
            spans = []
            start = None
            for y in range(y0, y1):
                lit = sum(1 for x in range(x0, min(x1, img.width)) if px[x, y] != (0, 0, 0))
                if lit and start is None:
                    start = y
                if not lit and start is not None:
                    spans.append((start, y - 1))
                    start = None
            if start is not None:
                spans.append((start, y1 - 1))
            for y0s, y1s in spans:
                total = sum(
                    1
                    for y in range(y0s, y1s + 1)
                    for x in range(x0, min(x1, img.width))
                    if px[x, y] != (0, 0, 0)
                )
                print(f"  band y {y0s}..{y1s} lit={total}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
