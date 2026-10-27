import argparse
import json
import os
import shutil
import sys
import time
from datetime import datetime, timedelta
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import slog
import winproc

GB = 1024 * 1024 * 1024
TOOL = slog.Tool("clean-rust")
REPO = Path(__file__).resolve().parent.parent
SCRATCH_GLOBS = ("cargo-mutants*", "bt-*")


def fail(message):
    return TOOL.line("fail", message)


def fmt_gb(value):
    return f"{value / GB:.2f}"


def fmt_floor(value):
    return f"{value:.6f}".rstrip("0").rstrip(".") or "0"


def config_path(repo):
    env = os.environ.get("BT_CONFIG", "")
    if env:
        return Path(env) if Path(env).is_absolute() else Path(repo) / env
    return Path(repo) / "config" / "config.json"


def disk_settings(override, repo):
    path = config_path(repo)
    if not path.is_file():
        fail(f"config not found: {path}")
        raise SystemExit(2)
    try:
        doc = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        fail(f"config parse failed: {exc}")
        raise SystemExit(2)
    disk = doc.get("disk") or {}
    stale = disk.get("scratch_stale_hours")
    floor = disk.get("free_floor_gb")
    if stale is None or floor is None:
        fail("config disk.scratch_stale_hours or disk.free_floor_gb missing")
        raise SystemExit(2)
    stale = int(stale)
    if override is not None and override >= 0:
        stale = override
    return stale, float(floor)


def dir_bytes(path):
    path = Path(path)
    if not path.exists():
        return 0
    total = 0
    for root, _dirs, files in os.walk(path):
        for name in files:
            try:
                total += os.lstat(os.path.join(root, name)).st_size
            except OSError:
                pass
    return total


def scratch_dirs(temp):
    temp = Path(temp)
    if not temp.is_dir():
        return []
    return sorted(child for child in temp.iterdir() if child.is_dir() and _is_scratch(child.name))


def _is_scratch(name):
    for pattern in SCRATCH_GLOBS:
        if pattern.endswith("*") and name.startswith(pattern[:-1]):
            return True
    return False


def stale_scratches(temp, stale_hours, now=None):
    cutoff = (now or time.time()) - stale_hours * 3600
    return [str(child) for child in scratch_dirs(temp) if child.stat().st_mtime < cutoff]


def orphan_scratches(entries, live_starts):
    """entries: (path, creation datetime) pairs; live_starts: datetime list."""
    if not live_starts:
        return [path for path, _ in entries]
    oldest = min(live_starts) - timedelta(seconds=5)
    return [path for path, created in entries if created < oldest]


def remove_incremental(path):
    path = Path(path)
    if not path.exists():
        return
    shutil.rmtree(path, ignore_errors=True)
    if path.exists():
        shutil.rmtree(path, ignore_errors=True)


def remove_tree(path):
    path = Path(path)
    if not path.exists():
        return False
    try:
        shutil.rmtree(path)
    except OSError as exc:
        fail(f"delete failed: {path}: {exc}")
        return False
    return not path.exists()


def drive_free_bytes(path):
    return shutil.disk_usage(path).free


def processes_named(names):
    wanted = {name.lower() for name in names}
    found = [proc for proc in winproc.snapshot_processes() if proc["exe"].rsplit(".", 1)[0].lower() in wanted]
    seen = {proc["pid"] for proc in found}
    for pid, name in winproc.process_name_rows():
        if pid == 0 or pid in seen or not name:
            continue
        if name.rsplit(".", 1)[0].lower() in wanted:
            found.append({"pid": pid, "ppid": 0, "exe": name})
            seen.add(pid)
    return found


def selftest(work_dir):
    if not work_dir:
        fail("selftest needs -WorkDir")
        raise SystemExit(2)
    work = Path(work_dir).resolve()
    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True)
    inc = work / "repo" / "target" / "debug" / "incremental"
    inc.mkdir(parents=True)
    (inc / "cache.bin").write_text("x")
    temp_fake = work / "temp"
    temp_fake.mkdir()
    stale_dir = temp_fake / "cargo-mutants-stale"
    fresh_dir = temp_fake / "cargo-mutants-fresh"
    bt_stale_dir = temp_fake / "bt-stale"
    bt_fresh_dir = temp_fake / "bt-fresh"
    for path in (stale_dir, fresh_dir, bt_stale_dir, bt_fresh_dir):
        path.mkdir()
        (path / "a").write_text("x")
    old = time.time() - 8 * 3600
    for path in (stale_dir, bt_stale_dir):
        os.utime(path, (old, old))
    ok = True
    scratches = stale_scratches(str(temp_fake), 6)
    if sorted(scratches) != sorted([str(stale_dir), str(bt_stale_dir)]) or len(scratches) != 2:
        fail(f"stale selection wrong: {len(scratches)} picked, expected exactly cargo-mutants-stale and bt-stale")
        ok = False
    remove_incremental(str(inc))
    if (inc / "cache.bin").exists():
        fail("incremental file survived apply")
        ok = False
    if not remove_tree(str(stale_dir)):
        fail("stale scratch delete failed")
        ok = False
    if not remove_tree(str(bt_stale_dir)):
        fail("bt scratch delete failed")
        ok = False
    if not fresh_dir.exists():
        fail("fresh scratch was deleted, active run would be destroyed")
        ok = False
    if not bt_fresh_dir.exists():
        fail("fresh bt scratch was deleted, active run would be destroyed")
        ok = False
    if ok:
        print("clean-rust: selftest ok")
    return 0 if ok else 1


def main(argv=None):
    parser = argparse.ArgumentParser(
        prog="clean_rust.py",
        description="size regenerable rust caches against the disk floor and delete them on request",
        allow_abbrev=False,
    )
    parser.add_argument("-StaleHours", "--stale-hours", dest="stale_hours", type=int, default=-1)
    parser.add_argument("-Apply", "--apply", dest="apply", action="store_true")
    parser.add_argument("-Deep", "--deep", dest="deep", action="store_true")
    parser.add_argument("-OrphanScratch", "--orphan-scratch", dest="orphan_scratch", action="store_true")
    parser.add_argument("-SelfTest", "--self-test", dest="self_test", action="store_true")
    parser.add_argument("-WorkDir", "--work-dir", dest="work_dir", default="")
    parser.add_argument("-RepoRoot", "--repo-root", dest="repo_root", default="")
    parser.add_argument("-TempDir", "--temp-dir", dest="temp_dir", default="")
    parser.add_argument("-CoverageTarget", "--coverage-target", dest="coverage_target", default="")
    parser.add_argument("-MutationTarget", "--mutation-target", dest="mutation_target", default="")
    args = parser.parse_args(argv)
    TOOL.entry("clean-rust", f"apply={args.apply} deep={args.deep}")
    if args.self_test:
        code = selftest(args.work_dir)
        TOOL.exit("selftest", code)
        return code
    if args.stale_hours < -1:
        fail("-StaleHours must be 0 or greater")
        return 2
    repo = Path(args.repo_root).resolve() if args.repo_root else REPO
    stale, floor = disk_settings(args.stale_hours, repo)
    temp = args.temp_dir or os.environ.get("TEMP", "")
    cov = Path(args.coverage_target) if args.coverage_target else repo.parent / "bt-coverage-target"
    mut = Path(args.mutation_target) if args.mutation_target else repo.parent / "bt-mutation-target"
    free_before = drive_free_bytes(repo)
    incrementals = [p for p in (repo / "target/debug/incremental", repo / "target/release/incremental") if p.exists()]
    scratches = stale_scratches(temp, stale)
    if args.orphan_scratch:
        entries = [(str(child), datetime.fromtimestamp(child.stat().st_ctime)) for child in scratch_dirs(temp)]
        live = [
            datetime.fromtimestamp(ts / 10_000_000 - 62135596800)
            for ts in (
                winproc.pid_start_ticks(proc["pid"])
                for proc in processes_named(["cargo-mutants"])
            )
            if ts is not None
        ]
        for path in orphan_scratches(entries, live):
            if path not in scratches:
                scratches.append(path)
    externals = [p for p in (cov, mut) if p.exists()]
    deep_dirs = [p for p in (repo / "tests/e2e/target", repo / "fuzz/target") if p.exists()]
    inc_bytes = sum(dir_bytes(p) for p in incrementals)
    scratch_bytes = sum(dir_bytes(p) for p in scratches)
    ext_bytes = sum(dir_bytes(p) for p in externals)
    deep_bytes = sum(dir_bytes(p) for p in deep_dirs)
    print(f"clean-rust: free {fmt_gb(free_before)} GB floor {fmt_floor(floor)} GB")
    print(f"incremental: {len(incrementals)} dirs {fmt_gb(inc_bytes)} GB")
    orphan_note = ", orphans with no live owner included" if args.orphan_scratch else ""
    print(
        f"scratch: {len(scratches)} dirs {fmt_gb(scratch_bytes)} GB"
        f" (cargo-mutants and bt scratch under TEMP older than {stale} h{orphan_note})"
    )
    print(f"external: {len(externals)} dirs {fmt_gb(ext_bytes)} GB (coverage and mutation targets)")
    print(f"deep: {len(deep_dirs)} dirs {fmt_gb(deep_bytes)} GB (e2e and fuzz targets, needs -Deep and no live cargo or rustc)")
    if free_before / GB < floor:
        print(f"clean-rust: WARN below {fmt_floor(floor)} GB floor")
    if not args.apply:
        print(f"reclaimable now {fmt_gb(inc_bytes + scratch_bytes + ext_bytes)} GB (run with -Apply)")
        return 0
    live_cargo = processes_named(["cargo", "rustc"]) if args.deep else []
    if args.deep and live_cargo:
        fail(f"deep clean refused: {len(live_cargo)} cargo or rustc processes live")
        return 3
    for path in incrementals:
        remove_incremental(str(path))
    for path in scratches + [str(p) for p in externals]:
        if not remove_tree(path):
            return 1
    if args.deep:
        for path in deep_dirs:
            if not remove_tree(str(path)):
                return 1
    free_after = drive_free_bytes(repo)
    print(f"clean-rust: freed {fmt_gb(free_after - free_before)} GB free {fmt_gb(free_after)} GB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
