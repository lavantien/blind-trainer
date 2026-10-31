import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import tour_run

REPO = tour_run.REPO
EXE = REPO / "build" / "bin" / "bt-app.exe"
ROOT = REPO / "artifacts" / "tmp-adv80b-idem"
OUT_A = ROOT / "out_a"
OUT_B = ROOT / "out_b"
OUT_C = ROOT / "out_c"
USER_A = ROOT / "user_a"
USER_C = ROOT / "user_c"
TIMING_SLUGS = {"solve-review", "stats"}


def main():
    problems = []
    notes = []
    ROOT.mkdir(parents=True, exist_ok=True)
    run_a = tour_run.run_tour(EXE, REPO, OUT_A, USER_A)
    print(f"run A fresh: exit {run_a['exit']} in {run_a['secs']}s")
    store_a = tour_run.store_summary(USER_A)
    run_b = tour_run.run_tour(EXE, REPO, OUT_B, USER_A, fresh_user=False)
    print(f"run B reuse APPDATA: exit {run_b['exit']} in {run_b['secs']}s")
    store_b = tour_run.store_summary(USER_A)
    run_c = tour_run.run_tour(EXE, REPO, OUT_C, USER_C)
    print(f"run C fresh: exit {run_c['exit']} in {run_c['secs']}s")
    for label, run in (("A", run_a), ("B", run_b), ("C", run_c)):
        if run["exit"] != 0:
            problems.append(f"run {label} exit {run['exit']}, stderr: {run['stderr_tail']}")
        if run["slugs_sorted"] != sorted(tour_run.TOUR_ORDER):
            problems.append(f"run {label} slug set mismatch: {run['slugs_sorted']}")
    drift_ac = [
        slug
        for slug in tour_run.TOUR_ORDER
        if run_a["hashes"].get(slug) != run_c["hashes"].get(slug)
    ]
    drift_ab = [
        slug
        for slug in tour_run.TOUR_ORDER
        if run_a["hashes"].get(slug) != run_b["hashes"].get(slug)
    ]
    print(f"store after A: {store_a}")
    print(f"store after B: {store_b}")
    print(f"byte drift A vs C (fresh vs fresh): {drift_ac or 'none'}")
    print(f"byte drift A vs B (fresh vs reuse): {drift_ab or 'none'}")
    replay_same_ab = run_a["hashes"].get("replay") == run_b["hashes"].get("replay")
    replay_same_ac = run_a["hashes"].get("replay") == run_c["hashes"].get("replay")
    print(f"replay.png identical A vs C: {replay_same_ac}, A vs B: {replay_same_ab}")
    unexpected_ac = [slug for slug in drift_ac if slug not in TIMING_SLUGS]
    if unexpected_ac:
        problems.append(f"fresh runs A and C differ outside timing slugs on {unexpected_ac}")
    solves_a = dict(store_a["tables"]).get("blind-trainer.db:solves")
    solves_b = dict(store_b["tables"]).get("blind-trainer.db:solves")
    if solves_a != 1:
        problems.append(f"fresh tour logged {solves_a} solve records, want exactly 1")
    if solves_b != 2:
        problems.append(f"reuse tour logged {solves_b} solve records total, want exactly 2")
    if not replay_same_ab or not replay_same_ac:
        problems.append("replay.png bytes are not stable across runs")
    notes.append(
        "solve-review and stats render wall-clock split times, so their bytes differ across runs by design"
    )
    manifest = tour_run.write_manifest(
        "manifest_idempotence.json",
        {
            "exe": str(EXE),
            "runs": {
                "A_fresh": {key: run_a[key] for key in ("exit", "secs", "slugs_sorted", "hashes")},
                "B_reuse": {key: run_b[key] for key in ("exit", "secs", "slugs_sorted", "hashes")},
                "C_fresh": {key: run_c[key] for key in ("exit", "secs", "slugs_sorted", "hashes")},
            },
            "store_after_a": store_a,
            "store_after_b": store_b,
            "drift_ac": drift_ac,
            "drift_ab": drift_ab,
        },
    )
    tour_run.purge(ROOT)
    print(f"manifest {manifest}")
    if problems:
        print("FAIL")
        for problem in problems:
            print(f"- {problem}")
        return 1
    print("PASS idempotence: 20-shot set on rerun, exactly 1 solve per tour, replay bytes stable")
    return 0


if __name__ == "__main__":
    sys.exit(main())
