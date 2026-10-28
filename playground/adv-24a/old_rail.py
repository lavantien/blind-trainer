import struct, zlib, sys

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

for path in sys.argv[1:]:
    w, h, px = load(path)
    n = 0
    rows = {}
    for y in range(100, 800):
        lit = 0
        for x in range(570, 1280):
            o = (y*w + x)*4
            if (px[o], px[o+1], px[o+2]) != (23, 26, 31):
                lit += 1
        if lit:
            rows[y] = lit
            n += lit
    groups = []
    for y in sorted(rows):
        if groups and y - groups[-1][1] <= 2:
            groups[-1][1] = y
            groups[-1][2] += rows[y]
        else:
            groups.append([y, y, rows[y]])
    print(path)
    print("   rail nonbg px y100-800:", n, " row bands:", [(g[0], g[1], g[2]) for g in groups])
