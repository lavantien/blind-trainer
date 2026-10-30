import difflib
import json
import os
import subprocess
import sys
import time

T62 = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "allowlist-port"))
sys.path.insert(0, T62)

import union_core_scheme as t62

REPO = t62.REPO
MUT_DIR = os.path.join(REPO, "artifacts", "mutants")


def ledger_rows(suite, tries=3):
    path = os.path.join(MUT_DIR, "ledger-%s.tsv" % suite)
    for attempt in range(tries):
        rows = []
        bad = 0
        try:
            with open(path, "r", encoding="utf-8") as handle:
                next(handle)
                for raw in handle:
                    fields = raw.rstrip("\r\n").split("\t")
                    if len(fields) < 11:
                        bad += 1
                        continue
                    rows.append(fields)
        except OSError:
            bad += 1
        if bad == 0:
            return rows
        print("torn read on %s (%d bad), retry %d" % (suite, bad, attempt + 1))
        time.sleep(0.2)
    raise SystemExit("ledger %s stays torn after %d tries" % (suite, tries))


def area_of(suite):
    return suite.split("_", 1)[0]


def site_key(row):
    return (row[1], int(row[2]), int(row[3]), row[4], row[5], row[6])


def union_verdict(statuses):
    if "pending" in statuses:
        return "pending"
    if "killed" in statuses:
        return "killed"
    if "timeout" in statuses:
        return "timeout"
    if "survived" in statuses:
        return "survived"
    return "unviable"


def main():
    areas = sys.argv[1].split(",") if len(sys.argv) > 1 else ["core", "scheme", "data", "puzzles", "ga", "common"]
    print("ENTER union_group areas=%s" % ",".join(areas))
    suites = []
    for name in os.listdir(MUT_DIR):
        if not name.startswith("ledger-") or not name.endswith(".tsv"):
            continue
        suite = name[7:-4]
        if area_of(suite) in areas:
            suites.append(suite)
    suites.sort()
    print("suites=%d" % len(suites))

    remap = t62.AlgRemap()
    sites = {}
    per_area = {}
    status_rows = {}
    for suite in suites:
        rows = ledger_rows(suite)
        area = area_of(suite)
        for r in rows:
            status_rows[r[7]] = status_rows.get(r[7], 0) + 1
            sites.setdefault(site_key(r), set()).add(r[7])
            per_area.setdefault(area, set()).add(site_key(r))

    union = {}
    raw_sites = {}
    for key, statuses in sites.items():
        verdict = union_verdict(statuses)
        union[verdict] = union.get(verdict, 0) + 1
        raw_sites[key] = verdict

    remapped = {}
    remap_stats = {"fresh": 0, "remapped": 0, "obsolete": 0, "ambiguous": 0}
    for key, verdict in raw_sites.items():
        path, line, col, op, frm, to = key
        if path != t62.ALG_PATH:
            remapped[key] = verdict
            continue
        state, new_line, new_col = remap.remap(line, col, frm)
        remap_stats[state] += 1
        if state in ("obsolete", "ambiguous"):
            continue
        new_key = (path, new_line, new_col, op, frm, to)
        if new_key in remapped:
            other = remapped[new_key]
            merged = union_verdict({other, verdict})
            remapped[new_key] = merged
        else:
            remapped[new_key] = verdict

    remapped_union = {}
    for verdict in remapped.values():
        remapped_union[verdict] = remapped_union.get(verdict, 0) + 1

    print("row statuses: %s" % sorted(status_rows.items()))
    print("raw distinct sites=%d union=%s" % (len(raw_sites), sorted(union.items())))
    print("remapped distinct sites=%d union=%s" % (len(remapped), sorted(remapped_union.items())))
    print("remap: %s" % remap_stats)

    per_area_counts = {}
    for area in sorted(per_area):
        verdicts = {}
        for key in per_area[area]:
            verdicts[raw_sites[key]] = verdicts.get(raw_sites[key], 0) + 1
        per_area_counts[area] = (len(per_area[area]), sorted(verdicts.items()))
        print("area %s sites=%d %s" % (area, len(per_area[area]), sorted(verdicts.items())))

    dump = {
        "areas": areas,
        "suites": suites,
        "row_statuses": status_rows,
        "raw": {"sites": len(raw_sites), "union": union},
        "remapped": {"sites": len(remapped), "union": remapped_union},
        "per_area": {a: per_area_counts[a] for a in per_area_counts},
    }
    out_path = os.path.join(os.path.dirname(__file__), "survey_%s.json" % "_".join(areas))
    with open(out_path, "w", encoding="utf-8") as handle:
        json.dump(dump, handle, indent=1)
    print("wrote %s" % out_path)
    print("EXIT union_group")


if __name__ == "__main__":
    main()
