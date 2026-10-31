import json
import sys


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(",", ": "), ensure_ascii=False)


def main():
    if len(sys.argv) != 3:
        print("usage: compare_report.py <got> <want>")
        return 2
    with open(sys.argv[1], encoding="utf-8") as f:
        got = json.load(f)
    with open(sys.argv[2], encoding="utf-8") as f:
        want = json.load(f)
    got_config = got.pop("run_config", None)
    want_config = want.pop("run_config", None)
    if canonical(got) != canonical(want):
        print("report body differs outside run_config")
        return 1
    got_ga = (got_config or {}).get("ga", {})
    want_ga = (want_config or {}).get("ga", {})
    added = {k: got_ga[k] for k in sorted(got_ga) if k not in want_ga}
    removed = {k: want_ga[k] for k in sorted(want_ga) if k not in got_ga}
    changed = {k: (want_ga[k], got_ga[k]) for k in sorted(want_ga) if k in got_ga and want_ga[k] != got_ga[k]}
    outside = {k: got_config[k] for k in sorted(got_config or {}) if k != "ga" and got_config[k] != (want_config or {}).get(k)}
    if removed or changed or outside:
        print(f"run_config drift: removed {removed} changed {changed} outside {outside}")
        return 1
    if added:
        print(f"run_config ga keys added only: {sorted(added)}")
    else:
        print("run_config identical")
    print("report identical outside run_config")
    return 0


if __name__ == "__main__":
    sys.exit(main())
