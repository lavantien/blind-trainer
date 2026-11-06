import json
import sys
from pathlib import Path


def load_cases(path):
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    return {(c["orbit"], c["t1"], c["t2"]): c for c in data["cases"]}


def main():
    if len(sys.argv) != 3:
        print("usage: cases_superset.py <old cases.json> <new cases.json>", file=sys.stderr)
        return 2
    old = load_cases(sys.argv[1])
    new = load_cases(sys.argv[2])
    lost = sorted(set(old) - set(new))
    added = sorted(set(new) - set(old))
    if lost:
        print(f"lost cases: {lost}")
    bad_added = [k for k in added if not new[k]["covered"]]
    regressions = sorted(k for k in set(old) & set(new)
                         if old[k]["covered"] and not new[k]["covered"])
    derived = []
    drift = []
    for k in sorted(set(old) & set(new)):
        a = old[k]
        b = new[k]
        if a == b:
            continue
        allowed = (not a["covered"] and b["covered"]
                   and not a["vendor_algs"] and b["vendor_algs"])
        if allowed and all(alg.get("ok") for alg in b["vendor_algs"]):
            derived.append(k)
        else:
            fields = sorted(f for f in set(a) | set(b) if a.get(f) != b.get(f))
            drift.append((k, fields))
    old_covered = sum(c["covered"] for c in old.values())
    new_covered = sum(c["covered"] for c in new.values())
    print(f"entries: old {len(old)} new {len(new)}")
    print(f"covered: {old_covered} -> {new_covered}")
    print(f"added: {added}")
    print(f"derived gains: {derived}")
    for k in derived:
        for alg in new[k]["vendor_algs"]:
            print(f"  {k} solver={alg['solver']} rowid={alg['rowid']} "
                  f"position={alg['position']} qtm={alg['qtm']}")
    print(f"regressions: {regressions}")
    print(f"drift: {drift}")
    return 1 if lost or bad_added or regressions or drift else 0


if __name__ == "__main__":
    sys.exit(main())
