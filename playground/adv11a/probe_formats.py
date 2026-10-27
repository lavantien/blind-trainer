import json
import os
import struct
import subprocess
import sys
import zlib

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
WORK = os.path.join(ROOT, "build", "test-tmp", "adv11a")
SHOTS = os.path.join(WORK, "shots")
BASE = os.path.join(WORK, "base")
EXE = os.path.join(ROOT, "build", "bin", "visual-diff.exe")


def chunk(tag, data):
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)


def write_png(path, width, height, depth, ctype, rows, extra=b""):
    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", width, height, depth, ctype, 0, 0, 0)
    idat = zlib.compress(rows)
    with open(path, "wb") as fh:
        fh.write(sig + chunk(b"IHDR", ihdr) + extra + chunk(b"IDAT", idat) + chunk(b"IEND", b""))


def fl32(x):
    return struct.unpack("f", struct.pack("f", x))[0]


def main():
    for d in (SHOTS, BASE):
        os.makedirs(d, exist_ok=True)
    config = os.path.join(WORK, "config.json")
    with open(config, "w") as fh:
        json.dump({"visual": {"tolerance": 0}}, fh)

    n = 256

    write_png(os.path.join(SHOTS, "gray.png"), n, 1, 8, 0, b"\x00" + bytes(range(n)))
    write_png(os.path.join(BASE, "gray.png"), n, 1, 8, 6,
              b"\x00" + bytes(b for v in range(n) for b in (v, v, v, 255)))

    graya = bytearray()
    for v in range(n):
        graya += bytes((v, 255 - v))
    write_png(os.path.join(SHOTS, "graya.png"), n, 1, 8, 4, b"\x00" + bytes(graya))
    base_ga = bytearray()
    for v in range(n):
        base_ga += bytes((v, v, v, 255 - v))
    write_png(os.path.join(BASE, "graya.png"), n, 1, 8, 6, b"\x00" + bytes(base_ga))

    rgba16 = bytearray()
    base16 = bytearray()
    for v in range(n):
        for chan in (v, 255 - v, v // 2, 255):
            rgba16 += struct.pack(">H", chan * 257)
        base16 += bytes((v, 255 - v, v // 2, 255))
    write_png(os.path.join(SHOTS, "rgba16.png"), n, 1, 16, 6, b"\x00" + bytes(rgba16))
    write_png(os.path.join(BASE, "rgba16.png"), n, 1, 8, 6, b"\x00" + bytes(base16))

    plte = bytes(b for v in range(n) for b in (v, 255 - v, v // 2))
    trns = bytes(255 - v for v in range(n))
    pal_extra = chunk(b"PLTE", plte) + chunk(b"tRNS", trns)
    write_png(os.path.join(SHOTS, "pal.png"), n, 1, 8, 3, b"\x00" + bytes(range(n)), extra=pal_extra)
    pal_base = bytearray()
    for v in range(n):
        pal_base += bytes((v, 255 - v, v // 2, 255 - v))
    write_png(os.path.join(BASE, "pal.png"), n, 1, 8, 6, b"\x00" + bytes(pal_base))

    name = "ảnh.png"
    with open(os.path.join(SHOTS, name), "wb") as fh:
        fh.write(open(os.path.join(BASE, "gray.png"), "rb").read())
    write_png(os.path.join(BASE, name), n, 1, 8, 6,
              b"\x00" + bytes(b for v in range(n) for b in (v, v, v, 255)))

    bad = []
    for v in range(256):
        u = fl32(fl32(v / 255.0) * 255.0)
        if int(u) != v:
            bad.append((v, repr(u)))

    env = dict(os.environ)
    env["BT_CONFIG"] = config
    env["BT_ARTIFACTS"] = os.path.join(WORK, "art")
    proc = subprocess.run([EXE, SHOTS, BASE], capture_output=True, text=True, encoding="utf-8",
                          errors="backslashreplace", env=env, timeout=120)
    print("exit:", proc.returncode)
    print("stdout:")
    print(proc.stdout)
    print("stderr:")
    print(proc.stderr)
    report = os.path.join(WORK, "art", "visual", "report.md")
    if os.path.isfile(report):
        print(open(report, encoding="utf-8", errors="backslashreplace").read())
    print("f32 roundtrip mismatches:", bad if bad else "none")


if __name__ == "__main__":
    main()
