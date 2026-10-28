import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from verify_shots import load


def diff_regions(a_path, b_path, x0, x1, y0, y1, step=4):
    wa, ha, pa = load(a_path)
    wb, hb, pb = load(b_path)
    assert (wa, ha) == (wb, hb)
    n = 0
    rows = set()
    for y in range(y0, y1, step):
        for x in range(x0, x1, step):
            oa = (y * wa + x) * 4
            if pa[oa:oa + 3] != pb[oa:oa + 3]:
                n += 1
                rows.add(y)
    return n, (min(rows), max(rows)) if rows else None


def main():
    docs = Path(__file__).resolve().parents[2] / "docs" / "screenshots"
    pairs = [
        ("algs.png", "algs-search.png", 25, 500, 100, 700),
        ("algs.png", "algs-search.png", 530, 1260, 100, 700),
    ]
    for a, b, x0, x1, y0, y1 in pairs:
        n, rows = diff_regions(docs / a, docs / b, x0, x1, y0, y1)
        print(f"{a} vs {b} region({x0},{y0},{x1},{y1}) sampled-diff={n} rows={rows}")


if __name__ == "__main__":
    main()
