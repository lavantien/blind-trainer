import os
import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[2]
clang = Path("C:/Program Files/LLVM/bin/clang.exe")
flags = [
    "-target", "x86_64-pc-windows-msvc", "-std=c23", "-Wall", "-Wextra", "-Werror",
    "-ffp-contract=off", "-g0", "-D_CRT_SECURE_NO_WARNINGS", "-O2", "-DNDEBUG",
]
linker = ["-fuse-ld=lld-link", "-Xlinker", "/SUBSYSTEM:CONSOLE", "-Xlinker", "/OPT:REF",
          "-Xlinker", "/OPT:ICF", "-Xlinker", "/DEBUG:NONE", "-Xlinker", "/brepro"]
obj_root = next((root / "build/obj").iterdir())

obj_dir = root / "build/test-tmp/gdverify/time"
obj_dir.mkdir(parents=True, exist_ok=True)
sources = [p for p in sorted((root / "tools/gendata").glob("*.c")) if p.name != "main.c"]
sources.append(root / "tests/unit/tools/gendata_synth_test.c")
sources.append(root / "playground/gendata/time_synth_main.c")
for source in sources:
    obj = obj_dir / (source.stem + ".obj")
    argv = [str(clang), *flags, "-I", str(root / "src"), "-I", str(root / "tools"), "-I",
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
exe = root / "build/test-tmp/gdverify/time_synth.exe"
argv = [str(clang), *flags, *linker, *[str(obj_dir / (p.stem + ".obj")) for p in sources],
        *map(str, closure), *map(str, sqlite), "-o", str(exe)]
proc = subprocess.run(argv, cwd=root, capture_output=True, text=True)
if proc.returncode != 0:
    sys.stderr.write(proc.stdout + proc.stderr)
    raise SystemExit(2)

env = dict(os.environ)
env["BT_CONFIG"] = str(root / "config/config.json")
env["BT_REF_GROUND"] = str(root / "ref_ground")
env["BT_ARTIFACTS"] = str(root / "artifacts")
run = subprocess.run([str(exe)], cwd=root, capture_output=True, text=True, env=env)
sys.stderr.write(run.stderr)
print(f"[{run.returncode}] time_synth")
