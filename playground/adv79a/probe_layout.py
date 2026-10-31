import sys
from PIL import Image


def load(path):
    return Image.open(path).convert("RGB")


def lit(p, threshold):
    return p[0] > threshold and p[1] > threshold and p[2] > threshold


def row_bands(img, threshold, x0=0, x1=None, y0=0, y1=None):
    w, h = img.size
    x1 = w if x1 is None else x1
    y1 = h if y1 is None else y1
    pixels = img.load()
    out = []
    run = None
    for y in range(y0, min(y1, h)):
        any_lit = any(lit(pixels[x, y], threshold) for x in range(x0, min(x1, w)))
        if any_lit and run is None:
            run = y
        if not any_lit and run is not None:
            out.append((run, y - 1))
            run = None
    if run is not None:
        out.append((run, min(y1, h) - 1))
    return out


def span_x(img, threshold, y0, y1, x0=0, x1=None):
    w, h = img.size
    x1 = w if x1 is None else x1
    pixels = img.load()
    left = None
    right = None
    for x in range(x0, min(x1, w)):
        col_l = any(lit(pixels[x, y], threshold) for y in range(y0, min(y1, h)))
        if col_l:
            if left is None:
                left = x
            right = x
    return left, right


def main():
    path = sys.argv[1]
    threshold = int(sys.argv[2]) if len(sys.argv) > 2 else 90
    img = load(path)
    w, h = img.size
    print(f"image {w}x{h} threshold {threshold}")
    print("== full width text bands ==")
    bands = row_bands(img, threshold)
    for i, (a, b) in enumerate(bands):
        left, right = span_x(img, threshold, a, b)
        print(f"  {i}: y {a:4d}-{b:4d} h={b - a + 1:3d} x {left:4d}-{right:4d}")
    print("== edge clip check: lit rows/cols within 8 px of each edge ==")
    pixels = img.load()
    for label, coords in (
        ("bottom", [("row", y) for y in range(h - 8, h)]),
        ("top", [("row", y) for y in range(0, 8)]),
        ("right", [("col", x) for x in range(w - 8, w)]),
    ):
        hits = []
        for kind, c in coords:
            if kind == "row":
                if any(lit(pixels[x, c], threshold) for x in range(w)):
                    hits.append(c)
            else:
                if any(lit(pixels[c, y], threshold) for y in range(h)):
                    hits.append(c)
        print(f"  {label}: {hits}")


if __name__ == "__main__":
    main()
