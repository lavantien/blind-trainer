import json
import struct
import sys
import zlib
from pathlib import Path

SIG = bytes([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A])


def decode(path):
    raw = Path(path).read_bytes()
    assert raw[:8] == SIG, f"{path}: bad signature"
    at = 8
    ihdr = None
    idat = b""
    chunks = []
    while at + 12 <= len(raw):
        ( length, ) = struct.unpack(">I", raw[at : at + 4])
        ctype = raw[at + 4 : at + 8]
        body = raw[at + 8 : at + 8 + length]
        chunks.append(ctype.decode("ascii"))
        if ctype == b"IHDR":
            ihdr = struct.unpack(">IIBBBBB", body)
        elif ctype == b"IDAT":
            idat += body
        at += 12 + length
    w, h, depth, ctype_, comp, filt, interlace = ihdr
    assert depth == 8, f"{path}: depth {depth}"
    assert ctype_ == 6, f"{path}: colortype {ctype_}"
    assert interlace == 0, f"{path}: interlace"
    bpp = 4
    stride = w * bpp
    data = zlib.decompress(idat)
    assert len(data) == h * (stride + 1), f"{path}: inflated {len(data)} want {h * (stride + 1)}"
    out = bytearray(h * stride)
    prev = bytearray(stride)
    for y in range(h):
        ft = data[y * (stride + 1)]
        line = bytearray(data[y * (stride + 1) + 1 : (y + 1) * (stride + 1)])
        if ft == 0:
            pass
        elif ft == 1:
            for x in range(bpp, stride):
                line[x] = (line[x] + line[x - bpp]) & 0xFF
        elif ft == 2:
            for x in range(stride):
                line[x] = (line[x] + prev[x]) & 0xFF
        elif ft == 3:
            for x in range(stride):
                left = line[x - bpp] if x >= bpp else 0
                line[x] = (line[x] + ((left + prev[x]) >> 1)) & 0xFF
        elif ft == 4:
            for x in range(stride):
                a = line[x - bpp] if x >= bpp else 0
                b = prev[x]
                c = prev[x - bpp] if x >= bpp else 0
                p = a + b - c
                pa = abs(p - a)
                pb = abs(p - b)
                pc = abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[x] = (line[x] + pr) & 0xFF
        else:
            raise AssertionError(f"{path}: filter {ft}")
        out[y * stride : (y + 1) * stride] = line
        prev = line
    return w, h, bytes(out), chunks


def distance(px_a, px_b, tolerance):
    over = 0
    max_delta = 0
    for at in range(0, len(px_a), 4):
        pixel_over = False
        for ch in range(4):
            a = px_a[at + ch]
            b = px_b[at + ch]
            d = a - b if a > b else b - a
            if d > max_delta:
                max_delta = d
            if d > tolerance:
                pixel_over = True
        if pixel_over:
            over += 1
    return max_delta, over


if __name__ == "__main__":
    tolerance = int(sys.argv[1]) if len(sys.argv) > 1 else 48
    root = Path(__file__).resolve().parents[2]
    bases = sorted((root / "tests/visual/baselines").glob("*.png"))
    info = {}
    pixels = {}
    for p in bases:
        w, h, px, chunks = decode(p)
        info[p.name] = {"w": w, "h": h, "rgba_sha": zlib.crc32(px), "chunks": chunks}
        pixels[p.name] = px
    cross = []
    names = sorted(pixels)
    for i, a in enumerate(names):
        for b in names[i + 1 :]:
            md, over = distance(pixels[a], pixels[b], tolerance)
            cross.append({"a": a, "b": b, "max_delta": md, "pixels_over": over})
    cross.sort(key=lambda r: r["pixels_over"])
    print(json.dumps({"baselines": info, "closest_pair": cross[:3]}, indent=1))
