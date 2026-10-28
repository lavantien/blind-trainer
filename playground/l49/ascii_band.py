import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from verify_shots import load


def ascii_region(path, x0, x1, y0, y1, bg=(23, 26, 31), xstep=3, ystep=2):
    w, h, px = load(path)
    for y in range(y0, min(y1, h), ystep):
        row = ""
        for x in range(x0, min(x1, w), xstep):
            o = (y * w + x) * 4
            row += "#" if (px[o], px[o + 1], px[o + 2]) != bg else "."
        print(row)


def main():
    docs = Path(__file__).resolve().parents[2] / "docs" / "screenshots"
    name = sys.argv[1]
    x0, x1, y0, y1 = (int(v) for v in sys.argv[2:6])
    print(f"{name} ({x0},{y0},{x1},{y1})")
    ascii_region(docs / name, x0, x1, y0, y1)


if __name__ == "__main__":
    main()
