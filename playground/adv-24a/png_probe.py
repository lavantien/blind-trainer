import struct, os, hashlib, glob

def png_info(path):
    with open(path, "rb") as f:
        data = f.read()
    sig = bytes([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A])
    if data[:8] != sig:
        return None
    pos = 8
    w = h = None
    idat = b""
    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos+4])[0]
        ctype = data[pos+4:pos+8]
        chunk = data[pos+8:pos+8+length]
        if ctype == b"IHDR":
            w, h = struct.unpack(">II", chunk[:8])
        if ctype == b"IDAT":
            idat += chunk
        pos += 12 + length
        if ctype == b"IEND":
            break
    return w, h, len(data), idat

seen = {}
for p in sorted(glob.glob(r"docs/screenshots/*.png")):
    r = png_info(p)
    if r is None:
        print(os.path.basename(p), "NOT A PNG")
        continue
    w, h, sz, idat = r
    digest = hashlib.sha1(idat).hexdigest()[:12]
    dup = " DUP!" if digest in seen else ""
    if not dup:
        seen[digest] = p
    print(f"{os.path.basename(p):26s} {w}x{h} {sz}b idat={len(idat)} sha1={digest}{dup}")
