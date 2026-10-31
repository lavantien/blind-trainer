import json
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent
OBJ = REPO / "build" / "obj"
CU_OBJ_CANDIDATES = list(OBJ.glob("*/src/ga/cuda/ga_eval.obj"))


def snap(tag):
    rows = {
        "tag": tag,
        "roots": sorted(p.name for p in OBJ.iterdir() if p.is_dir()),
        "cu_objs": [
            {
                "root": obj.parents[3].name,
                "mtime": obj.stat().st_mtime,
                "size": obj.stat().st_size,
            }
            for obj in CU_OBJ_CANDIDATES
        ],
        "plain_ga_run": (REPO / "build" / "bin" / "ga-run.exe").stat().st_mtime,
        "cuda_ga_run": (REPO / "build" / "bin-cuda" / "ga-run.exe").stat().st_mtime,
    }
    return rows


def build(args, name):
    started = time.monotonic()
    proc = subprocess.run(
        [sys.executable, "bt.py", *args],
        cwd=str(REPO),
        capture_output=True,
        text=True,
        timeout=1800,
    )
    dt = time.monotonic() - started
    (OUT / f"{name}.log").write_text(proc.stdout + proc.stderr, encoding="utf-8")
    compiled = [ln for ln in proc.stdout.splitlines() if " compiled, " in ln]
    nvcc_lines = [ln for ln in proc.stdout.splitlines() if "nvcc" in ln.lower()]
    skip_cu = [ln for ln in proc.stdout.splitlines() if "ga_eval.cu" in ln]
    links = [ln for ln in proc.stdout.splitlines() if ln.startswith("[link ")]
    return {
        "args": args,
        "code": proc.returncode,
        "secs": round(dt, 1),
        "compiled_line": compiled,
        "nvcc_mentions": len(nvcc_lines),
        "cu_lines": skip_cu,
        "link_count": len(links),
    }


def main():
    flow = {"snaps": [], "builds": []}
    flow["snaps"].append(snap("start"))
    for args, name in (
        (["build"], "b1_plain"),
        (["build", "--cuda"], "b2_cuda"),
        (["build"], "b3_plain"),
        (["build", "--cuda"], "b4_cuda"),
    ):
        flow["builds"].append(build(args, name))
        flow["snaps"].append(snap(name))
    (OUT / "flavor_flow.json").write_text(json.dumps(flow, indent=2), encoding="utf-8")
    print(json.dumps(flow["builds"], indent=2))
    print(json.dumps(flow["snaps"], indent=2))


if __name__ == "__main__":
    main()
