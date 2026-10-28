import struct
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from pngprobe import decode


def chunk(tag, body):
    return (
        struct.pack(">I", len(body))
        + tag
        + body
        + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF)
    )


def encode(path, w, h, rgba):
    raw = b"".join(b"\x00" + bytes(rgba[y * w * 4 : (y + 1) * w * 4]) for y in range(h))
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    data = zlib.compress(raw, 9)
    png = bytes([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A])
    png += chunk(b"IHDR", ihdr) + chunk(b"IDAT", data) + chunk(b"IEND", b"")
    Path(path).write_bytes(png)


if __name__ == "__main__":
    src, dst = sys.argv[1], sys.argv[2]
    w, h, px, _ = decode(src)
    px = bytearray(px)
    for y in range(100, 300):
        for x in range(200, 500):
            at = (y * w + x) * 4
            px[at] = (px[at] + 120) & 0xFF
    encode(dst, w, h, bytes(px))
    print("wrote", dst)
