import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
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
linker = ["-fuse-ld=lld-link", "-Xlinker", "/SUBSYSTEM:CONSOLE", "-Xlinker", "/OPT:REF",
          "-Xlinker", "/OPT:ICF", "-Xlinker", "/DEBUG:NONE", "-Xlinker", "/brepro"]
obj_root = next((root / "build/obj").iterdir())

SUITES = [
    "tools_gendata_audit_test",
    "tools_gendata_build_test",
    "tools_gendata_determinism_test",
    "tools_gendata_hash_test",
    "tools_gendata_lib_test",
    "tools_gendata_manifest_pins_test",
    "tools_gendata_model_test",
    "tools_gendata_out_test",
    "tools_gendata_scoring_test",
    "tools_gendata_synth_test",
    "tools_gendata_vendor_pins_test",
    "tools_gendata_when_test",
]

tool_sources = sorted((root / "tools/gendata").glob("*.c"))
test_sources = sorted((root / "tests/unit/tools").glob("gendata_*.c"))
main_sources = [root / "build/gen" / f"{suite}_main.c" for suite in SUITES]
missing = [str(p) for p in [*tool_sources, *test_sources, *main_sources] if not p.exists()]
if missing:
    sys.stderr.write("missing sources:\n" + "\n".join(missing) + "\n")
    raise SystemExit(2)

out_dir = root / "build/test-tmp/gdverify"
obj_dir = out_dir / "obj"
obj_dir.mkdir(parents=True, exist_ok=True)
fresh = [*tool_sources, *test_sources, *main_sources]


def compile_one(source):
    obj = obj_dir / (source.stem + ".obj")
    argv = [str(clang), *flags, "-I", str(root / "src"), "-I", str(root / "tools"), "-I",
            str(root / "tests"), "-isystem", str(root / "vendor/sqlite"), "-c", str(source), "-o",
            str(obj)]
    proc = subprocess.run(argv, cwd=root, capture_output=True, text=True)
    return source.name, obj, proc


with ThreadPoolExecutor(max_workers=8) as pool:
    builds = list(pool.map(compile_one, fresh))
for name, _, proc in builds:
    if proc.returncode != 0:
        sys.stderr.write(f"[compile-fail] {name}\n{proc.stdout}{proc.stderr}")
        raise SystemExit(2)
obj_by_name = {name: obj for name, obj, _ in builds}

closure = []
for area in ("common", "core", "scheme", "data", "puzzles"):
    closure.extend(sorted((obj_root / "src" / area).rglob("*.obj")))
sqlite = sorted((obj_root / "vendor").rglob("sqlite3.obj"))
tool_objs = [obj for name, obj, _ in builds if not name.endswith("_main.c") and name != "main.c"]

env = dict(os.environ)
env["BT_CONFIG"] = str(root / "config/config.json")
env["BT_REF_GROUND"] = str(root / "ref_ground")
env["BT_ARTIFACTS"] = str(root / "artifacts")

failures = 0
for suite in SUITES:
    test_obj = obj_by_name[f"{suite.replace('tools_', '')}.c"]
    main_obj = obj_by_name[f"{suite}_main.c"]
    exe = out_dir / f"{suite}.exe"
    argv = [str(clang), *flags, *linker, str(test_obj), str(main_obj), *map(str, tool_objs),
            *map(str, closure), *map(str, sqlite), "-o", str(exe)]
    proc = subprocess.run(argv, cwd=root, capture_output=True, text=True)
    if proc.returncode != 0:
        sys.stderr.write(proc.stdout + proc.stderr)
        print(f"[link-fail] {suite}")
        failures += 1
        continue
    run = subprocess.run([str(exe)], cwd=root, capture_output=True, text=True, env=env)
    tail = (run.stdout + run.stderr).strip().splitlines()
    print(f"[{run.returncode}] {suite}: {tail[-1] if tail else 'no output'}")
    if run.returncode != 0:
        failures += 1
        for line in tail[:20]:
            print("   ", line)
raise SystemExit(1 if failures else 0)
