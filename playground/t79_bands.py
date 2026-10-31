import sys
from PIL import Image


def bands(path, x0, x1, y0, y1):
    image = Image.open(path).convert("RGB")
    w, h = image.size
    pixels = image.load()
    out = []
    run = None
    for y in range(y0, min(y1, h)):
        lit = False
        for x in range(x0, min(x1, w)):
            p = pixels[x, y]
            if p[0] != 0 or p[1] != 0 or p[2] != 0:
                lit = True
                break
        if lit and run is None:
            run = y
        if not lit and run is not None:
            out.append((run, y - 1))
            run = None
    if run is not None:
        out.append((run, min(y1, h) - 1))
    return out


def main():
    path = sys.argv[1]
    x0, x1, y0, y1 = 590, 1260, 92, 780
    rows = bands(path, x0, x1, y0, y1)
    print(f"bands in x {x0}-{x1} y {y0}-{y1}")
    for i, (a, b) in enumerate(rows):
        height = b - a + 1
        gap = "" if i == 0 else f" gap={a - rows[i - 1][1] - 1}"
        print(f"  {i}: y {a}-{b} h={height}{gap}")


if __name__ == "__main__":
    main()
