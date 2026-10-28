import struct, zlib, sys

def load(path):
    data = open(path, "rb").read()
    pos = 8
    w = h = None
    bd = ct = None
    plte = b""
    trns = b""
    idat = b""
    while pos < len(data):
        ln = struct.unpack(">I", data[pos:pos+4])[0]
        t = data[pos+4:pos+8]
        ch = data[pos+8:pos+8+ln]
        if t == b"IHDR":
            w, h, bd, ct = struct.unpack(">IIBB", ch[:10])
        if t == b"PLTE":
            plte = ch
        if t == b"tRNS":
            trns = ch
        if t == b"IDAT":
            idat += ch
        pos += 12 + ln
        if t == b"IEND":
            break
    raw = zlib.decompress(idat)
    channels = {0:1, 2:3, 3:1, 4:2, 6:4}[ct]
    bpp = channels
    stride = w * bpp
    lines = []
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
        lines.append(bytes(line))
        prev = line
    def px(x, y):
        row = lines[y]
        o = x * channels
        if ct == 6:
            return (row[o], row[o+1], row[o+2])
        if ct == 2:
            return (row[o], row[o+1], row[o+2])
        if ct == 3:
            p = row[o]
            return (plte[p*3], plte[p*3+1], plte[p*3+2])
        if ct == 4:
            v = row[o]
            return (v, v, v)
        return (row[o], row[o], row[o])
    return w, h, px, ct

for path in sys.argv[1:]:
    w, h, px, ct = load(path)
    n = 0
    rows = {}
    for y in range(100, 800):
        lit = 0
        for x in range(570, 1280):
            if px(x, y) != (23, 26, 31):
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
    print(path, "colortype", ct)
    print("   rail nonbg px y100-800:", n, " bands:", [(g[0], g[1], g[2]) for g in groups])
