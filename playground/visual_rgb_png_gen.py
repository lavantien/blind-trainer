import struct
import zlib
from pathlib import Path

WIDTH = 2
HEIGHT = 2
PIXEL = bytes([200, 10, 10])


def chunk(kind: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def main() -> None:
    ihdr = struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 2, 0, 0, 0)
    raw = b"".join(b"\x00" + PIXEL * WIDTH for _ in range(HEIGHT))
    compressor = zlib.compressobj(0)
    idat = compressor.compress(raw) + compressor.flush()
    assert zlib.decompress(idat) == raw, "the idat stream must round trip"
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", idat) + chunk(b"IEND", b"")
    out = Path(__file__).resolve().parent / "visual_rgb_png.bin"
    out.write_bytes(png)
    rows = [", ".join(str(b) for b in png[at : at + 16]) for at in range(0, len(png), 16)]
    print(f"len={len(png)}")
    for row in rows:
        print(f"        {row},")


if __name__ == "__main__":
    main()
