import os
import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[2]
clang = Path("C:/Program Files/LLVM/bin/clang.exe")
flags = [
    "-target",
    "x86_64-pc-windows-msvc",
    "-std=c23",
    "-Wall",
    "-Wextra",
    "-Werror",
    "-ffp-contract=off",
    "-g0",
    "-D_CRT_SECURE_NO_WARNINGS",
    "-O2",
    "-DNDEBUG",
]
asan_flags = ["-fsanitize=address", "-O1", "-g"]
linker = ["-fuse-ld=lld-link", "-Xlinker", "/SUBSYSTEM:CONSOLE", "-Xlinker", "/OPT:REF",
          "-Xlinker", "/OPT:ICF", "-Xlinker", "/DEBUG:NONE", "-Xlinker", "/brepro"]
obj_root = next((root / "build/obj").iterdir())

mode = sys.argv[1] if len(sys.argv) > 1 else "trace"
if mode not in ("trace", "asan"):
    raise SystemExit("usage: trace_build.py [trace|asan]")

tool_sources = sorted(p for p in (root / "tools/gendata").glob("*.c") if p.name != "main.c")
test_source = root / "tests/unit/tools/gendata_build_test.c"
main_source = root / "playground/gendata/trace_main.c"

out_dir = root / "build/test-tmp/gdverify"
obj_dir = out_dir / mode
obj_dir.mkdir(parents=True, exist_ok=True)

active_flags = flags if mode == "trace" else [f for f in flags if f not in ("-O2", "-g0")] + asan_flags

for source in [*tool_sources, test_source, main_source]:
    obj = obj_dir / (source.stem + ".obj")
    argv = [str(clang), *active_flags, "-I", str(root / "src"), "-I", str(root / "tools"), "-I",
            str(root / "tests"), "-isystem", str(root / "vendor/sqlite"), "-c", str(source), "-o",
            str(obj)]
    proc = subprocess.run(argv, cwd=root, capture_output=True, text=True)
    if proc.returncode != 0:
        sys.stderr.write(f"[compile-fail] {source.name}\n{proc.stdout}{proc.stderr}")
        raise SystemExit(2)

closure = []
for area in ("common", "core", "scheme", "data", "puzzles"):
    closure.extend(sorted((obj_root / "src" / area).rglob("*.obj")))
sqlite = sorted((obj_root / "vendor").rglob("sqlite3.obj"))

exe = out_dir / f"trace_build_{mode}.exe"
argv = [str(clang), *active_flags, *linker, str(obj_dir / "trace_main.obj"),
        str(obj_dir / "gendata_build_test.obj"),
        *[str(p) for p in obj_dir.glob("*.obj") if p.stem != "trace_main" and
          p.stem != "gendata_build_test" and p.stem != "main"],
        *map(str, closure), *map(str, sqlite), "-o", str(exe)]
proc = subprocess.run(argv, cwd=root, capture_output=True, text=True)
if proc.returncode != 0:
    sys.stderr.write(proc.stdout + proc.stderr)
    raise SystemExit(2)

env = dict(os.environ)
env["BT_CONFIG"] = str(root / "config/config.json")
env["BT_REF_GROUND"] = str(root / "ref_ground")
env["BT_ARTIFACTS"] = str(root / "artifacts")
if mode == "asan":
    runtime = sorted((Path(clang).parent.parent / "lib" / "clang").glob("*/lib/windows"))
    if runtime:
        env["PATH"] = str(runtime[-1]) + os.pathsep + env["PATH"]

run = subprocess.run([str(exe)], cwd=root, capture_output=True, text=True, env=env)
sys.stdout.write(run.stdout)
sys.stderr.write(run.stderr)
print(f"[{run.returncode}] trace_build_{mode}")
