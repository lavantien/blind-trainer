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

def diffpx(a, b, label):
    wa, ha, pa = load(a)
    wb, hb, pb = load(b)
    pts = []
    for y in range(ha):
        for x in range(0, wa):
            oa = (y*wa + x)*4
            if pa[oa:oa+3] != pb[oa:oa+3]:
                pts.append((x, y, pa[oa:oa+3], pb[oa:oa+3]))
    print(label, "differing px:")
    for p in pts[:40]:
        print("   ", p)

def textmap(path, label, x0, y0, x1, y1):
    w, h, px = load(path)
    rows = []
    for y in range(y0, y1):
        lit = 0
        xs = []
        for x in range(x0, x1):
            o = (y*w + x)*4
            c = (px[o], px[o+1], px[o+2])
            if c != (23, 26, 31) and sum(c) > 200:
                lit += 1
                xs.append(x)
        if lit:
            rows.append((y, lit, xs[0] if xs else -1, xs[-1] if xs else -1))
    print(label, "lit text rows (y, count, xstart, xend):")
    merged = []
    for r in rows:
        if merged and r[0] - merged[-1][-1][0] <= 2:
            merged[-1].append(r)
        else:
            merged.append([r])
    for grp in merged:
        ys = [g[0] for g in grp]
        cnt = sum(g[1] for g in grp)
        print(f"   y {min(ys)}-{max(ys)} litpx={cnt} x {grp[0][2]}..{grp[-1][3]}")

diffpx(r"docs/screenshots/solve-ready.png", r"docs/screenshots/solve-review.png", "ready vs review")
diffpx(r"docs/screenshots/case-drill-show.png", r"docs/screenshots/case-drill-grade.png", "show vs grade")
diffpx(r"docs/screenshots/algs.png", r"docs/screenshots/algs-search.png", "algs vs algs-search")
diffpx(r"docs/screenshots/solve-ready.png", r"docs/screenshots/solve-memo.png", "ready vs memo")
textmap(r"docs/screenshots/solve-review.png", "solve-review lit rows", 0, 0, 1280, 800)
