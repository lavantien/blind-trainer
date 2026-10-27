import os
import struct
import subprocess
import sys
import zlib
from pathlib import Path

REPO = Path(r"C:\Users\lavantien\dev\github\blind-trainer")
EXE = REPO / "build" / "bin" / "visual-diff.exe"
WORK = Path(os.environ.get("TEMP", ".")) / "adv11b-visual2"


def chunk(tag, data):
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)


def png_bytes(width, height, rows, colortype=6, bitdepth=8, bad_crc=False, extra_idat=b""):
    ihdr = struct.pack(">IIBBBBB", width, height, bitdepth, colortype, 0, 0, 0)
    raw = b"".join(b"\x00" + row for row in rows)
    idat = zlib.compress(raw) + extra_idat
    body = chunk(b"IHDR", ihdr) + chunk(b"IDAT", idat) + chunk(b"IEND", b"")
    if bad_crc:
        body = body[:-4] + b"\x00\x00\x00\x00"
    return b"\x89PNG\r\n\x1a\n" + body


def solid(width, height, rgba=(0, 0, 0, 255), **kw):
    return png_bytes(width, height, [bytes(rgba) * width] * height, **kw)


def run(args, config, artifacts):
    env = dict(os.environ)
    env.pop("BT_CONFIG", None)
    env.pop("BT_ARTIFACTS", None)
    if config is not None:
        env["BT_CONFIG"] = str(config)
    if artifacts is not None:
        env["BT_ARTIFACTS"] = str(artifacts)
    return subprocess.run([str(EXE), *args], capture_output=True, text=True, env=env, timeout=60)


def fresh(tag):
    d = WORK / tag
    if d.exists():
        import shutil
        shutil.rmtree(d)
    d.mkdir(parents=True)
    return d


print("=== A: which corrupt png silently decodes ===")
d = fresh("corrupt")
cfg = d / "config.json"
cfg.write_text('{"visual": {"tolerance": 48}}', encoding="utf-8")
shots, bases = d / "s", d / "b"
shots.mkdir()
bases.mkdir()
cases = {
    "empty.png": b"",
    "junk.png": b"not a png" * 8,
    "trunc.png": solid(2, 1)[: len(solid(2, 1)) // 2],
    "badcrc.png": solid(1, 1, bad_crc=True),
    "zerodim.png": png_bytes(0, 1, []),
    "gray.png": png_bytes(2, 1, [b"\x80\x80"], colortype=0),
    "gray_alpha.png": png_bytes(2, 1, [b"\x80\x40\x80\xff"], colortype=4),
    "palette.png": png_bytes(2, 1, [b"\x00\x01"], colortype=3, extra_idat=b""),
}
for name, data in cases.items():
    (shots / name).write_bytes(data)
    (bases / name).write_bytes(data)
proc = run([str(shots), str(bases)], cfg, d)
report = (d / "visual" / "report.md").read_text(encoding="utf-8")
for line in report.splitlines():
    if line.startswith("|") and "---" not in line and "pair |" not in line:
        print(line)

print("\n=== B: update into a blocked path with --update ===")
d = fresh("blocked")
cfg = d / "config.json"
cfg.write_text('{"visual": {"tolerance": 48}}', encoding="utf-8")
shots = d / "s"
shots.mkdir()
(shots / "fresh.png").write_bytes(solid(1, 1))
blocker = d / "blocker"
blocker.write_text("file not dir", encoding="utf-8")
proc = run([str(shots), str(blocker), "--update"], cfg, d)
print(f"exit={proc.returncode} err={proc.stderr.strip()}")

print("\n=== C: unicode names with pinned artifacts ===")
d = fresh("uni")
cfg = d / "config.json"
cfg.write_text('{"visual": {"tolerance": 48}}', encoding="utf-8")
shots, bases, out = d / "ảnh chụp", d / "cơ sở", d / "out"
shots.mkdir()
bases.mkdir()
for name in ["ảnh-01.png", "scène β.png"]:
    (shots / name).write_bytes(solid(1, 1))
    (bases / name).write_bytes(solid(1, 1))
proc = run([str(shots), str(bases)], cfg, out)
report = (out / "visual" / "report.md").read_text(encoding="utf-8") if (out / "visual" / "report.md").is_file() else "<missing>"
print(f"exit={proc.returncode}")
for line in report.splitlines():
    if line.startswith("|") and "---" not in line and "pair |" not in line:
        print(repr(line))

print("\n=== D: empty artifacts string, where does the report land ===")
d = fresh("emptyart")
cfg = d / "config.json"
cfg.write_text('{"visual": {"tolerance": 48}, "paths": {"artifacts": ""}}', encoding="utf-8")
shots, bases = d / "s", d / "b"
shots.mkdir()
bases.mkdir()
(shots / "x.png").write_bytes(solid(1, 1))
(bases / "x.png").write_bytes(solid(1, 1))
proc = run([str(shots), str(bases)], cfg, None)
report_line = [l for l in proc.stdout.splitlines() if l.startswith("report:")]
print(f"exit={proc.returncode} report_line={report_line} repo_report_exists={(REPO / 'artifacts' / 'visual' / 'report.md').is_file()}")

print("\n=== E: round trip byte identity through export and reload ===")
d = fresh("rt")
cfg = d / "config.json"
cfg.write_text('{"visual": {"tolerance": 0}}', encoding="utf-8")
shots, bases = d / "s", d / "b"
shots.mkdir()
bases.mkdir()
src = solid(3, 2, (10, 200, 30, 255))
(shots / "rt.png").write_bytes(src)
(bases / "rt.png").write_bytes(src)
proc = run([str(shots), str(bases)], cfg, d)
report = (d / "visual" / "report.md").read_text(encoding="utf-8")
print(f"identical python-written png pair: exit={proc.returncode} line={[l for l in report.splitlines() if l.startswith('| rt')]}")
sys.exit(0)
