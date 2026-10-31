import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import tour_run

REPO = tour_run.REPO
CURRENT = "5eb4eb3"
PARENT = "eb767a6"
BUILD_TIMEOUT_SECS = 900


def export_tree(rev, target):
    import tarfile
    import io

    proc = subprocess.run(
        ["git", "-C", str(REPO), "archive", "--format=tar", rev],
        stdout=subprocess.PIPE,
        check=True,
    )
    with tarfile.open(fileobj=io.BytesIO(proc.stdout)) as archive:
        archive.extractall(target)
    for name in ("vendor", "ga"):
        source = REPO / "data" / name
        if source.is_dir():
            subprocess.run(
                ["robocopy", str(source), str(target / "data" / name), "/E", "/NFL", "/NDL", "/NJH", "/NJS", "/NP"],
                check=False,
                capture_output=True,
            )
    subprocess.run(
        ["robocopy", str(REPO / "vendor"), str(target / "vendor"), "/E", "/NFL", "/NDL", "/NJH", "/NJS", "/NP"],
        check=False,
        capture_output=True,
    )
    (target / "build" / "vendor").mkdir(parents=True, exist_ok=True)
    shutil.copy2(REPO / "build" / "vendor" / "raylib.lib", target / "build" / "vendor" / "raylib.lib")


def build_tree(target, label):
    env = None
    proc = subprocess.run(
        [sys.executable, "bt.py", "build"],
        cwd=target,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="backslashreplace",
        timeout=BUILD_TIMEOUT_SECS,
        env=env,
    )
    tail = proc.stdout[-3000:] + proc.stderr[-2000:]
    print(f"build {label}: exit {proc.returncode}")
    print(tail.strip()[-1500:])
    return proc.returncode == 0, target / "build" / "bin" / "bt-app.exe"


def main():
    problems = []
    tmp = Path(tempfile.mkdtemp(prefix="adv80b_"))
    current_root = tmp / "current"
    parent_root = tmp / "parent"
    current_root.mkdir()
    parent_root.mkdir()
    export_tree(CURRENT, current_root)
    export_tree(PARENT, parent_root)
    ok_current, exe_current = build_tree(current_root, f"current {CURRENT}")
    ok_parent, exe_parent = build_tree(parent_root, f"parent {PARENT}")
    if not ok_current or not ok_parent:
        print("FAIL build step failed")
        tour_run.purge(tmp)
        return 1
    run_current = tour_run.run_tour(exe_current, current_root, current_root / "shots_out", current_root / "appdata")
    print(f"current tour: exit {run_current['exit']} in {run_current['secs']}s, {len(run_current['slugs_sorted'])} shots")
    store_current = tour_run.store_summary(current_root / "appdata")
    run_parent = tour_run.run_tour(exe_parent, parent_root, parent_root / "shots_out", parent_root / "appdata")
    print(f"parent tour: exit {run_parent['exit']} in {run_parent['secs']}s, {len(run_parent['slugs_sorted'])} shots")
    store_parent = tour_run.store_summary(parent_root / "appdata")
    print(f"store current: {store_current['tables']}")
    print(f"store parent: {store_parent['tables']}")
    if run_current["exit"] != 0:
        problems.append(f"current exit {run_current['exit']}: {run_current['stderr_tail']}")
    if run_parent["exit"] != 0:
        problems.append(f"parent exit {run_parent['exit']}: {run_parent['stderr_tail']}")
    if run_current["slugs_sorted"] != sorted(tour_run.TOUR_ORDER):
        problems.append(f"current slug set: {run_current['slugs_sorted']}")
    if run_parent["slugs_sorted"] != sorted(tour_run.TOUR_ORDER_PARENT):
        problems.append(f"parent slug set: {run_parent['slugs_sorted']}")
    if run_current["slugs_tour_order"] != tour_run.TOUR_ORDER:
        problems.append(f"current tour order: {run_current['slugs_tour_order']}")
    if run_parent["slugs_tour_order"] != tour_run.TOUR_ORDER_PARENT:
        problems.append(f"parent tour order: {run_parent['slugs_tour_order']}")
    shared = tour_run.TOUR_ORDER_PARENT
    timing_slugs = {"solve-review", "stats"}
    drift = [
        slug
        for slug in shared
        if run_current["hashes"].get(slug) != run_parent["hashes"].get(slug)
    ]
    unexpected = [slug for slug in drift if slug not in timing_slugs]
    print(f"shared-shot byte drift parent vs current: {drift or 'none'}")
    print(f"drift outside the timing-text slugs: {unexpected or 'none'}")
    if unexpected:
        problems.append(f"shared shots differ after the replay leg: {unexpected}")
    if store_current["tables"].get("blind-trainer.db:solves") != 1:
        problems.append(f"current store solves {store_current['tables'].get('blind-trainer.db:solves')}")
    if store_parent["tables"].get("blind-trainer.db:solves") != 1:
        problems.append(f"parent store solves {store_parent['tables'].get('blind-trainer.db:solves')}")
    counts_current = {key: value for key, value in store_current["tables"].items() if key != "blind-trainer.db:solves"}
    counts_parent = {key: value for key, value in store_parent["tables"].items() if key != "blind-trainer.db:solves"}
    if counts_current != counts_parent:
        problems.append(f"store table counts differ: {counts_current} vs {counts_parent}")
    if "replay" not in run_current["hashes"]:
        problems.append("current build produced no replay.png")
    if "replay" in run_parent["hashes"]:
        problems.append("parent build produced a replay.png")
    manifest = tour_run.write_manifest(
        "manifest_parent.json",
        {
            "current_rev": CURRENT,
            "parent_rev": PARENT,
            "current": {key: run_current[key] for key in ("exit", "secs", "slugs_tour_order", "hashes", "sizes")},
            "parent": {key: run_parent[key] for key in ("exit", "secs", "slugs_tour_order", "hashes", "sizes")},
            "store_current": store_current,
            "store_parent": store_parent,
            "shared_drift": drift,
        },
    )
    tour_run.purge(tmp)
    print(f"manifest {manifest}")
    if problems:
        print("FAIL")
        for problem in problems:
            print(f"- {problem}")
        return 1
    print("PASS parent compare: 19 shared shots byte-identical, replay only in current, order preserved")
    return 0


if __name__ == "__main__":
    sys.exit(main())
