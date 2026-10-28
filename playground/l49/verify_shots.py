import struct
import subprocess
import sys
import zlib
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]


def load(path):
    data = Path(path).read_bytes()
    pos = 8
    w = h = None
    idat = b""
    while pos < len(data):
        ln = struct.unpack(">I", data[pos:pos + 4])[0]
        ct = data[pos + 4:pos + 8]
        ch = data[pos + 8:pos + 8 + ln]
        if ct == b"IHDR":
            w, h = struct.unpack(">II", ch[:8])
        if ct == b"IDAT":
            idat += ch
        pos += 12 + ln
        if ct == b"IEND":
            break
    raw = zlib.decompress(idat)
    bpp = 4
    stride = w * bpp
    out = bytearray(w * h * bpp)
    prev = bytearray(stride)
    i = 0
    for y in range(h):
        f = raw[i]
        i += 1
        line = bytearray(raw[i:i + stride])
        i += stride
        if f == 1:
            for x in range(bpp, stride):
                line[x] = (line[x] + line[x - bpp]) & 0xFF
        elif f == 2:
            for x in range(stride):
                line[x] = (line[x] + prev[x]) & 0xFF
        elif f == 3:
            for x in range(stride):
                a = line[x - bpp] if x >= bpp else 0
                line[x] = (line[x] + ((a + prev[x]) >> 1)) & 0xFF
        elif f == 4:
            for x in range(stride):
                a = line[x - bpp] if x >= bpp else 0
                b = prev[x]
                c = prev[x - bpp] if x >= bpp else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[x] = (line[x] + pr) & 0xFF
        out[y * stride:(y + 1) * stride] = line
        prev = line
    return w, h, bytes(out)


def lit(path, x0, y0, x1, y1, bg=(23, 26, 31)):
    w, h, px = load(path)
    n = 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            o = (y * w + x) * 4
            if (px[o], px[o + 1], px[o + 2]) != bg:
                n += 1
    return n


SHOTS = {
    "solve-ready.png": [(570, 200, 1280, 500)],
    "solve-memo.png": [(570, 200, 1280, 500)],
    "solve-exec.png": [(570, 200, 1280, 500)],
    "solve-review.png": [(570, 200, 1280, 500)],
    "algs.png": [(25, 100, 500, 700), (530, 100, 1260, 700)],
    "algs-search.png": [(25, 100, 500, 700), (530, 100, 1260, 700)],
    "case-drill-show.png": [(25, 92, 900, 210)],
    "case-drill-grade.png": [(25, 92, 900, 230)],
}

FLOOR = 400


def main():
    old_dir = Path(__file__).parent / "old"
    old_dir.mkdir(exist_ok=True)
    failures = 0
    for name, regions in SHOTS.items():
        rel = f"docs/screenshots/{name}"
        old = old_dir / name
        if not old.is_file():
            blob = subprocess.run(
                ["git", "-C", str(REPO), "show", f"HEAD:{rel}"],
                capture_output=True, check=True,
            ).stdout
            old.write_bytes(blob)
        new_path = REPO / rel
        for at, (x0, y0, x1, y1) in enumerate(regions):
            o = lit(old, x0, y0, x1, y1)
            n = lit(new_path, x0, y0, x1, y1)
            ok = n >= FLOOR and n > o * 4
            if not ok:
                failures += 1
            print(f"{name} region{at} ({x0},{y0},{x1},{y1}) old={o} new={n} floor={FLOOR} "
                  f"{'OK' if ok else 'FAIL'}")
    print(f"verify_shots: {'PASS' if failures == 0 else 'FAIL'} ({failures} failures)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
