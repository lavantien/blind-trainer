import json
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
HUB = REPO / "config" / "config.json"
PROBE = REPO / "playground" / "l15" / "probe.c"
EXE = REPO / "build" / "bin" / "l15-probe.exe"


def run(argv, label):
    print(f"[entry] {label} {' '.join(str(a) for a in argv)}", flush=True)
    proc = subprocess.run([str(a) for a in argv], capture_output=True, text=True)
    tail = ((proc.stderr or "") + (proc.stdout or ""))[-3000:].strip()
    print(f"[exit] {label} code={proc.returncode}", flush=True)
    if tail:
        print(tail, flush=True)
    if proc.returncode != 0:
        raise SystemExit(f"{label} failed with code {proc.returncode}")
    return proc


def main():
    hub = json.loads(HUB.read_text(encoding="utf-8"))
    build = hub["build"]
    ray = build["raylib_build"]
    app = hub["app"]
    includes = [REPO / d for d in ray["include_dirs"]]
    flags = [
        "-target",
        build["target"],
        f"-std={build['std']}",
        *build["cflags"],
        *build["release_flags"],
        *[f"-D{define}" for define in ray["defines"]],
        *ray["extra_flags"],
    ]
    argv = ["clang", *flags]
    for include in includes:
        argv += ["-I", str(include)]
    lib = REPO / ray["lib"]
    shot = REPO / "playground" / "l15" / "probe_shot.png"
    fonts = REPO / hub["paths"]["fonts"]
    link = ["-fuse-ld=lld-link"]
    for flag in build["link_flags"]:
        link += ["-Xlinker", flag]
    argv += [
        str(PROBE),
        str(lib),
        *[f"-l{name}" for name in ray["system_libs"]],
        *link,
        "-o",
        str(EXE),
    ]
    run(argv, "compile probe")
    run(
        [
            EXE,
            str(app["window"]["width"]),
            str(app["window"]["height"]),
            str(app["window"]["title"]),
            str(fonts / "NotoSans-Regular.ttf"),
            str(shot),
        ],
        "run probe",
    )
    if not shot.is_file() or shot.stat().st_size == 0:
        raise SystemExit("probe screenshot missing or empty")
    print(f"[pass] screenshot {shot} size={shot.stat().st_size}", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
