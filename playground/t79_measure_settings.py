import sys
from PIL import Image


def lit_mask(pixels, w, h, threshold):
    mask = []
    for y in range(h):
        row = []
        for x in range(w):
            p = pixels[x, y]
            row.append(1 if (p[0] > threshold and p[1] > threshold and p[2] > threshold) else 0)
        mask.append(row)
    return mask


def bands(mask, w, h):
    rows = []
    run = None
    for y in range(h):
        lit = sum(mask[y])
        if lit > 0:
            if run is None:
                run = [y, y]
            else:
                run[1] = y
        else:
            if run is not None:
                rows.append((run[0], run[1]))
                run = None
    if run is not None:
        rows.append((run[0], run[1]))
    return rows


def main(path, threshold):
    image = Image.open(path).convert("RGB")
    w, h = image.size
    print(f"image {w}x{h} threshold {threshold}")
    pixels = image.load()
    mask = lit_mask(pixels, w, h, threshold)
    for start, end in bands(mask, w, h):
        cols = []
        for x in range(w):
            if any(mask[y][x] for y in range(start, end + 1)):
                cols.append(x)
        left = cols[0] if cols else -1
        right = cols[-1] if cols else -1
        print(f"y {start:4d}-{end:4d} h={end - start + 1:3d} x {left:4d}-{right:4d}")


if __name__ == "__main__":
    main(sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 110)
