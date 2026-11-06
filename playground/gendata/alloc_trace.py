import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[2]
clang = Path("C:/Program Files/LLVM/bin/clang.exe")
symbolizer = Path("C:/Program Files/LLVM/bin/llvm-symbolizer.exe")
flags = [
    "-target", "x86_64-pc-windows-msvc", "-std=c23", "-Wall", "-Wextra", "-Werror",
    "-ffp-contract=off", "-g", "-D_CRT_SECURE_NO_WARNINGS", "-O2", "-DNDEBUG",
]
linker = ["-fuse-ld=lld-link", "-Xlinker", "/SUBSYSTEM:CONSOLE", "-Xlinker", "/OPT:REF",
          "-Xlinker", "/OPT:ICF", "-Xlinker", "/DEBUG", "-Xlinker", "/DYNAMICBASE:NO",
          "-Xlinker", "/brepro"]
obj_root = next((root / "build/obj").iterdir())

obj_dir = root / "build/test-tmp/gdverify/alloctrace"
obj_dir.mkdir(parents=True, exist_ok=True)
sources = [p for p in sorted((root / "tools/gendata").glob("*.c")) if p.name != "main.c"]
sources.append(root / "playground/gendata/alloc_trace.c")
sources.append(root / "playground/gendata/alloc_trace_main.c")
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
closure = [obj for obj in closure if obj.name != "alloc.obj"]
sqlite = sorted((obj_root / "vendor").rglob("sqlite3.obj"))
exe = root / "build/test-tmp/gdverify/alloc_trace.exe"
dump = root / "build/test-tmp/gdverify/alloc_trace.tsv"
argv = [str(clang), *flags, *linker, *[str(obj_dir / (p.stem + ".obj")) for p in sources],
        *map(str, closure), *map(str, sqlite), "-o", str(exe)]
proc = subprocess.run(argv, cwd=root, capture_output=True, text=True)
if proc.returncode != 0:
    sys.stderr.write(proc.stdout + proc.stderr)
    raise SystemExit(2)

run = subprocess.run([str(exe), str(dump)], cwd=root, capture_output=True, text=True, timeout=1500)
sys.stderr.write(run.stderr)
sys.stdout.write(run.stdout)
print(f"[{run.returncode}] alloc_trace")
if run.returncode != 0:
    raise SystemExit(2)

rows = []
for line in dump.read_text(encoding="utf-8").splitlines():
    index, size, site = line.split()
    rows.append((int(index), int(size), int(site, 16)))
start = 0
for part in run.stdout.split():
    if part.startswith("start="):
        start = int(part.split("=")[1])
print(f"rows={len(rows)} start={start}")

sites = sorted({site for _, _, site in rows})
proc = subprocess.run(
    [str(symbolizer), f"--obj={exe}", "--functions=none", "--output-style=GNU"],
    input="\n".join(f"{site:#x}" for site in sites), capture_output=True, text=True)
lines = [part.strip() for part in proc.stdout.split("\n") if part.strip() != "?"]
loc_by_site = {}
for i, site in enumerate(sites):
    loc_by_site[site] = lines[i] if i < len(lines) else "?"

synth_sites = {}
for site, loc in loc_by_site.items():
    if "synth.c:" in loc:
        synth_sites[site] = loc
print("synth.c sites:")
for site in sorted(synth_sites):
    hits = [(index - start, size) for index, size, s in rows if s == site]
    print(f"  {synth_sites[site]} count={len(hits)} last_rel={[h[0] for h in hits[-6:]]}")

print("last 40 rows:")
for index, size, site in rows[-40:]:
    print(f"  {index - start} {size} {loc_by_site.get(site, '?')}")

if len(sys.argv) >= 3:
    lo = int(sys.argv[1])
    hi = int(sys.argv[2])
    print(f"window rows {lo}..{hi}:")
    for index, size, site in rows:
        if lo <= index - start <= hi:
            print(f"  {index - start} {size} {loc_by_site.get(site, '?')}")
