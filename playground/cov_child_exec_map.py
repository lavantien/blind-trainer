import json
import subprocess
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
LANE = REPO / "artifacts" / "lane" / "lane-cov-verify"
LLVM = Path(r"C:\Program Files\LLVM\bin")
SUITE = Path(LANE / "build" / "bin_cov" / "tools_release_verify_oom_test.exe")
TARGET = "tools/release-verify/exec.c"
SMALL = ["14520", "16804", "2600", "33052", "34508", "9208",
         "11248", "28444", "31872", "34696", "36048", "8040"]

for name in SMALL:
    raw = LANE / "prof-release" / f"{name}.profraw"
    profdata = LANE / "one.profdata"
    subprocess.run(
        [str(LLVM / "llvm-profdata.exe"), "merge", f"--output={profdata}", str(raw)],
        capture_output=True,
    )
    proc = subprocess.run(
        [str(LLVM / "llvm-cov.exe"), "export", f"--instr-profile={profdata}", str(SUITE)],
        capture_output=True, text=True, encoding="utf-8", errors="backslashreplace",
    )
    data = json.loads(proc.stdout)["data"][0]
    for entry in data["files"]:
        rel = str(entry["filename"]).replace("\\", "/")
        prefix = str(REPO).replace("\\", "/") + "/"
        if rel.lower().startswith(prefix.lower()):
            rel = rel[len(prefix):]
        if rel != TARGET:
            continue
        seen = set()
        lines = []
        for seg in entry.get("segments", []):
            line, col, count, has_count = seg[0], seg[1], seg[2], seg[3]
            is_gap = len(seg) > 5 and bool(seg[5])
            if has_count and not is_gap and count > 0 and (line, col) not in seen:
                seen.add((line, col))
                lines.append(f"{line}:{col}x{count}")
        print(name, " ".join(lines) if lines else "no exec.c coverage")
