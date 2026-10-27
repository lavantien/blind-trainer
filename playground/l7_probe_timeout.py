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
OUT = ROOT / "build/l7probe"

OUT.mkdir(parents=True, exist_ok=True)
source = (ROOT / "tests/unit/tools/fixtures/fx_target.c").read_text()
lines = source.splitlines(keepends=True)
target_line = lines[9]
assert target_line.rstrip().endswith("n = n + 1;"), target_line
lines[9] = target_line.replace("1;", "0;", 1)
mutated = OUT / "fx_target.c"
mutated.write_text("".join(lines))


def run(args, timeout=60):
    proc = subprocess.run(args, capture_output=True, text=True, timeout=timeout)
    return proc.returncode, (proc.stdout + proc.stderr)[-400:]


for name, src in (("fx_target", mutated), ("fx_main", ROOT / "tests/unit/tools/fixtures/fx_main.c")):
    code, tail = run([CLANG, *FLAGS, *[part for inc in ("src", "tools", "tests") for part in ("-I", str(ROOT / inc))], "-c", str(src), "-o", str(OUT / f"{name}.obj")])
    print(name, "compile", code, tail)
code, tail = run([CLANG, *FLAGS, *LINK, str(OUT / "fx_target.obj"), str(OUT / "fx_main.obj"), "-o", str(OUT / "fx_test.exe")])
print("link", code, tail)
try:
    proc = subprocess.run([str(OUT / "fx_test.exe")], capture_output=True, text=True, timeout=5)
    print("run exit", proc.returncode, proc.stdout, proc.stderr)
except subprocess.TimeoutExpired:
    print("run TIMED OUT as expected")
