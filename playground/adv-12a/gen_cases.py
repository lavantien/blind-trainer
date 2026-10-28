import os
import random
import zlib
import zipfile

ROOT = os.path.dirname(os.path.abspath(__file__))
CASES = os.path.join(ROOT, "cases")


def corpora():
    rng = random.Random(20261028)
    text = ("the quick brown fox jumps over the lazy dog " * 40).encode()
    yield "empty", b""
    yield "one", b"x"
    yield "repeat_a", b"A" * 200000
    yield "text", text
    yield "text_x3", text * 3
    yield "seq", bytes(range(256)) * 400
    yield "random", rng.randbytes(120000)
    yield "mixed", text + rng.randbytes(50000) + text
    yield "sweep", b"".join(bytes([b]) * n for b, n in ((65 + i % 26, 3 + i % 250) for i in range(500)))
    yield "far_match", (lambda s: s + rng.randbytes(40000) + s)(b"far-back-reference-payload-0123456789" * 8)
    yield "exact_65535", b"Z" * 65535
    yield "exact_65536", b"Z" * 65536
    yield "exact_131072", b"Q" * 131072


def main():
    os.makedirs(CASES, exist_ok=True)
    manifest = []
    for name, data in corpora():
        expected = os.path.join(CASES, f"{name}.expected")
        with open(expected, "wb") as handle:
            handle.write(data)
        for level in (0, 1, 6, 9):
            packed = zlib.compress(data, level)
            tag = f"{name}_l{level}"
            raw = os.path.join(CASES, f"{tag}.raw")
            with open(raw, "wb") as handle:
                handle.write(packed[2:-4])
            manifest.append(("raw", raw, expected))
            zpath = os.path.join(CASES, f"{tag}.zlib")
            with open(zpath, "wb") as handle:
                handle.write(packed)
            manifest.append(("zlib", zpath, expected))
    with open(os.path.join(ROOT, "manifest.txt"), "w", encoding="utf-8") as handle:
        for kind, src, dst in manifest:
            handle.write(f"{kind}\t{src}\t{dst}\n")

    staging = os.path.join(ROOT, "cases", "staging")
    os.makedirs(os.path.join(staging, "config"), exist_ok=True)
    os.makedirs(os.path.join(staging, "assets", "fonts"), exist_ok=True)
    rng = random.Random(7)
    exe = rng.randbytes(400000)
    with open(os.path.join(staging, "blind-trainer.exe"), "wb") as handle:
        handle.write(exe)
    with open(os.path.join(staging, "config", "config.json"), "wb") as handle:
        handle.write(b'{"app": {"window": {"width": 1280, "height": 800}}}')
    with open(os.path.join(staging, "assets", "fonts", "main.ttf"), "wb") as handle:
        handle.write(rng.randbytes(150000))
    with open(os.path.join(staging, "assets", "fonts", "bold.ttf"), "wb") as handle:
        handle.write(b"bold font bytes " * 2000)

    zip_path = os.path.join(ROOT, "cases", "real-deflate.zip")
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as archive:
        for name in (
            "blind-trainer\\blind-trainer.exe",
            "blind-trainer\\config\\config.json",
            "blind-trainer/assets/fonts/main.ttf",
            "blind-trainer/assets/fonts/bold.ttf",
        ):
            leaf = name.replace("\\", "/")
            with open(os.path.join(staging, leaf[len("blind-trainer/"):]), "rb") as handle:
                archive.writestr(name, handle.read())

    shots = os.path.join(ROOT, "cases", "shots-both")
    os.makedirs(shots, exist_ok=True)
    png = minimal_png(10, 10)
    with open(os.path.join(shots, "menu.png"), "wb") as handle:
        handle.write(png)
    with open(os.path.join(shots, "stray.txt"), "wb") as handle:
        handle.write(b"")
    print(f"cases={len(manifest)}")


def minimal_png(width, height):
    def chunk(kind, data):
        return (
            len(data).to_bytes(4, "big")
            + kind
            + data
            + (zlib.crc32(kind + data) & 0xFFFFFFFF).to_bytes(4, "big")
        )

    raw = b"".join(b"\x00" + bytes([9] * width) for _ in range(height))
    ihdr = width.to_bytes(4, "big") + height.to_bytes(4, "big") + bytes([8, 0, 0, 0, 0])
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b"")


if __name__ == "__main__":
    main()
