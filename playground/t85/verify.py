import sys
from pathlib import Path

from PIL import Image

GALLERY = Path(__file__).resolve().parents[2] / "docs" / "screenshots"
BASELINES = Path(__file__).resolve().parents[2] / "tests" / "visual" / "baselines"
PAD = 16.8
HEADER = 77.0
CONTENT_X = PAD
CONTENT_Y = PAD + HEADER
CONTENT_W = 1280.0 - 2 * PAD
BOTTOM = 800.0 - PAD


def distinct_colors(image, step=16):
    pixels = image.convert("RGB").load()
    seen = set()
    for y in range(0, image.height, step):
        for x in range(0, image.width, step):
            seen.add(pixels[x, y])
    return len(seen)


def lit_rows(image, box, threshold=90):
    x0, y0, x1, y1 = [int(v) for v in box]
    pixels = image.convert("RGB").load()
    bands = []
    run = None
    for y in range(y0, y1):
        lit = any(pixels[x, y][0] > threshold and pixels[x, y][1] > threshold
                  and pixels[x, y][2] > threshold for x in range(x0, x1))
        if lit and run is None:
            run = [y, y]
        elif lit:
            run[1] = y
        elif run is not None:
            bands.append((run[0], run[1]))
            run = None
    if run is not None:
        bands.append((run[0], run[1]))
    return bands


def gallery_report():
    print("== gallery non-blank check")
    names = sorted(p.name for p in GALLERY.glob("*.png"))
    for name in names:
        image = Image.open(GALLERY / name)
        colors = distinct_colors(image)
        verdict = "ok" if colors >= 4 and image.width == 1280 and image.height == 800 else "BAD"
        print(f"  {name}: {image.width}x{image.height} distinct {colors} {verdict}")
    print(f"  total pngs {len(names)}")


def replay_compare():
    print("== replay gallery vs baseline")
    gallery = Image.open(GALLERY / "replay.png").convert("RGB")
    baseline = Image.open(BASELINES / "replay.png").convert("RGB")
    gp, bp = gallery.load(), baseline.load()
    diff = 0
    for y in range(gallery.height):
        for x in range(gallery.width):
            a, b = gp[x, y], bp[x, y]
            if a != b:
                diff += 1
    faces_g = {gp[x, y] for y in range(0, gallery.height, 8) for x in range(0, gallery.width, 8)}
    faces_b = {bp[x, y] for y in range(0, baseline.height, 8) for x in range(0, baseline.width, 8)}
    print(f"  differing pixels {diff} of {gallery.width * gallery.height}")
    print(f"  gallery sampled colors {len(faces_g)}, baseline sampled colors {len(faces_b)}")


def caption_evidence():
    print("== menu data lines")
    menu = Image.open(GALLERY / "menu.png")
    y = CONTENT_Y
    for i in range(6):
        box = (CONTENT_X, y, 1280 - PAD, y + 36.4 + 4.9 + 26.88 + 4.9 + 25.2)
        bands = lit_rows(menu, box)
        print(f"  row{i} bands {len(bands)}")
        y += 36.4 + 4.9 + 26.88 + 4.9 + 25.2 + 19.6
    print("== settings full-width rows")
    settings = Image.open(GALLERY / "settings.png")
    for i, top in enumerate((284.5, 393.7, 502.9)):
        label = lit_rows(settings, (CONTENT_X, top, 860.0, top + 89.6))
        value = lit_rows(settings, (871.2, top, 1280 - PAD, top + 89.6))
        print(f"  row{i} label bands {len(label)} value bands {len(value)}")
    print("== algs list rows")
    algs = Image.open(GALLERY / "algs.png")
    bands = lit_rows(algs, (CONTENT_X, CONTENT_Y, CONTENT_X + CONTENT_W * 0.4 - 8, BOTTOM))
    print(f"  list bands {len(bands)}")


def main():
    which = sys.argv[1] if len(sys.argv) > 1 else "all"
    if which in ("all", "gallery"):
        gallery_report()
    if which in ("all", "replay"):
        replay_compare()
    if which in ("all", "captions"):
        caption_evidence()


if __name__ == "__main__":
    main()
