import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[2]
EXE = REPO / "build" / "bin" / "bt-app.exe"
TMP = REPO / "artifacts"
PREFIX = "tmp-adv80a"
RESULTS = Path(__file__).resolve().parent / "drive_results.json"
TIMEOUT_SECS = 300
USAGE_SHOTS = 20
SMOKE_SHOTS = 10
USAGE_NAMES = {
    "algs",
    "algs-search",
    "case-drill-grade",
    "case-drill-show",
    "drill-card",
    "drill-rate-a",
    "drill-rate-b",
    "drill-reps",
    "menu",
    "memo-intro",
    "memo-recall",
    "memo-review",
    "memo-stream",
    "replay",
    "settings",
    "solve-exec",
    "solve-memo",
    "solve-ready",
    "solve-review",
    "stats",
}
FACES = {
    "U": (242, 242, 242),
    "L": (242, 153, 38),
    "F": (51, 191, 89),
    "R": (217, 51, 51),
    "B": (51, 115, 230),
    "D": (242, 217, 38),
}
COLOR_TOLERANCE = 16
MIN_FACE_PIXELS = 2000


def sandbox_env(user_dir):
    env = os.environ.copy()
    env["APPDATA"] = str(user_dir)
    env["BT_CONFIG"] = str(REPO / "config" / "config.json")
    env["BT_REF_GROUND"] = "ref_ground"
    env["BT_ARTIFACTS"] = "artifacts"
    return env


def drive(mode, out_dir, user_dir):
    if out_dir.is_dir():
        shutil.rmtree(out_dir)
    if user_dir.is_dir():
        shutil.rmtree(user_dir)
    out_dir.mkdir(parents=True)
    user_dir.mkdir(parents=True)
    started = time.time()
    proc = subprocess.run(
        [str(EXE), mode, str(out_dir)],
        cwd=REPO,
        env=sandbox_env(user_dir),
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="backslashreplace",
        timeout=TIMEOUT_SECS,
    )
    elapsed = time.time() - started
    return proc, elapsed


def sha256_of(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def png_diff(a_path, b_path):
    from PIL import Image

    with Image.open(a_path) as a_img, Image.open(b_path) as b_img:
        a_raw = a_img.convert("RGB").tobytes()
        b_raw = b_img.convert("RGB").tobytes()
    if len(a_raw) != len(b_raw):
        return None
    diff = 0
    for at in range(0, len(a_raw), 3):
        if a_raw[at : at + 3] != b_raw[at : at + 3]:
            diff += 1
    return diff


def face_census(path):
    from PIL import Image

    with Image.open(path) as image:
        raw = image.convert("RGB").tobytes()
    total = len(raw) // 3
    counts = {name: 0 for name in FACES}
    for at in range(0, len(raw), 3):
        r, g, b = raw[at], raw[at + 1], raw[at + 2]
        for name, (fr, fg, fb) in FACES.items():
            if abs(r - fr) <= COLOR_TOLERANCE and abs(g - fg) <= COLOR_TOLERANCE and abs(b - fb) <= COLOR_TOLERANCE:
                counts[name] += 1
                break
    visible = {name: count for name, count in counts.items() if count >= MIN_FACE_PIXELS}
    return {"total_px": total, "counts": counts, "visible": visible, "visible_count": len(visible)}


def main():
    if not EXE.is_file():
        print(f"FAIL missing {EXE}")
        return 1
    exe_mtime = EXE.stat().st_mtime
    newest_src = 0.0
    newest_name = ""
    for pattern in ("src/**/*.c", "src/**/*.h"):
        for src in REPO.glob(pattern):
            if src.stat().st_mtime > newest_src:
                newest_src = src.stat().st_mtime
                newest_name = str(src.relative_to(REPO))
    print(f"exe mtime {time.ctime(exe_mtime)}, newest src {newest_name} {time.ctime(newest_src)}")
    binary_fresh = exe_mtime >= newest_src
    report = {
        "binary_fresh": binary_fresh,
        "newest_source": newest_name,
        "runs": {},
        "hash_compare": {},
        "replay": {},
    }
    if not binary_fresh:
        print("FAIL binary is older than sources, rebuild first")
        return 1

    code = 0
    pngs = {}
    for label, mode, want in (("usage1", "--usage", USAGE_SHOTS), ("usage2", "--usage", USAGE_SHOTS), ("smoke", "--smoke", SMOKE_SHOTS)):
        out_dir = TMP / f"{PREFIX}-{label}"
        user_dir = TMP / f"{PREFIX}-user-{label}"
        proc, elapsed = drive(mode, out_dir, user_dir)
        names = sorted(p.stem for p in out_dir.glob("*.png"))
        print(f"{label}: exit {proc.returncode} in {elapsed:.1f}s, {len(names)} shots")
        ok = proc.returncode == 0 and len(names) == want
        if not ok and proc.returncode != 0:
            print("stdout tail:", proc.stdout[-1500:])
            print("stderr tail:", proc.stderr[-1500:])
        if mode == "--usage":
            ok = ok and set(names) == USAGE_NAMES
        report["runs"][label] = {
            "exit": proc.returncode,
            "secs": round(elapsed, 1),
            "shots": names,
            "ok": bool(ok),
        }
        if not ok:
            print(f"FAIL {label} run checks")
            code = 1
        pngs[label] = {p.name: sha256_of(p) for p in out_dir.glob("*.png")}

    if "usage1" in pngs and "usage2" in pngs:
        same, differ = [], []
        for name in sorted(pngs["usage1"]):
            if pngs["usage1"][name] == pngs["usage2"].get(name):
                same.append(name)
            else:
                differ.append(name)
        report["hash_compare"] = {"identical": same, "differing": differ}
        print(f"hash identical: {len(same)}/{len(pngs['usage1'])}")
        if differ:
            print(f"hash differing: {differ}")

    usage_replay = TMP / f"{PREFIX}-usage1" / "replay.png"
    smoke_replay = TMP / f"{PREFIX}-smoke" / "replay.png"
    if usage_replay.is_file() and smoke_replay.is_file():
        diff = png_diff(usage_replay, smoke_replay)
        report["replay"]["usage_vs_smoke_diff_px"] = diff
        report["replay"]["usage_vs_smoke_same_hash"] = (
            pngs["usage1"]["replay.png"] == pngs["smoke"].get("replay.png")
        )
        print(f"replay usage vs smoke: {diff} differing pixels")
        report["replay"]["usage_census"] = face_census(usage_replay)
        report["replay"]["smoke_census"] = face_census(smoke_replay)
        print(f"usage replay faces: {report['replay']['usage_census']['visible']}")
        print(f"smoke replay faces: {report['replay']['smoke_census']['visible']}")
        if report["replay"]["usage_census"]["visible_count"] < 4:
            print("FAIL usage replay shows fewer than 4 face colors, cube may be solved")
            code = 1

    with open(RESULTS, "w", encoding="utf-8") as handle:
        json.dump(report, handle, indent=1, sort_keys=True)
    print(f"report written to {RESULTS}")
    return code


if __name__ == "__main__":
    exit_code = main()
    for path in TMP.glob(f"{PREFIX}*"):
        if path.is_dir():
            shutil.rmtree(path)
    print("throwaway dirs purged")
    sys.exit(exit_code)
