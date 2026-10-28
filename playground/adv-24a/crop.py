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

def save_png(path, w, h, px):
    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    sig = bytes([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A])
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        raw += px[y*w*4:(y+1)*w*4]
    out = sig + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")
    open(path, "wb").write(out)

src, x0, y0, x1, y1, scale, dst = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5]), int(sys.argv[6]), sys.argv[7]
w, h, px = load(src)
cw, ch = x1-x0, y1-y0
buf = bytearray(cw*ch*4*scale*scale)
for y in range(ch*scale):
    for x in range(cw*scale):
        sx, sy = x0 + x//scale, y0 + y//scale
        o = (sy*w + sx)*4
        b = (y*cw*scale + x)*4
        buf[b:b+4] = px[o:o+4]
save_png(dst, cw*scale, ch*scale, bytes(buf))
print("wrote", dst, cw*scale, "x", ch*scale)
