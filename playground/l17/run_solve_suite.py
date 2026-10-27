import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SUITE = "app_view_solve_test"
TEST = ROOT / "tests" / "unit" / "app" / "view_solve_test.c"
MAIN = ROOT / "playground" / "l17" / "app_view_solve_test_main.c"
EXE = ROOT / "build" / f"{SUITE}.exe"

sources = []
for area in ("common", "core", "scheme", "data", "puzzles"):
    sources.extend(sorted((ROOT / "src" / area).rglob("*.c")))
for source in sorted((ROOT / "src" / "app").rglob("*.c")):
    if source.name in ("main.c", "replay.c"):
        continue
    sources.append(source)
sources.append(TEST)
sources.append(MAIN)

includes = ["-I", str(ROOT / "src")]
for include in ("vendor/raylib/src", "vendor/raylib/src/external/glfw/include", "vendor/sqlite"):
    includes += ["-I", str(ROOT / include)]

flags = [
    "-std=c23",
    "-Wall",
    "-Wextra",
    "-ffp-contract=off",
    "-g0",
    "-D_CRT_SECURE_NO_WARNINGS",
]

argv = (
    ["clang", *flags, *includes, *[str(s) for s in sources],
     "-o", str(EXE),
     str(ROOT / "build" / "vendor" / "raylib.lib"),
     str(ROOT / "vendor" / "sqlite" / "sqlite3.c"),
     "-lopengl32", "-lgdi32", "-luser32", "-lshell32", "-lwinmm"]
)
proc = subprocess.run(argv, cwd=ROOT, capture_output=True, text=True)
if proc.returncode != 0:
    sys.stderr.write(proc.stdout + proc.stderr[-4000:])
    raise SystemExit("compile failed")
env = dict(os.environ)
env.setdefault("BT_CONFIG", str(ROOT / "config" / "config.json"))
env.setdefault("BT_ARTIFACTS", str(ROOT / "artifacts"))
env.setdefault("BT_REF_GROUND", str(ROOT / "ref_ground"))
run = subprocess.run([str(EXE)], cwd=ROOT, capture_output=True, text=True, env=env)
sys.stdout.write(run.stdout)
if run.returncode != 0:
    sys.stderr.write(run.stderr[-4000:])
raise SystemExit(run.returncode)
