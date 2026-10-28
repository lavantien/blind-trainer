import struct, sys, zlib, glob, os

def crc32(data):
    return zlib.crc32(data) & 0xFFFFFFFF

def audit(path):
    raw = open(path, "rb").read()
    problems = []
    if raw[:8] != bytes([137, 80, 78, 71, 13, 10, 26, 10]):
        problems.append("bad signature")
        return raw, problems
    pos = 8
    ihdr = None
    idat = bytearray()
    seen_iend = False
    while pos < len(raw):
        if pos + 8 > len(raw):
            problems.append(f"truncated chunk header at {pos}")
            break
        length = struct.unpack(">I", raw[pos:pos+4])[0]
        ctype = raw[pos+4:pos+8]
        data = raw[pos+8:pos+8+length]
        if len(data) < length:
            problems.append(f"truncated {ctype} chunk")
            break
        stored = struct.unpack(">I", raw[pos+8+length:pos+12+length])[0]
        actual = crc32(ctype + data)
        if stored != actual:
            problems.append(f"crc mismatch on {ctype}")
        if ctype == b"IHDR":
            ihdr = struct.unpack(">IIBBBBB", data)
        elif ctype == b"IDAT":
            idat += data
        elif ctype == b"IEND":
            seen_iend = True
        pos += 12 + length
    if ihdr is None:
        problems.append("no IHDR")
    else:
        w, h, depth, color = ihdr[0], ihdr[1], ihdr[2], ihdr[3]
        if (w, h) != (1280, 800):
            problems.append(f"dims {w}x{h}")
        if depth != 8 or color != 6:
            problems.append(f"depth {depth} color {color}")
    if not seen_iend:
        problems.append("no IEND")
    if ihdr and idat:
        try:
            pixels = zlib.decompress(bytes(idat))
            channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ihdr[3]]
            stride = ihdr[0] * channels + 1
            if len(pixels) != stride * ihdr[1]:
                problems.append(f"pixel bytes {len(pixels)} expected {stride * ihdr[1]}")
            for y in range(ihdr[1]):
                f = pixels[y * stride]
                if f > 4:
                    problems.append(f"bad filter {f} on row {y}")
                    break
        except zlib.error as e:
            problems.append(f"idat zlib: {e}")
    return raw, problems

for path in sorted(glob.glob("artifacts/e2e/*.png")):
    raw, problems = audit(path)
    print(f"{os.path.basename(path)}: {len(raw)} bytes " + ("OK" if not problems else "FAIL " + "; ".join(problems)))
