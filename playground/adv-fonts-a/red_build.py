import hashlib
import os
import shutil
import subprocess
import sys
import tarfile
import time
from pathlib import Path
from PIL import Image

REPO = Path(__file__).resolve().parents[2]
SCRATCH = REPO / "playground/adv-fonts-a/redbuild"
PARENT = "f53951f^"
FIXED = "f53951f"
CLANG = shutil.which("clang")
FLAGS = [
    "-target", "x86_64-pc-windows-msvc", "-std=c23", "-Wall", "-Wextra", "-Werror",
    "-ffp-contract=off", "-g0", "-D_CRT_SECURE_NO_WARNINGS", "-O2", "-DNDEBUG",
]
INCLUDES = [
    "-I", str(REPO / "src"), "-I", str(REPO / "tools"), "-I", str(REPO / "tests"),
    "-isystem", str(REPO / "vendor/sqlite"),
    "-isystem", str(REPO / "vendor/raylib/src"),
    "-isystem", str(REPO / "vendor/raylib/src/external/glfw/include"),
]
OBJ_HASH = "70dcf3245464"
OBJ = REPO / "build/obj" / OBJ_HASH
RAYLIB = REPO / "build/vendor/raylib.lib"
LINK_FLAGS = ["-fuse-ld=lld-link"]
for flag in ["/SUBSYSTEM:CONSOLE", "/OPT:REF", "/OPT:ICF", "/DEBUG:NONE", "/brepro"]:
    LINK_FLAGS += ["-Xlinker", flag]
LIBS = ["-lopengl32", "-lgdi32", "-luser32", "-lshell32", "-lwinmm"]
SUITES = {
    "full_red": ["src/app/views/solve.c", "src/app/views/algs/mod.c", "src/app/views/case_drill_view.c"],
    "solve_only": ["src/app/views/solve.c"],
    "algs_only": ["src/app/views/algs/mod.c"],
    "case_only": ["src/app/views/case_drill_view.c"],
}
BG = (0, 0, 0)
REGIONS = {
    "solve.png": (310, 95, 630, 460),
    "algs.png_rows": (25, 95, 250, 460),
    "algs.png_detail": (265, 95, 630, 460),
    "case_drill.png": (25, 92, 430, 200),
}


def extract(commit, dest):
    proc = subprocess.run(["git", "-C", str(REPO), "archive", commit],
                          capture_output=True, check=True)
    with tarfile.open(fileobj=__import__("io").BytesIO(proc.stdout), mode="r:*") as tar:
        tar.extractall(dest, filter="data")


def git_show(commit, rel, dest):
    blob = subprocess.run(["git", "-C", str(REPO), "show", f"{commit}:{rel}"],
                          capture_output=True, check=True).stdout
    dest.write_bytes(blob)


def compile_one(source, obj):
    argv = [CLANG, *FLAGS, *INCLUDES, "-c", str(source), "-o", str(obj), "-MD", "-MF",
            str(obj.with_suffix(".d"))]
    proc = subprocess.run(argv, capture_output=True, text=True)
    if proc.returncode != 0:
        print(proc.stdout, proc.stderr)
        raise SystemExit(f"compile failed: {source}")
    with obj.open("r+b") as handle:
        handle.seek(4)
        handle.write(b"\x00\x00\x00\x00")


def collect_shared_objs():
    objs = []
    for path in sorted((OBJ / "src").rglob("*.obj")):
        if path.name == "main.obj":
            continue
        objs.append(path)
    for path in sorted((OBJ / "vendor").rglob("*.obj")):
        objs.append(path)
    return objs


def build_variant(tag, parent_sources):
    work = SCRATCH / tag
    if work.exists():
        shutil.rmtree(work)
    src_root = SCRATCH / "tree"
    if not src_root.exists():
        extract(PARENT, src_root)
        fixed_test = src_root / "tests/unit/app/view_draw_b_test.c"
        git_show(FIXED, "tests/unit/app/view_draw_b_test.c", fixed_test)
    obj_dir = work / "obj"
    obj_dir.mkdir(parents=True)
    override = {}
    for rel in parent_sources:
        local = obj_dir / Path(rel).name + ".obj" if False else obj_dir / (Path(rel).stem + ".obj")
        compile_one(src_root / rel, local)
        override[rel] = local
    test_obj = obj_dir / "view_draw_b_test.obj"
    compile_one(src_root / "tests/unit/app/view_draw_b_test.c", test_obj)
    main_obj = OBJ / "gen/app_view_draw_b_test_main.obj"
    if not main_obj.is_file():
        raise SystemExit("shared gen main obj missing")
    objects = []
    swapped = []
    for path in collect_shared_objs():
        rel = str(path.relative_to(OBJ).with_suffix("")) + ".c"
        rel = rel.replace("\\", "/")
        if rel in override:
            objects.append(override[rel])
            swapped.append(rel)
        else:
            objects.append(path)
    print(f"[{tag}] substituted parent sources: {swapped}")
    objects += [test_obj, main_obj, RAYLIB]
    exe = work / "app_view_draw_b_test.exe"
    argv = [CLANG, *FLAGS, *LINK_FLAGS, *[str(o) for o in objects], *LIBS, "-o", str(exe)]
    proc = subprocess.run(argv, capture_output=True, text=True)
    if proc.returncode != 0:
        print(proc.stdout[-3000:], proc.stderr[-3000:])
        raise SystemExit(f"link failed for {tag}")
    print(f"built {exe}")
    return exe, work


def run_suite(exe, tag):
    env = dict(os.environ)
    env["BT_REF_GROUND"] = "ref_ground"
    env["BT_CONFIG"] = str(REPO / "config/config.json")
    env["BT_ARTIFACTS"] = "artifacts"
    proc = subprocess.run([str(exe)], cwd=str(REPO), env=env, capture_output=True,
                          text=True, timeout=600)
    print(f"[{tag}] exit={proc.returncode}")
    for line in (proc.stdout or "").splitlines():
        if "view_draw_b" in line or "fail" in line.lower() or "pass" in line.lower():
            print("   ", line)
    return proc.returncode


def measure(tag):
    print(f"[{tag}] lit measurements")
    base = Path(os.environ.get("TEMP", ".")) / "bt-app-view_draw_b-tests"
    shots = {}
    for root, _, files in os.walk(base):
        for name in files:
            if name.endswith(".png"):
                shots.setdefault(name, []).append(Path(root) / name)
    for name in sorted(shots):
        for path in sorted(shots[name], key=lambda p: p.stat().st_mtime)[-1:]:
            img = Image.open(path).convert("RGB")
            px = img.load()
            for key, (x0, y0, x1, y1) in REGIONS.items():
                if not key.startswith(name):
                    continue
                lit = 0
                for y in range(y0, min(y1, img.height)):
                    for x in range(x0, min(x1, img.width)):
                        if px[x, y] != (0, 0, 0):
                            lit += 1
                print(f"    {key} lit={lit}")


def main():
    SCRATCH.mkdir(parents=True, exist_ok=True)
    for tag, sources in SUITES.items():
        exe, work = build_variant(tag, sources)
        code = run_suite(exe, tag)
        measure(tag)
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
