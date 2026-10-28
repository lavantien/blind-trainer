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

for name in ["algs.png", "algs-search.png", "fatal.png"]:
    w, h, px = load("docs/screenshots/" + name)
    colors = set()
    for y in range(h):
        base = y*w*4
        for x in range(0, w*4, 4):
            colors.add((px[base+x], px[base+x+1], px[base+x+2], px[base+x+3]))
    top = sorted(colors, key=lambda c: -sum(px.count(bytes(c)) if False else 0 for _ in [0]))
    print(name, "full_colors=", len(colors))
    cnt = {}
    for y in range(0, h, 2):
        base = y*w*4
        for x in range(0, w*4, 8):
            c = (px[base+x], px[base+x+1], px[base+x+2])
            cnt[c] = cnt.get(c, 0) + 1
    for c, n in sorted(cnt.items(), key=lambda kv: -kv[1])[:8]:
        print("   ", c, n)
