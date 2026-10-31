import hashlib
import json
import os
import shutil
import struct
import subprocess
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
TOUR_ORDER = [
    "menu",
    "drill-card",
    "drill-reps",
    "drill-rate-a",
    "drill-rate-b",
    "memo-intro",
    "memo-stream",
    "memo-recall",
    "memo-review",
    "solve-ready",
    "solve-memo",
    "solve-exec",
    "solve-review",
    "replay",
    "algs",
    "algs-search",
    "case-drill-show",
    "case-drill-grade",
    "stats",
    "settings",
]
TOUR_ORDER_PARENT = [name for name in TOUR_ORDER if name != "replay"]
TIMEOUT_SECS = 300


def sha256_of(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def png_dims(path):
    with open(path, "rb") as handle:
        head = handle.read(33)
    if len(head) < 33 or head[:8] != b"\x89PNG\r\n\x1a\n" or head[12:16] != b"IHDR":
        return None
    width, height = struct.unpack(">II", head[16:24])
    return width, height


def shot_is_distinct(path):
    from PIL import Image

    with Image.open(path) as image:
        raw = image.convert("RGB").tobytes()
    first = raw[:3]
    for at in range(3, len(raw) - 2, 3):
        if raw[at : at + 3] != first:
            return True
    return False


def slug_set(out_dir):
    return sorted(path.stem for path in out_dir.glob("*.png"))


def slug_order_by_creation(out_dir):
    pngs = list(out_dir.glob("*.png"))
    pngs.sort(key=lambda path: (path.stat().st_ctime, path.name))
    return [path.stem for path in pngs]


def hashes(out_dir):
    return {path.stem: sha256_of(path) for path in out_dir.glob("*.png")}


def sizes(out_dir):
    return {path.stem: path.stat().st_size for path in out_dir.glob("*.png")}


def purge(*paths):
    for path in paths:
        if path.is_dir():
            shutil.rmtree(path)


def store_summary(user_dir):
    import sqlite3

    dbs = list(user_dir.rglob("*.db"))
    summary = {"dbs": [str(path.relative_to(user_dir)) for path in dbs], "tables": {}}
    for db in dbs:
        connection = sqlite3.connect(f"file:{db}?mode=ro", uri=True)
        try:
            names = [
                row[0]
                for row in connection.execute(
                    "select name from sqlite_master where type='table' order by name"
                )
            ]
            for name in names:
                count = connection.execute(f'select count(*) from "{name}"').fetchone()[0]
                summary["tables"][f"{db.name}:{name}"] = count
        finally:
            connection.close()
    return summary


def run_tour(exe, cwd, out_dir, user_dir, timeout=TIMEOUT_SECS, keep_out=False, fresh_user=True):
    if out_dir.is_dir() and not keep_out:
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    if fresh_user:
        if user_dir.is_dir():
            shutil.rmtree(user_dir)
        user_dir.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["APPDATA"] = str(user_dir)
    env["BT_CONFIG"] = str(cwd / "config" / "config.json")
    env["BT_REF_GROUND"] = "ref_ground"
    env["BT_ARTIFACTS"] = "artifacts"
    started = time.time()
    failure = None
    try:
        proc = subprocess.run(
            [str(exe), "--usage", str(out_dir)],
            cwd=cwd,
            env=env,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="backslashreplace",
            timeout=timeout,
        )
        code = proc.returncode
        stdout = proc.stdout[-3000:]
        stderr = proc.stderr[-3000:]
    except subprocess.TimeoutExpired:
        code = None
        stdout = ""
        stderr = f"tour exceeded {timeout}s"
        failure = "timeout"
    elapsed = time.time() - started
    result = {
        "exit": code,
        "secs": round(elapsed, 1),
        "failure": failure,
        "stdout_tail": stdout,
        "stderr_tail": stderr,
        "slugs_sorted": slug_set(out_dir),
        "slugs_tour_order": slug_order_by_creation(out_dir),
        "hashes": hashes(out_dir),
        "sizes": sizes(out_dir),
    }
    return result


def write_manifest(name, payload):
    target = Path(__file__).resolve().parent / name
    with open(target, "w", encoding="utf-8") as handle:
        json.dump(payload, handle, indent=1, sort_keys=True)
    return target
