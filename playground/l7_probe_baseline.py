import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CFG = json.loads((ROOT / "config/config.json").read_text())
BUILD = CFG["build"]
ENV = json.loads((ROOT / "build/env.json").read_text())
CLANG = ENV["clang"]["path"]
FLAGS = ["-target", BUILD["target"], f"-std={BUILD['std']}"] + BUILD["cflags"] + BUILD["release_flags"]
LINK = ["-fuse-ld=lld-link"] + [part for flag in BUILD["link_flags"] for part in ("-Xlinker", flag)]
OBJROOT = next((ROOT / "build/obj").iterdir())
OUT = ROOT / "build/test-tmp/l7-int/probe"
OUT.mkdir(parents=True, exist_ok=True)

src = OUT / "rng.c"
src.write_text((ROOT / "src/common/rng.c").read_text())
src_objs = sorted((ROOT / "src").rglob("*.c"))
objects = []
for c in src_objs:
    rel = c.relative_to(ROOT).with_suffix(".obj")
    objects.append(OBJROOT / rel)
target_obj = OUT / "rng.obj"
replaced = False
for index, obj in enumerate(objects):
    if obj.name == "rng.obj" and obj.parent.name == "common":
        objects[index] = target_obj
        replaced = True
print("target slot replaced:", replaced, "objects:", len(objects))
test_obj = OBJROOT / "tests/unit/common/rng_test.obj"
main_obj = OBJROOT / "gen/common_rng_test_main.obj"
for probe in (test_obj, main_obj):
    print("exists", probe.name, probe.is_file())


def run(label, args, cwd=None, timeout=120):
    line = subprocess.list2cmdline(args)
    print(label, "cmdline chars", len(line))
    try:
        proc = subprocess.run(args, capture_output=True, text=True, timeout=timeout, cwd=cwd)
        print(label, "exit", proc.returncode)
        if proc.returncode != 0:
            print((proc.stdout + proc.stderr)[-800:])
        return proc.returncode
    except subprocess.TimeoutExpired:
        print(label, "TIMEOUT")
        return -2


rc = run(
    "compile",
    [CLANG, *FLAGS, *[part for inc in ("src", "tools", "tests") for part in ("-I", str(ROOT / inc))], "-c", str(src), "-o", str(target_obj)],
)
vendor_objects = [OBJROOT / c.relative_to(ROOT).with_suffix(".obj") for c in sorted((ROOT / "vendor").rglob("*.c"))]
print("vendor objects:", [v.name for v in vendor_objects])
rc = run("link", [CLANG, *FLAGS, *LINK, *[str(o) for o in objects], str(test_obj), str(main_obj), *[str(v) for v in vendor_objects], "-o", str(OUT / "t.exe")])
if rc == 0:
    rc = run("run", [str(OUT / "t.exe")], cwd=str(ROOT), timeout=20)
