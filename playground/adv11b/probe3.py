import os
import struct
import subprocess
import sys
import zlib
from pathlib import Path

REPO = Path(r"C:\Users\lavantien\dev\github\blind-trainer")
EXE = REPO / "build" / "bin" / "visual-diff.exe"
WORK = Path(os.environ.get("TEMP", ".")) / "adv11b-visual3"


def chunk(tag, data):
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)


def solid(width, height, rgba=(0, 0, 0, 255)):
    raw = b"".join(b"\x00" + bytes(rgba) * width for _ in range(height))
    body = chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")
    return b"\x89PNG\r\n\x1a\n" + body


def run(args, config, artifacts):
    env = dict(os.environ)
    env.pop("BT_CONFIG", None)
    env.pop("BT_ARTIFACTS", None)
    env["BT_CONFIG"] = str(config)
    env["BT_ARTIFACTS"] = str(artifacts)
    return subprocess.run([str(EXE), *args], capture_output=True, text=True, env=env, timeout=60)


d = WORK
if d.exists():
    import shutil
    shutil.rmtree(d)
d.mkdir(parents=True)
cfg = d / "config.json"
cfg.write_text('{"visual": {"tolerance": 48}}', encoding="utf-8")
shots, bases, out = d / "shots", d / "bases", d / "out"
shots.mkdir()
bases.mkdir()
(shots / "ảnh-01.png").write_bytes(solid(1, 1))
(bases / "ảnh-01.png").write_bytes(solid(1, 1))
(shots / "plain.png").write_bytes(solid(1, 1))
(bases / "plain.png").write_bytes(solid(1, 1))
(shots / "β-only-shot.png").write_bytes(solid(1, 1))
proc = run([str(shots), str(bases)], cfg, out)
report = (out / "visual" / "report.md").read_text(encoding="utf-8")
print(f"ascii dirs with unicode filenames: exit={proc.returncode}")
for line in report.splitlines():
    if line.startswith("|") and "---" not in line and "pair |" not in line:
        print(repr(line))
listed = sorted(p.name for p in shots.iterdir())
print("filesystem truth:", [repr(n) for n in listed])
sys.exit(0)
