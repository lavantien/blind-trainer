import json
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
LANE = REPO / "artifacts" / "lane" / "lane-cov-verify"
LLVM = Path(r"C:\Program Files\LLVM\bin")
SUITE = Path(LANE / "build" / "bin_cov" / "tools_release_verify_oom_test.exe")
TARGET = "tools/release-verify/exec.c"
WANTED = {94, 95, 96, 97}

for raw in sorted((LANE / "prof-release").glob("*.profraw")):
    profdata = LANE / "one.profdata"
    subprocess.run(
        [str(LLVM / "llvm-profdata.exe"), "merge", f"--output={profdata}", str(raw)],
        capture_output=True,
    )
    proc = subprocess.run(
        [
            str(LLVM / "llvm-cov.exe"),
            "export",
            f"--instr-profile={profdata}",
            str(SUITE),
        ],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="backslashreplace",
    )
    if proc.returncode != 0:
        print(raw.name, "export failed")
        continue
    data = json.loads(proc.stdout)["data"][0]
    for entry in data["files"]:
        rel = str(entry["filename"]).replace("\\", "/")
        prefix = str(REPO).replace("\\", "/") + "/"
        if rel.lower().startswith(prefix.lower()):
            rel = rel[len(prefix):]
        if rel != TARGET:
            continue
        hits = []
        for seg in entry.get("segments", []):
            line, count, has_count = seg[0], seg[2], seg[3]
            is_gap = len(seg) > 5 and bool(seg[5])
            if has_count and not is_gap and line in WANTED and count > 0:
                hits.append((line, count))
        total = entry["summary"]["regions"]["covered"]
        print(raw.name, f"covered={total}", "line-hits:", hits)
