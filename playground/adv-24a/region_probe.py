import struct, zlib

def load(path):
    data = open(path, "rb").read()
    pos = 8
    w = h = None
    idat = b""
    while pos < len(data):
        ln = struct.unpack(">I", data[pos:pos+4])[0]
        ct = data[pos+4:pos+8]
        ch = data[pos+8:pos+8+ln]
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
    out = bytearray(w*h*bpp)
    prev = bytearray(stride)
    i = 0
    for y in range(h):
        f = raw[i]; i += 1
        line = bytearray(raw[i:i+stride]); i += stride
        if f == 1:
            for x in range(bpp, stride):
                line[x] = (line[x] + line[x-bpp]) & 0xFF
        elif f == 2:
            for x in range(stride):
                line[x] = (line[x] + prev[x]) & 0xFF
        elif f == 3:
            for x in range(stride):
                a = line[x-bpp] if x >= bpp else 0
                line[x] = (line[x] + ((a + prev[x]) >> 1)) & 0xFF
        elif f == 4:
            for x in range(stride):
                a = line[x-bpp] if x >= bpp else 0
                b = prev[x]
                c = prev[x-bpp] if x >= bpp else 0
                p = a + b - c
                pa, pb, pc = abs(p-a), abs(p-b), abs(p-c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[x] = (line[x] + pr) & 0xFF
        out[y*stride:(y+1)*stride] = line
        prev = line
    return w, h, bytes(out)

def region_stats(path, x0, y0, x1, y1):
    w, h, px = load(path)
    bg = None
    vals = {}
    maxdev = 0
    n = 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            o = (y*w + x)*4
            c = (px[o], px[o+1], px[o+2])
            vals[c] = vals.get(c, 0) + 1
            n += 1
    top = sorted(vals.items(), key=lambda kv: -kv[1])[:6]
    print(path.split("/")[-1], "region", (x0,y0,x1,y1), "n=", n)
    for c, k in top:
        print("   ", c, k, f"{100*k/n:.2f}%")

def diff(a, b):
    wa, ha, pa = load(a)
    wb, hb, pb = load(b)
    assert (wa, ha) == (wb, hb)
    d = 0
    first = None
    for y in range(ha):
        for x in range(0, wa*4, 4):
            oa = y*wa*4 + x
            if pa[oa:oa+3] != pb[oa:oa+3]:
                d += 1
                if first is None:
                    first = (x//4, y)
    print(f"diff {a.split(chr(47))[-1]} vs {b.split(chr(47))[-1]}: {d} px ({100*d/(wa*ha):.2f}%) first at {first}")

region_stats(r"docs/screenshots/solve-review.png", 560, 200, 1280, 500)
region_stats(r"docs/screenshots/solve-ready.png", 560, 200, 1280, 500)
region_stats(r"docs/screenshots/solve-exec.png", 560, 200, 1280, 500)
diff(r"docs/screenshots/solve-ready.png", r"docs/screenshots/solve-review.png")
diff(r"docs/screenshots/solve-memo.png", r"docs/screenshots/solve-review.png")
diff(r"docs/screenshots/solve-ready.png", r"docs/screenshots/solve-exec.png")
diff(r"docs/screenshots/case-drill-show.png", r"docs/screenshots/case-drill-grade.png")
diff(r"docs/screenshots/algs.png", r"docs/screenshots/algs-search.png")
diff(r"docs/screenshots/drill-rate-a.png", r"docs/screenshots/drill-rate-b.png")
