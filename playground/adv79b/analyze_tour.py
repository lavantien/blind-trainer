import sys
from pathlib import Path

from PIL import Image

SHOTS = Path(__file__).resolve().parent / "shots"
BG = (23, 26, 31)
DIFF = 30


def load(name):
    return Image.open(SHOTS / name).convert("RGB")


def ink(image, x, y):
    p = image.load()[x, y]
    return max(abs(p[0] - BG[0]), abs(p[1] - BG[1]), abs(p[2] - BG[2])) > DIFF


def bounds(image, box=None):
    x0, y0, x1, y1 = box or (0, 0, image.size[0], image.size[1])
    xs = []
    ys = []
    for y in range(int(y0), int(y1)):
        for x in range(int(x0), int(x1)):
            if ink(image, x, y):
                xs.append(x)
                ys.append(y)
    if not xs:
        return None
    return min(xs), max(xs), min(ys), max(ys)


def row_bands(image, box):
    x0, y0, x1, y1 = [int(v) for v in box]
    bands = []
    run = None
    for y in range(y0, y1):
        lit = any(ink(image, x, y) for x in range(x0, x1))
        if lit and run is None:
            run = [y, y]
        elif lit:
            run[1] = y
        elif run is not None:
            bands.append(tuple(run))
            run = None
    if run is not None:
        bands.append(tuple(run))
    return bands


def global_report(name):
    image = load(name)
    b = bounds(image)
    if b is None:
        print(f"{name}: blank")
        return
    left, right, top, bottom = b
    flags = []
    if right >= image.size[0] - 1:
        flags.append("TOUCHES-RIGHT")
    if bottom >= image.size[1] - 1:
        flags.append("TOUCHES-BOTTOM")
    if left <= 0:
        flags.append("TOUCHES-LEFT")
    print(f"{name}: ink x {left}-{right} y {top}-{bottom} {' '.join(flags)}")


def solve_column(name):
    image = load(name)
    box = (560, 120, 1280, 800)
    bands = row_bands(image, box)
    print(f"{name} right column bands {bands}")
    if bands:
        lowest = bands[-1][1]
        print(f"{name} lowest right-column ink row {lowest}, window 800, margin {800 - lowest}")


def centered_line(name, y_hint):
    image = load(name)
    bands = row_bands(image, (0, max(0, y_hint - 40), 1280, min(800, y_hint + 40)))
    for y0, y1 in bands:
        b = bounds(image, (0, y0, 1280, y1 + 1))
        if b:
            print(f"{name} band y {y0}-{y1} spans x {b[0]}-{b[1]}")


def main():
    for name in sorted(p.name for p in SHOTS.glob("m_*.png")):
        global_report(name)
    print()
    for name in ["t_01_solve_ready.png", "t_02_solve_memo.png", "t_03_solve_exec.png",
                 "t_04_solve_review.png"]:
        solve_column(name)
    print()
    for name in ["t_05_solve_replay.png", "m_replay.png"]:
        global_report(name)
    print()
    for name in ["t_06_memo_intro.png", "t_07_memo_stream.png", "t_08_memo_late.png",
                 "t_09_memo_rate.png", "p_02_memo_rate.png"]:
        image = load(name)
        bands = row_bands(image, (0, 120, 1280, 800))
        print(f"{name} bands {bands}")
        for y0, y1 in bands:
            b = bounds(image, (0, y0, 1280, y1 + 1))
            if b:
                print(f"  band y {y0}-{y1} x {b[0]}-{b[1]}")
    print()
    for name in ["t_10_drill_card.png", "t_11_drill_reps.png", "t_12_drill_rate.png",
                 "t_13_algs.png", "t_14_algs_scrolled.png", "t_15_stats.png"]:
        global_report(name)
        image = load(name)
        bands = row_bands(image, (0, 120, 1280, 800))
        tallest = max((b[1] - b[0] + 1 for b in bands), default=0)
        print(f"  bands {len(bands)} tallest {tallest}")


if __name__ == "__main__":
    main()
