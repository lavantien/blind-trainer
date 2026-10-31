import hashlib
import json
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
PROBE_DIR = Path(__file__).resolve().parent


def compile_flags(hub):
    cfg = hub["build"]
    return ["-target", cfg["target"], f"-std={cfg['std']}", *cfg["cflags"], *cfg["release_flags"]]


def main():
    stem = sys.argv[1] if len(sys.argv) > 1 else "faultprobe"
    source = Path(sys.argv[2]) if len(sys.argv) > 2 else PROBE_DIR / f"{stem}.c"
    source = source if source.is_absolute() else REPO / source
    hub = json.loads((REPO / "config" / "config.json").read_text(encoding="utf-8"))
    clang = hub["build"]["tools"].get("clang") or "clang"
    flags = compile_flags(hub)
    dirname = hashlib.sha256("\n".join(flags).encode("utf-8")).hexdigest()[:12]
    obj_root = REPO / hub["build"]["dirs"]["obj"] / dirname
    if not obj_root.is_dir():
        print(f"object root missing: {obj_root}")
        return 1
    probe_obj = PROBE_DIR / f"{stem}.obj"
    compile_argv = [
        clang,
        *flags,
        "-I",
        str(REPO / "src"),
        "-I",
        str(REPO / "tools"),
        "-I",
        str(REPO / "tests"),
        "-isystem",
        str(REPO / "vendor" / "sqlite"),
        "-c",
        str(source),
        "-o",
        str(probe_obj),
    ]
    proc = subprocess.run(compile_argv, capture_output=True, text=True)
    if proc.returncode != 0:
        print(proc.stderr)
        return proc.returncode
    objects = sorted(
        obj for obj in obj_root.glob("src/**/*.obj") if "src\\app" not in str(obj.relative_to(obj_root))
    )
    sqlite_obj = obj_root / "vendor" / "sqlite" / "sqlite3.obj"
    if sqlite_obj.is_file():
        objects.append(sqlite_obj)
    objects.append(probe_obj)
    link_argv = [clang, *flags, "-fuse-ld=lld-link"]
    for flag in hub["build"]["link_flags"]:
        link_argv += ["-Xlinker", flag]
    link_argv += [str(obj) for obj in objects]
    link_argv += ["-o", str(PROBE_DIR / f"{stem}.exe")]
    proc = subprocess.run(link_argv, capture_output=True, text=True)
    if proc.returncode != 0:
        print(proc.stderr[-4000:])
        return proc.returncode
    print(f"linked {PROBE_DIR / (stem + '.exe')} over {len(objects)} objects")
    return 0


if __name__ == "__main__":
    sys.exit(main())
