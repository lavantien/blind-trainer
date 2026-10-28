import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from verify_shots import load


def bands(path, x0, x1, y0, y1, bg=(23, 26, 31)):
    w, h, px = load(path)
    out = []
    run = None
    lit_total = 0
    for y in range(y0, min(y1, h)):
        lit = 0
        for x in range(x0, min(x1, w)):
            o = (y * w + x) * 4
            if (px[o], px[o + 1], px[o + 2]) != bg:
                lit += 1
        lit_total += lit
        if lit > 0 and run is None:
            run = [y, y, lit]
        elif lit > 0:
            run[1] = y
            run[2] += lit
        elif run is not None:
            out.append(tuple(run))
            run = None
    if run is not None:
        out.append(tuple(run))
    return out, lit_total


def main():
    docs = Path(__file__).resolve().parents[2] / "docs" / "screenshots"
    for name, regions in [
        ("algs-search.png", [(25, 500, 95, 700), (530, 1260, 95, 700)]),
        ("algs.png", [(25, 500, 95, 700)]),
        ("solve-review.png", [(570, 1280, 90, 780)]),
    ]:
        for x0, x1, y0, y1 in regions:
            b, total = bands(docs / name, x0, x1, y0, y1)
            shown = b[:8]
            print(f"{name} ({x0},{y0},{x1},{y1}) lit={total} bands={len(b)} first={shown}")


if __name__ == "__main__":
    main()
