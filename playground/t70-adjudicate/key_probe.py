import os
import sys

T62 = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "allowlist-port"))
sys.path.insert(0, T62)

import union_core_scheme as t62

MUT_DIR = os.path.join(t62.REPO, "artifacts", "mutants")

KEYS = {
    "file_line_col_op_from_to": lambda r: (r[1], r[2], r[3], r[4], r[5], r[6]),
    "file_line_col_op": lambda r: (r[1], r[2], r[3], r[4]),
    "file_line_col": lambda r: (r[1], r[2], r[3]),
    "file_line_col_op_to": lambda r: (r[1], r[2], r[3], r[4], r[6]),
    "file_line_col_from_to": lambda r: (r[1], r[2], r[3], r[5], r[6]),
}


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
    print("ENTER key_probe")
    sites = {name: {} for name in KEYS}
    for name in os.listdir(MUT_DIR):
        if not name.startswith("ledger-") or not name.endswith(".tsv"):
            continue
        path = os.path.join(MUT_DIR, name)
        with open(path, "r", encoding="utf-8") as handle:
            next(handle)
            for raw in handle:
                fields = raw.rstrip("\r\n").split("\t")
                if len(fields) < 11:
                    continue
                for key_name, key_of in KEYS.items():
                    sites[key_name].setdefault(key_of(fields), set()).add(fields[7])
    for key_name in KEYS:
        counts = {}
        for statuses in sites[key_name].values():
            verdict = verdict_of(statuses)
            counts[verdict] = counts.get(verdict, 0) + 1
        print("%s sites=%d %s" % (key_name, len(sites[key_name]), sorted(counts.items())))
    print("EXIT key_probe")


if __name__ == "__main__":
    main()
