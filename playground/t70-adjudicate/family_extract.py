import json
import os
import sys

T62 = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "allowlist-port"))
sys.path.insert(0, T62)

import union_core_scheme as t62

MUT_DIR = os.path.join(t62.REPO, "artifacts", "mutants")


def ledger_rows(suite):
    path = os.path.join(MUT_DIR, "ledger-%s.tsv" % suite)
    rows = []
    with open(path, "r", encoding="utf-8") as handle:
        next(handle)
        for raw in handle:
            fields = raw.rstrip("\r\n").split("\t")
            if len(fields) < 11:
                continue
            rows.append(fields)
    return rows


def site_key(row):
    return (row[1], int(row[2]), int(row[3]), row[4], row[5], row[6])


def verdict_of(statuses):
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
    print("ENTER family_extract areas=%s" % ",".join(areas))
    sites = {}
    for name in sorted(os.listdir(MUT_DIR)):
        if not name.startswith("ledger-") or not name.endswith(".tsv"):
            continue
        suite = name[7:-4]
        if suite.split("_", 1)[0] not in areas:
            continue
        for r in ledger_rows(suite):
            sites.setdefault(site_key(r), set()).add(r[7])

    survivors = []
    timeout_sites = []
    pending_sites = []
    for key, statuses in sites.items():
        verdict = verdict_of(statuses)
        if verdict == "survived":
            survivors.append(key)
        elif verdict == "timeout":
            timeout_sites.append(key)
        elif verdict == "pending":
            pending_sites.append(key)

    cache = {}

    def function_of(path, line, col):
        if path not in cache:
            full = os.path.join(t62.REPO, path.replace("/", os.sep))
            try:
                with open(full, "r", encoding="utf-8") as handle:
                    text = handle.read()
            except OSError:
                cache[path] = (None, None)
                return ""
            cache[path] = (text, t62.tokenize(text))
        text, tokens = cache[path]
        if text is None:
            return ""
        offset = t62.offset_at(text, line, col)
        if offset is None:
            return ""
        return t62.function_at(tokens, offset)

    fam = {}
    for key in survivors:
        path, line, col, op, frm, to = key
        fn = function_of(path, line, col)
        gate_key = (path, fn, op, to)
        entry = fam.setdefault(gate_key, {"count": 0, "pairs": set(), "sites": []})
        entry["count"] += 1
        entry["pairs"].add((frm, to))
        entry["sites"].append((line, col, frm))

    pending_fam = {}
    for key in pending_sites + timeout_sites:
        path, line, col, op, frm, to = key
        fn = function_of(path, line, col)
        gk = (path, fn, op, to)
        pending_fam[gk] = pending_fam.get(gk, 0) + 1

    print("site survivors=%d timeout=%d pending=%d" % (len(survivors), len(timeout_sites), len(pending_sites)))
    print("gate families=%d spanning=%d" % (len(fam), sum(e["count"] for e in fam.values())))
    print("excluded timeout+pending gate families=%d" % len(pending_fam))

    out = {"areas": areas, "families": [], "excluded_timeout_pending": []}
    for gk in sorted(fam, key=lambda k: (k[0], k[1], k[2], k[3])):
        path, fn, op, to = gk
        entry = fam[gk]
        out["families"].append({
            "file": path,
            "function": fn,
            "op": op,
            "to": to,
            "count": entry["count"],
            "pairs": sorted("%s->%s" % p if p[0] else "(empty)->%s" % p[1] for p in entry["pairs"]),
            "sites": ["%d:%d %s" % s for s in sorted(entry["sites"])],
        })
    for gk in sorted(pending_fam, key=lambda k: (k[0], k[1], k[2], k[3])):
        path, fn, op, to = gk
        out["excluded_timeout_pending"].append({
            "file": path, "function": fn, "op": op, "to": to, "count": pending_fam[gk],
        })
    dump = os.path.join(os.path.dirname(__file__), "families_%s.json" % "_".join(areas))
    with open(dump, "w", encoding="utf-8") as handle:
        json.dump(out, handle, indent=1)
    print("wrote %s" % dump)
    print("EXIT family_extract")


if __name__ == "__main__":
    main()
