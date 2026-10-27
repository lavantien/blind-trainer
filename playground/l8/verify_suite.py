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
includes = ["-Isrc", "-Itools", "-Itests"]
out = root / "build/test-tmp/l8verify"
out.mkdir(parents=True, exist_ok=True)
obj_root = next((root / "build/obj").iterdir())


def run(argv):
    proc = subprocess.run([str(part) for part in argv], cwd=root, capture_output=True, text=True)
    if proc.returncode != 0:
        sys.stderr.write(proc.stdout + proc.stderr)
        raise SystemExit(f"failed: {argv[:4]}")
    return proc


units = [
    ("tools/jobwrap/lib.c", out / "lib.obj"),
    ("tests/unit/tools/jobwrap_test.c", out / "test.obj"),
    ("build/gen/tools_jobwrap_test_main.c", out / "main.obj"),
]
for source, obj in units:
    run([clang, *flags, *includes, "-c", source, "-o", obj])

link = [
    clang,
    *flags,
    "-fuse-ld=lld-link",
    "-Xlinker",
    "/SUBSYSTEM:CONSOLE",
    "-Xlinker",
    "/OPT:REF",
    "-Xlinker",
    "/OPT:ICF",
    "-Xlinker",
    "/DEBUG:NONE",
    "-Xlinker",
    "/brepro",
    out / "lib.obj",
    obj_root / "tools/jobwrap/ffi.obj",
    obj_root / "src/common/bt_trace.obj",
    obj_root / "src/common/strbuf.obj",
    obj_root / "src/common/json.obj",
    obj_root / "src/common/config.obj",
    obj_root / "src/common/paths.obj",
    obj_root / "src/common/bt_harness.obj",
    out / "test.obj",
    out / "main.obj",
    "-o",
    out / "jobwrap_test.exe",
]
run(link)
print("built", out / "jobwrap_test.exe")
