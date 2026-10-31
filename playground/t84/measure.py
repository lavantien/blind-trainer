import sys
from pathlib import Path

from PIL import Image

SHOTS = Path(__file__).resolve().parent / "shots"
PAD = 16.8
HEADER = 77.0
CONTENT_X = PAD
CONTENT_Y = PAD + HEADER
CONTENT_W = 1280.0 - 2 * PAD
CONTENT_H = 800.0 - 2 * PAD - HEADER
BOTTOM = CONTENT_Y + CONTENT_H
SMALL = 26.88
MICRO = 25.2
BODY = 28.0


def load(name):
    return Image.open(SHOTS / name).convert("RGB")


def lit_rows(image, box, threshold=90):
    x0, y0, x1, y1 = [int(v) for v in box]
    pixels = image.load()
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


def heights(bands):
    return [b[1] - b[0] + 1 for b in bands]


def col_extents(image, y0, y1, threshold=90):
    pixels = image.load()
    xs = []
    for x in range(int(CONTENT_X), 1280):
        if any(pixels[x, y][0] > threshold and pixels[x, y][1] > threshold
               and pixels[x, y][2] > threshold for y in range(int(y0), int(y1))):
            xs.append(x)
    return (min(xs), max(xs)) if xs else (None, None)


def settings_report():
    image = load("settings.png")
    row_tops = [284.5, 393.7, 502.9]
    print("== settings.png")
    for i, top in enumerate(row_tops):
        hint_box = (CONTENT_X + 9.8, top + 9.8 + BODY + 4.9, 860.0, top + 89.6)
        bands = lit_rows(image, hint_box)
        print(f"  row{i} hint bands {bands} heights {heights(bands)}")
        label_box = (CONTENT_X, top, 860.0, top + 9.8 + BODY + 4.9)
        bands = lit_rows(image, label_box)
        print(f"  row{i} label bands {bands} heights {heights(bands)}")
    control_x0 = 871.2
    for i, top in enumerate(row_tops):
        value_box = (control_x0 + 72.8 + 2, top + 8.4 + 4, 1263.2 - 72.8 - 2, top + 8.4 + 72.8 - 4)
        bands = lit_rows(image, value_box)
        print(f"  row{i} value bands {bands} heights {heights(bands)}")
    header_bands = lit_rows(image, (0, 0, 700, int(HEADER)))
    print(f"  header title bands {header_bands} heights {heights(header_bands)}")
    ctx_bands = lit_rows(image, (700, 0, 1280, int(HEADER)))
    print(f"  header context bands {ctx_bands} heights {heights(ctx_bands)}")


def menu_report():
    image = load("menu.png")
    print("== menu.png")
    y = CONTENT_Y
    for i in range(6):
        box = (CONTENT_X, y, 1280 - PAD, y + 36.4 + 4.9 + SMALL + 4.9 + MICRO)
        bands = lit_rows(image, box)
        print(f"  menu row{i} bands {bands} heights {heights(bands)}")
        y += 36.4 + 4.9 + SMALL + 4.9 + MICRO + 19.6
    lo, hi = col_extents(image, CONTENT_Y, BOTTOM)
    print(f"  content column extents x {lo}..{hi}")


def algs_report():
    image = load("algs.png")
    print("== algs.png")
    box = (CONTENT_X, CONTENT_Y, CONTENT_X + CONTENT_W * 0.4 - 8, BOTTOM)
    bands = lit_rows(image, box)
    print(f"  list bands {len(bands)} heights {heights(bands)}")
    pitches = [bands[i + 1][0] - bands[i][0] for i in range(len(bands) - 1)]
    print(f"  list pitches {pitches}")
    detail_box = (CONTENT_X + CONTENT_W * 0.4, CONTENT_Y, 1280 - PAD, BOTTOM)
    dbands = lit_rows(image, detail_box)
    print(f"  detail bands {len(dbands)} last {dbands[-1] if dbands else None}")
    print(f"  detail bottom-most lit row {dbands[-1][1] if dbands else None} of {BOTTOM}")


def solve_review_report():
    image = load("solve-review.png")
    print("== solve-review.png")
    box = (CONTENT_X + CONTENT_W * 0.44 + 19.6 + 4, 500, 1280 - PAD, 795)
    bands = lit_rows(image, box)
    print(f"  column bands {bands}")
    print(f"  heights {heights(bands)}")
    if len(bands) >= 2:
        print(f"  last band top {bands[-1][0]} bottom {bands[-1][1]}")


def stats_report():
    image = load("stats.png")
    print("== stats.png")
    bands = lit_rows(image, (CONTENT_X, CONTENT_Y, 1280 - PAD, BOTTOM))
    print(f"  bands {len(bands)} heights {heights(bands)}")
    lo, hi = col_extents(image, CONTENT_Y, BOTTOM)
    print(f"  content column extents x {lo}..{hi}")


def memo_report():
    image = load("memo-stream.png")
    print("== memo-stream.png")
    bands = lit_rows(image, (CONTENT_X, CONTENT_Y, 1280 - PAD, BOTTOM))
    print(f"  bands {bands}")
    print(f"  heights {heights(bands)}")
    lo, hi = col_extents(image, CONTENT_Y, BOTTOM)
    print(f"  content column extents x {lo}..{hi}")


def drill_report():
    for name in ("drill-card.png", "drill-reps.png"):
        image = load(name)
        print(f"== {name}")
        card_x = CONTENT_X + CONTENT_W * 0.44 + 9.8
        bands = lit_rows(image, (card_x + 2, CONTENT_Y, 1280 - PAD, BOTTOM))
        print(f"  card bands {bands} heights {heights(bands)}")
        print(f"  first band top {bands[0][0] if bands else None} vs content top {CONTENT_Y}")


def worst_subtext():
    worst = []
    print("== worst subtext band across views")
    checks = [
        ("settings.png", (CONTENT_X, 294.0, 860.0, 380.0), "settings hint row0"),
        ("settings.png", (CONTENT_X, 403.0, 860.0, 490.0), "settings hint row1"),
        ("settings.png", (CONTENT_X, 512.0, 860.0, 600.0), "settings hint row2"),
        ("menu.png", (CONTENT_X, 135.0, 1280 - PAD, 200.0), "menu desc+data row0"),
        ("solve-ready.png", (CONTENT_X + CONTENT_W * 0.44 + 23, CONTENT_Y, 1280 - PAD, 200.0), "solve phase"),
        ("algs.png", (CONTENT_X + CONTENT_W * 0.4, CONTENT_Y, 1280 - PAD, 130.0), "algs detail head"),
        ("stats.png", (CONTENT_X, 620.0, 1280 - PAD, 780.0), "stats labels row"),
    ]
    for name, box, label in checks:
        bands = lit_rows(load(name), box)
        hs = heights(bands)
        if hs:
            worst.append((min(hs), label, bands))
            print(f"  {label}: min band {min(hs)} bands {bands}")
        else:
            print(f"  {label}: no bands in {box}")
    if worst:
        worst.sort()
        print(f"  worst overall: {worst[0][1]} at {worst[0][0]} px")


def main():
    which = sys.argv[1] if len(sys.argv) > 1 else "all"
    if which in ("all", "settings"):
        settings_report()
    if which in ("all", "menu"):
        menu_report()
    if which in ("all", "algs"):
        algs_report()
    if which in ("all", "solve"):
        solve_review_report()
    if which in ("all", "stats"):
        stats_report()
    if which in ("all", "memo"):
        memo_report()
    if which in ("all", "drill"):
        drill_report()
    if which in ("all", "worst"):
        worst_subtext()


if __name__ == "__main__":
    main()
