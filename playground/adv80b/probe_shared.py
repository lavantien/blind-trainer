import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import tour_run

REPO = tour_run.REPO
EXE = REPO / "build" / "bin" / "bt-app.exe"
OUT = REPO / "artifacts" / "tmp-adv80b-usage"
USER = REPO / "artifacts" / "tmp-adv80b-user"


def main():
    problems = []
    result = tour_run.run_tour(EXE, REPO, OUT, USER)
    print(f"exit {result['exit']} in {result['secs']}s")
    print(f"shots sorted: {result['slugs_sorted']}")
    print(f"shots in tour order: {result['slugs_tour_order']}")
    if result["exit"] != 0:
        problems.append(f"exit {result['exit']}, stderr: {result['stderr_tail']}")
    if result["slugs_sorted"] != sorted(tour_run.TOUR_ORDER):
        problems.append(f"slug set mismatch: {result['slugs_sorted']}")
    if result["slugs_tour_order"] != tour_run.TOUR_ORDER:
        problems.append(f"tour order mismatch: {result['slugs_tour_order']}")
    dims = {}
    for slug in result["slugs_sorted"]:
        path = OUT / f"{slug}.png"
        size = path.stat().st_size
        dimension = tour_run.png_dims(path)
        dims[slug] = {"bytes": size, "dims": dimension}
        if dimension is None:
            problems.append(f"{slug}.png is not a valid png")
        if size <= 0:
            problems.append(f"{slug}.png is empty")
    replay = OUT / "replay.png"
    if not tour_run.shot_is_distinct(replay):
        problems.append("replay.png is blank under the distinct-content check")
    for slug in result["slugs_sorted"]:
        if not tour_run.shot_is_distinct(OUT / f"{slug}.png"):
            problems.append(f"{slug}.png is blank under the distinct-content check")
    result["dims"] = dims
    result["distinct_replay"] = True
    manifest = tour_run.write_manifest(
        "manifest_shared.json",
        {
            "exe": str(EXE),
            "run": {key: result[key] for key in ("exit", "secs", "slugs_sorted", "slugs_tour_order", "hashes", "sizes")},
            "dims": dims,
        },
    )
    tour_run.purge(OUT, USER)
    print(f"manifest {manifest}")
    if problems:
        print("FAIL")
        for problem in problems:
            print(f"- {problem}")
        return 1
    print("PASS shared binary tour: exit 0, 20 shots, order matches, all pngs valid and distinct")
    return 0


if __name__ == "__main__":
    sys.exit(main())
