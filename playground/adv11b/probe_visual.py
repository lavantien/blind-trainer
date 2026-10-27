import os
import struct
import subprocess
import sys
import zlib
from pathlib import Path

REPO = Path(r"C:\Users\lavantien\dev\github\blind-trainer")
EXE = REPO / "build" / "bin" / "visual-diff.exe"
WORK = Path(os.environ.get("TEMP", ".")) / "adv11b-visual"
CRASH = "3221225477"


def chunk(tag, data):
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)


def png_bytes(width, height, rows, colortype=6, bitdepth=8, extra_idat=b"", bad_crc=False):
    ihdr = struct.pack(">IIBBBBB", width, height, bitdepth, colortype, 0, 0, 0)
    raw = b"".join(b"\x00" + row for row in rows)
    idat = zlib.compress(raw) + extra_idat
    body = chunk(b"IHDR", ihdr) + chunk(b"IDAT", idat) + chunk(b"IEND", b"")
    if bad_crc:
        body = body[:-4] + b"\x00\x00\x00\x00"
    return b"\x89PNG\r\n\x1a\n" + body


def solid(width, height, rgba=(0, 0, 0, 255), bad_crc=False):
    row = bytes(rgba) * width
    return png_bytes(width, height, [row] * height, bad_crc=bad_crc)


def run(args, config, artifacts, cwd=None):
    env = dict(os.environ)
    env.pop("BT_CONFIG", None)
    env.pop("BT_ARTIFACTS", None)
    if config is not None:
        env["BT_CONFIG"] = str(config)
    if artifacts is not None:
        env["BT_ARTIFACTS"] = str(artifacts)
    proc = subprocess.run([str(EXE), *args], capture_output=True, text=True, env=env, cwd=cwd, timeout=60)
    return proc.returncode, proc.stdout, proc.stderr


def fresh(tag):
    d = WORK / tag
    if d.exists():
        import shutil
        shutil.rmtree(d)
    d.mkdir(parents=True)
    return d


def write_cfg(root, body):
    root.mkdir(parents=True, exist_ok=True)
    p = root / "config.json"
    p.write_text(body, encoding="utf-8")
    return p


def report_text(artifacts):
    p = Path(artifacts) / "visual" / "report.md"
    return p.read_text(encoding="utf-8") if p.is_file() else "<no report>"


results = []


def check(name, condition, detail=""):
    results.append((name, "PASS" if condition else "FAIL", detail))
    print(f"{name}: {'PASS' if condition else 'FAIL'} {detail}")


d = fresh("crash")
cfg = write_cfg(d, '{"visual": {"tolerance": 48}}')
shots, bases = d / "shots", d / "bases"
shots.mkdir()
bases.mkdir()
solid_ok = solid(2, 1)
(shots / "ok.png").write_bytes(solid_ok)
(bases / "ok.png").write_bytes(solid_ok)
(bases / "empty.png").write_bytes(b"")
(bases / "junk.png").write_bytes(b"not a png at all" * 10)
good = solid(2, 1)
(bases / "trunc.png").write_bytes(good[: len(good) // 2])
(bases / "badcrc.png").write_bytes(solid(1, 1, bad_crc=True))
(bases / "zerodim.png").write_bytes(png_bytes(0, 1, []))
(bases / "onepx.png").write_bytes(solid(1, 1))
(shots / "onepx.png").write_bytes(solid(1, 1))
(shots / "empty.png").write_bytes(b"")
(shots / "junk.png").write_bytes(b"not a png at all" * 10)
(shots / "trunc.png").write_bytes(good[: len(good) // 2])
(shots / "badcrc.png").write_bytes(solid(1, 1, bad_crc=True))
(shots / "zerodim.png").write_bytes(png_bytes(0, 1, []))
code, out, err = run([str(shots), str(bases)], cfg, d)
text = report_text(d)
rows = [line for line in text.splitlines() if line.startswith("|") and "---" not in line and "pair |" not in line]
check("junk inputs no crash", code in (0, 1, 2) and code != int(CRASH) if isinstance(code, int) else False, f"exit={code}")
check("decode failures are verdicts not aborts", any("decode failed on" in r for r in rows), rows[2] if len(rows) > 2 else text)
check("junk pair count", "9 pairs, 2 ok, 7 failed" in text, text.strip().splitlines()[-1] if text.strip() else "")

d = fresh("determinism")
cfg = write_cfg(d, '{"visual": {"tolerance": 48}}')
shots, bases = d / "s", d / "b"
shots.mkdir()
bases.mkdir()
for name in ["m.png", "a.png", "z.png", "B.png", "0.png"]:
    (shots / name).write_bytes(solid(1, 1))
    (bases / name).write_bytes(solid(1, 1))
r1 = run([str(shots), str(bases)], cfg, d)
t1 = report_text(d)
(d / "visual2").mkdir()
r2 = run([str(shots), str(bases)], cfg, d)
t2 = report_text(d)
check("report deterministic across runs", t1 == t2 and r1[0] == r2[0] == 0, f"exit={r1[0]}/{r2[0]}")
sort_rows = [l.split("|")[1].strip() for l in t1.splitlines() if l.startswith("|") and "---" not in l and "pair |" not in l]
check("names byte sorted", sort_rows == ["0.png", "B.png", "a.png", "m.png", "z.png"], str(sort_rows))

d = fresh("env-edges")
missing_cfg = d / "nope.json"
code, out, err = run([str(d / "a"), str(d / "b")], missing_cfg, d)
check("missing config exits 2", code == 2 and "cannot read config" in err, f"exit={code} err={err.strip()[:80]}")
bad_cfg = write_cfg(d / "bad", "{not json")
code, out, err = run([str(d / "a"), str(d / "b")], bad_cfg, d)
check("malformed config exits 2", code == 2 and "cannot parse config" in err, f"exit={code} err={err.strip()[:80]}")
no_tol = write_cfg(d / "notol", '{"app": {}}')
code, out, err = run([str(d / "a"), str(d / "b")], no_tol, d)
check("tolerance missing exits 2", code == 2 and "config visual.tolerance missing" in err, f"exit={code}")
tol_256 = write_cfg(d / "t256", '{"visual": {"tolerance": 256}}')
code, out, err = run([str(d / "a"), str(d / "b")], tol_256, d)
check("tolerance 256 exits 2", code == 2 and "0..=255" in err, f"exit={code}")
tol_str = write_cfg(d / "tstr", '{"visual": {"tolerance": "8"}}')
code, out, err = run([str(d / "a"), str(d / "b")], tol_str, d)
check("tolerance string exits 2", code == 2 and "0..=255" in err, f"exit={code}")
tol_float = write_cfg(d / "tfloat", '{"visual": {"tolerance": 8.5}}')
code, out, err = run([str(d / "a"), str(d / "b")], tol_float, d)
check("tolerance 8.5 exits 2", code == 2 and "0..=255" in err, f"exit={code}")

d = fresh("artifacts-edges")
cfg = write_cfg(d, '{"visual": {"tolerance": 48}}')
shots, bases = d / "s", d / "b"
shots.mkdir()
bases.mkdir()
(shots / "x.png").write_bytes(solid(1, 1))
(bases / "x.png").write_bytes(solid(1, 1))
blocker = d / "out"
blocker.write_text("i am a file", encoding="utf-8")
code, out, err = run([str(shots), str(bases)], cfg, blocker)
check("artifacts path blocked exits 2", code == 2 and "cannot write report" in err, f"exit={code} err={err.strip()[:90]}")
empty_art = write_cfg(d / "emptyart", '{"visual": {"tolerance": 48}, "paths": {"artifacts": ""}}')
code, out, err = run([str(shots), str(bases)], empty_art, None)
check("empty artifacts string no crash", code in (0, 1, 2), f"exit={code} out={out.strip().splitlines()[0] if out.strip() else ''}")

d = fresh("cli")
code, out, err = run([], None, None)
check("no args usage exit 2", code == 2 and "usage:" in err, f"exit={code}")
code, out, err = run(["one"], None, None)
check("one arg usage exit 2", code == 2 and "usage:" in err, f"exit={code}")
code, out, err = run(["a", "b", "c"], None, None)
check("three args usage exit 2", code == 2 and "usage:" in err, f"exit={code}")
code, out, err = run(["wd", "--selftest"], None, None)
check("selftest second token usage exit 2", code == 2 and "usage:" in err, f"exit={code}")
code, out, err = run(["--selftest", "wd", "extra"], None, None)
check("selftest extra arg usage exit 2", code == 2 and "usage:" in err, f"exit={code}")
code, out, err = run(["--selftest", ""], None, None)
check("selftest empty workdir usage exit 2", code == 2 and "usage:" in err, f"exit={code}")
d2 = fresh("selftest-run")
code, out, err = run(["--selftest", str(d2)], None, None)
check("selftest green exit 0", code == 0 and "4/4" in out, f"exit={code} {out.strip().splitlines()[-1] if out.strip() else ''}")
code, out, err = run(["--selftest", str(d2)], None, None)
check("selftest rerun idempotent exit 0", code == 0, f"exit={code}")

d = fresh("update")
cfg = write_cfg(d, '{"visual": {"tolerance": 48}}')
shots, bases = d / "s", d / "b"
shots.mkdir()
bases.mkdir()
(shots / "fresh.png").write_bytes(solid(2, 1, (7, 7, 7, 255)))
(shots / "with space.png").write_bytes(solid(2, 1, (7, 7, 7, 255)))
(bases / "stale.png").write_bytes(solid(1, 1))
(bases / "keep.txt").write_text("x", encoding="utf-8")
code, out, err = run([str(shots), str(bases), "--update"], cfg, d)
check("update mirrors and exits 0", code == 0 and (bases / "fresh.png").is_file() and (bases / "with space.png").is_file() and not (bases / "stale.png").exists() and (bases / "keep.txt").is_file(), f"exit={code} err={err.strip()[:80]}")
check("update prints count then names", out.strip().startswith("updated 2 baselines") and "updated with space.png" in out, out.strip().splitlines()[:1][0] if out.strip() else "")
code, out, err = run([str(shots), str(d / "blocker")], cfg, d)
check("update to blocked path exits 2", code == 2 and "update failed" in err, f"exit={code}")

d = fresh("unicode")
cfg = write_cfg(d, '{"visual": {"tolerance": 48}}')
name = "ảnh-01.png"
shots, bases = d / "ảnh chụp", d / "cơ sở"
shots.mkdir()
bases.mkdir()
(shots / name).write_bytes(solid(1, 1))
(bases / name).write_bytes(solid(1, 1))
code, out, err = run([str(shots), str(bases)], cfg, d)
text = report_text(d)
check("unicode dirs and names no crash", code in (0, 1), f"exit={code}")
check("unicode pair reported ok", "| ảnh-01.png | ok |" in text, text.splitlines()[6] if len(text.splitlines()) > 6 else text)

d = fresh("missing-dirs")
cfg = write_cfg(d, '{"visual": {"tolerance": 48}}')
code, out, err = run([str(d / "no-shots"), str(d / "no-bases")], cfg, d)
text = report_text(d)
check("both dirs missing exit 0 empty report", code == 0 and "0 pairs, 0 ok, 0 failed" in text, f"exit={code} {text.strip().splitlines()[-1] if text.strip() else ''}")
code, out, err = run([str(d / "no-shots")], cfg, d)
check("usage before config read", code == 2 and "usage:" in err, f"exit={code}")

fails = [r for r in results if r[1] == "FAIL"]
print(f"\nprobe summary: {len(results) - len(fails)}/{len(results)} passed")
sys.exit(1 if fails else 0)
