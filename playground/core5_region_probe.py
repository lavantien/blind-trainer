import json
import subprocess
import sys

lane = "artifacts/lane/lane-cov-core"
exe = f"{lane}/cov-build/bin/core_moves_test.exe"
proc = subprocess.run(
    ["llvm-cov", "export", f"--instr-profile={lane}/deathcheck/prof.profdata", exe],
    capture_output=True, text=True, encoding="utf-8", errors="backslashreplace",
)
if proc.returncode != 0:
    sys.stderr.write(proc.stderr[-2000:])
    raise SystemExit("llvm-cov failed")
data = json.loads(proc.stdout)["data"][0]
for entry in data["files"]:
    if "moves.c" not in entry["filename"]:
        continue
    regions = entry["summary"]["regions"]
    print(f"moves.c {regions['covered']}/{regions['count']}")
    dark = []
    for seg in entry.get("segments", []):
        line, col, count, has_count, is_region = seg[0], seg[1], seg[2], seg[3], seg[4]
        if has_count and is_region and count == 0:
            dark.append(f"{line}:{col}")
    print("dark:", ", ".join(dark) if dark else "none")
