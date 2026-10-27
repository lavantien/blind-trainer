import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SUITES = {
    "app_view_solve_test": ROOT / "tests" / "unit" / "app" / "view_solve_test.c",
    "app_solve_more_test": ROOT / "tests" / "unit" / "app" / "solve_more_test.c",
    "app_views_solve_test": ROOT / "tests" / "unit" / "app" / "views_solve_test.c",
    "app_view_algs_test": ROOT / "tests" / "unit" / "app" / "view_algs_test.c",
    "app_view_case_drill_test": ROOT / "tests" / "unit" / "app" / "view_case_drill_test.c",
    "app_view_case_drill_view_test": ROOT / "tests" / "unit" / "app" / "view_case_drill_view_test.c",
    "app_one_case_drill": ROOT / "tests" / "unit" / "app" / "views_case_drill_test.c",
}

flags = ["-std=c23", "-Wall", "-Wextra", "-ffp-contract=off", "-g0",
         "-D_CRT_SECURE_NO_WARNINGS"]
includes = ["-I", str(ROOT / "src")]
for include in ("vendor/raylib/src", "vendor/raylib/src/external/glfw/include", "vendor/sqlite"):
    includes += ["-I", str(ROOT / include)]

base_sources = []
for area in ("common", "core", "scheme", "data", "puzzles"):
    base_sources.extend(sorted((ROOT / "src" / area).rglob("*.c")))
app_units = ("solve.c", "state.c", "memo.c", "drill.c", "drill_view.c", "drill_card.c", "net.c",
             "theme.c", "ui.c", "window.c", "frame.c", "net_view.c", "logic.c", "persist.c",
             "config.c", "fonts.c", "states.c", "images.c", "case_drill.c", "case_drill_view.c")
for source in sorted((ROOT / "src" / "app").rglob("*.c")):
    if source.name in app_units:
        base_sources.append(source)
for source in sorted((ROOT / "src" / "app" / "views" / "algs").glob("*.c")):
    base_sources.append(source)

env = dict(os.environ)
env.setdefault("BT_CONFIG", str(ROOT / "config" / "config.json"))
env.setdefault("BT_ARTIFACTS", str(ROOT / "artifacts"))
env.setdefault("BT_REF_GROUND", str(ROOT / "ref_ground"))

failures = 0
wanted = sys.argv[1:] or list(SUITES)
for suite in wanted:
    test = SUITES[suite]
    main_c = ROOT / "playground" / "l17" / ("one_case_drill_main.c" if suite == "app_one_case_drill" else f"{suite}_main.c")
    exe = ROOT / "build" / f"{suite}.exe"
    argv = ["clang", *flags, *includes, *[str(s) for s in base_sources], str(test),
            str(main_c), "-o", str(exe),
            str(ROOT / "build" / "vendor" / "raylib.lib"),
            str(ROOT / "vendor" / "sqlite" / "sqlite3.c"),
            "-lopengl32", "-lgdi32", "-luser32", "-lshell32", "-lwinmm"]
    proc = subprocess.run(argv, cwd=ROOT, capture_output=True, text=True)
    if proc.returncode != 0:
        sys.stderr.write((proc.stdout + proc.stderr)[-4000:])
        failures += 1
        continue
    run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True, env=env)
    sys.stdout.write(run.stdout)
    if run.returncode != 0:
        sys.stderr.write(run.stderr[-4000:])
        failures += 1
raise SystemExit(1 if failures else 0)
