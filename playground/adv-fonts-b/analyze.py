import hashlib
import sys
from collections import Counter
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[2]
SHOTS = REPO / "playground" / "adv-fonts-b" / "shots"
DOCS = REPO / "docs" / "screenshots"
BASES = REPO / "tests" / "visual" / "baselines"


def load(path):
    img = Image.open(path).convert("RGB")
    return img, img.size


def bg_of(img, box):
    crop = img.crop(box)
    counts = Counter(crop.getdata())
    return counts.most_common(1)[0][0]


def row_counts(img, box):
    x0, y0, x1, y1 = box
    crop = img.crop(box)
    w, h = crop.size
    px = crop.load()
    bg = Counter(crop.getdata()).most_common(1)[0][0]
    rows = []
    for y in range(h):
        n = 0
        for x in range(w):
            if px[x, y] != bg:
                n += 1
        rows.append(n)
    return rows, bg


def bands(img, box, min_lit=2, gap_merge=2):
    rows, bg = row_counts(img, box)
    spans = []
    at = 0
    while at < len(rows):
        if rows[at] >= min_lit:
            start = at
            while at < len(rows) and rows[at] >= min_lit:
                at += 1
            spans.append((start + box[1], at - 1 + box[1], at - start))
        else:
            at += 1
    merged = []
    for span in spans:
        if merged and span[0] - merged[-1][1] - 1 <= gap_merge:
            m = merged[-1]
            merged[-1] = (m[0], span[1], span[1] - m[0] + 1)
        else:
            merged.append(span)
    total = sum(r for r in rows if r >= min_lit)
    return merged, total, bg


def region_lit(img, box):
    merged, total, bg = bands(img, box)
    return total


def diff_pixels(path_a, path_b, box):
    a = Image.open(path_a).convert("RGB").crop(box)
    b = Image.open(path_b).convert("RGB").crop(box)
    pa, pb = a.load(), b.load()
    w, h = a.size
    n = 0
    for y in range(h):
        for x in range(w):
            if pa[x, y] != pb[x, y]:
                n += 1
    return n


def report(name, box, min_lit=2):
    img, size = load(SHOTS / name)
    merged, total, bg = bands(img, box, min_lit)
    print(f"{name} box={box} bg={bg} bands={len(merged)} lit={total}")
    for top, bottom, height in merged:
        print(f"  band y {top}-{bottom} height={height}")
    return merged, total


def main():
    print("== solve rail across phases, region x586-1280 y90-780 ==")
    rail = (586, 90, 1280, 780)
    solve_names = ["solve-ready.png", "solve-memo.png", "solve-exec.png", "solve-review.png"]
    for name in solve_names:
        report(name, rail)
    print("== solve phase pairwise pixel diffs in rail ==")
    for i in range(len(solve_names)):
        for j in range(i + 1, len(solve_names)):
            n = diff_pixels(SHOTS / solve_names[i], SHOTS / solve_names[j], rail)
            print(f"  {solve_names[i]} vs {solve_names[j]}: {n} px differ")

    print("== algs rows and detail, columns x20-510 and x516-1270 y90-780 ==")
    rows_box = (20, 90, 510, 780)
    detail_box = (516, 90, 1270, 780)
    report("algs.png", rows_box)
    report("algs.png", detail_box)
    report("algs-search.png", rows_box)
    report("algs-search.png", detail_box)
    print(f"algs vs algs-search rows diff: {diff_pixels(SHOTS / 'algs.png', SHOTS / 'algs-search.png', rows_box)} px")
    print(f"algs vs algs-search detail diff: {diff_pixels(SHOTS / 'algs.png', SHOTS / 'algs-search.png', detail_box)} px")

    print("== case drill text band x20-900 y90-240 ==")
    cd_box = (20, 90, 900, 240)
    report("case-drill-show.png", cd_box)
    report("case-drill-grade.png", cd_box)
    print(f"show vs grade diff: {diff_pixels(SHOTS / 'case-drill-show.png', SHOTS / 'case-drill-grade.png', (0, 90, 1280, 780))} px in full band")

    print("== cross view size consistency, band heights for same roles ==")
    report("menu.png", (20, 90, 1270, 780))
    report("settings.png", (20, 90, 1270, 780))
    report("stats.png", (20, 90, 1270, 780))
    report("fatal.png", (0, 0, 1280, 800))

    print("== baselines carry text ==")
    for name in ["solve.png", "algs.png", "case-drill.png"]:
        merged, total, bg = bands(Image.open(BASES / name).convert("RGB"),
                                  (586, 90, 1280, 780) if name == "solve.png"
                                  else (20, 90, 510, 780) if name == "algs.png"
                                  else (20, 90, 900, 240))
        print(f"baseline {name} bands={len(merged)} lit={total}")

    print("== docs vs fresh shot drill nondeterminism, whole frame ==")
    for name in ["drill-card.png", "drill-reps.png", "drill-rate-a.png", "drill-rate-b.png"]:
        n = diff_pixels(SHOTS / name, DOCS / name, (0, 0, 1280, 800))
        print(f"  {name}: {n} px differ docs vs fresh capture")
    return 0


if __name__ == "__main__":
    sys.exit(main())
