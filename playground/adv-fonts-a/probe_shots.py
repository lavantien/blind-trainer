import subprocess
import sys
from pathlib import Path
from PIL import Image

REPO = Path(__file__).resolve().parents[2]
OLD_COMMIT = "14415ab"
OLD_DIR = REPO / "playground/adv-fonts-a/old"
BG = (23, 26, 31)


def old_shot(name):
    OLD_DIR.mkdir(parents=True, exist_ok=True)
    target = OLD_DIR / name
    if not target.is_file():
        blob = subprocess.run(
            ["git", "-C", str(REPO), "show", f"{OLD_COMMIT}:docs/screenshots/{name}"],
            capture_output=True, check=True,
        ).stdout
        target.write_bytes(blob)
    return target


def lit(path, x0, y0, x1, y1):
    img = Image.open(path).convert("RGB")
    n = 0
    px = img.load()
    for y in range(y0, min(y1, img.height)):
        for x in range(x0, min(x1, img.width)):
            if px[x, y] != BG:
                n += 1
    return n, img


def row_ink(img, x0, x1, y):
    px = img.load()
    n = 0
    for x in range(x0, min(x1, img.width)):
        if px[x, y] != BG:
            n += 1
    return n


def bands(img, x0, y0, x1, y1, min_ink=1):
    spans = []
    start = None
    for y in range(y0, y1):
        ink = row_ink(img, x0, x1, y) >= min_ink
        if ink and start is None:
            start = y
        if not ink and start is not None:
            spans.append((start, y - 1))
            start = None
    if start is not None:
        spans.append((start, y1 - 1))
    return spans


def col_ink(img, y0, y1, x):
    px = img.load()
    n = 0
    for y in range(y0, min(y1, img.height)):
        if px[x, y] != BG:
            n += 1
    return n


def diff_mask(a, b):
    ia = Image.open(a).convert("RGB").load()
    ib = Image.open(b).convert("RGB").load()
    w = min(Image.open(a).width, Image.open(b).width)
    h = min(Image.open(a).height, Image.open(b).height)
    pts = []
    for y in range(h):
        for x in range(w):
            if ia[x, y] != ib[x, y]:
                pts.append((x, y))
    return pts


def main():
    print("== verify_shots regions reproduced with PIL ==")
    regions = {
        "solve-ready.png": [(570, 200, 1280, 500)],
        "solve-memo.png": [(570, 200, 1280, 500)],
        "solve-exec.png": [(570, 200, 1280, 500)],
        "solve-review.png": [(570, 200, 1280, 500)],
        "algs.png": [(25, 100, 500, 700), (530, 100, 1260, 700)],
        "algs-search.png": [(25, 100, 500, 700), (530, 100, 1260, 700)],
        "case-drill-show.png": [(25, 92, 900, 210)],
        "case-drill-grade.png": [(25, 92, 900, 230)],
    }
    for name, regs in regions.items():
        for i, (x0, y0, x1, y1) in enumerate(regs):
            o, _ = lit(old_shot(name), x0, y0, x1, y1)
            n, _ = lit(REPO / "docs/screenshots" / name, x0, y0, x1, y1)
            print(f"{name} r{i} old={o} new={n}")

    print()
    print("== solve rail ink bands (x 570..1270, y 85..430) ==")
    for name in ["solve-ready.png", "solve-memo.png", "solve-exec.png", "solve-review.png"]:
        img = Image.open(REPO / "docs/screenshots" / name).convert("RGB")
        spans = bands(img, 570, 85, 1270, 430)
        print(name, "bands:", spans)

    print()
    print("== case drill ink bands (x 20..900, y 85..340) ==")
    for name in ["case-drill-show.png", "case-drill-grade.png"]:
        img = Image.open(REPO / "docs/screenshots" / name).convert("RGB")
        spans = bands(img, 20, 85, 900, 340)
        print(name, "bands:", spans)

    print()
    print("== algs horizontal split, column ink y 95..770 ==")
    for name in ["algs.png", "algs-search.png"]:
        img = Image.open(REPO / "docs/screenshots" / name).convert("RGB")
        gaps = []
        run = 0
        for x in range(20, 1260):
            if col_ink(img, 95, 770, x) == 0:
                run += 1
            else:
                if run >= 20:
                    gaps.append((x - run, x - 1))
                run = 0
        print(name, "blank column runs >=20px:", gaps)
        spans = bands(img, 530, 90, 1270, 790)
        print(name, "detail vertical bands:", spans)

    print()
    print("== drill shot diffs old vs new ==")
    for name in ["drill-card.png", "drill-rate-a.png", "drill-rate-b.png", "drill-reps.png",
                 "menu.png", "settings.png", "stats.png", "fatal.png"]:
        o = old_shot(name)
        n = REPO / "docs/screenshots" / name
        same = o.read_bytes() == n.read_bytes()
        if same:
            print(name, "identical")
            continue
        pts = diff_mask(o, n)
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]
        left = sum(1 for p in pts if p[0] < 570)
        right = len(pts) - left
        print(name, f"diff={len(pts)} bbox=({min(xs)},{min(ys)})..({max(xs)},{max(ys)}) "
                    f"left_of_570={left} right={right}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
