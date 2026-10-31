import json
import os
import subprocess
import sys

T62 = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "allowlist-port"))
sys.path.insert(0, T62)

import union_core_scheme as t62

REPO = t62.REPO
MUT_DIR = os.path.join(REPO, "artifacts", "mutants")
GATE_EXE = os.path.join(REPO, "build", "bin", "mutants-gate.exe")
HERE = os.path.dirname(os.path.abspath(__file__))


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


def load_sites(areas):
    sites = {}
    for name in sorted(os.listdir(MUT_DIR)):
        if not name.startswith("ledger-") or not name.endswith(".tsv"):
            continue
        suite = name[7:-4]
        if suite.split("_", 1)[0] not in areas:
            continue
        for r in ledger_rows(suite):
            sites.setdefault(site_key(r), set()).add(r[7])
    return sites


FUNCTION_CACHE = {}


def function_of(path, line, col):
    if path not in FUNCTION_CACHE:
        full = os.path.join(REPO, path.replace("/", os.sep))
        try:
            with open(full, "r", encoding="utf-8") as handle:
                text = handle.read()
        except OSError:
            FUNCTION_CACHE[path] = (None, None)
            return ""
        FUNCTION_CACHE[path] = (text, t62.tokenize(text))
    text, tokens = FUNCTION_CACHE[path]
    if text is None:
        return ""
    offset = t62.offset_at(text, line, col)
    if offset is None:
        return ""
    return t62.function_at(tokens, offset)


def mutant_scenario(key, function):
    path, line, col, op, frm, to = key
    display = to if to != "" else "(empty)"
    return {
        "name": "%s:%d:%d: %s %s -> %s" % (path, line, col, op, frm, display),
        "package": "blind-trainer",
        "file": path,
        "function": None if function == "" else {"function_name": function},
        "span": {"start": {"line": line, "column": col}, "end": {"line": line, "column": col}},
        "replacement": to,
        "genre": op,
    }


SUMMARY = {
    "killed": "CaughtMutant",
    "survived": "MissedMutant",
    "unviable": "Unviable",
    "timeout": "Timeout",
}


def shard_json(entries):
    counters = {"caught": 0, "missed": 0, "timeout": 0, "unviable": 0}
    outcomes = [{"scenario": "Baseline", "summary": "Success"}]
    for key, function, verdict in entries:
        outcomes.append({"scenario": {"Mutant": mutant_scenario(key, function)},
                         "summary": SUMMARY[verdict]})
        if verdict == "killed":
            counters["caught"] += 1
        elif verdict == "survived":
            counters["missed"] += 1
        elif verdict == "timeout":
            counters["timeout"] += 1
        else:
            counters["unviable"] += 1
    total = len(entries)
    root = {
        "total_mutants": total,
        "caught": counters["caught"],
        "missed": counters["missed"],
        "timeout": counters["timeout"],
        "unviable": counters["unviable"],
        "success": 0,
        "cargo_mutants_version": "bt-mutate",
        "start_time": "2026-10-30T00:00:00",
        "end_time": "2026-10-30T00:00:00",
        "outcomes": outcomes,
    }
    return root


def write_json(path, root):
    with open(path, "w", encoding="utf-8") as handle:
        json.dump(root, handle, separators=(",", ":"))


def run_gate(outcomes_path, allowlist_path):
    proc = subprocess.run([GATE_EXE, outcomes_path, allowlist_path], capture_output=True,
                          text=True, cwd=REPO)
    return proc.returncode, proc.stdout.strip(), proc.stderr.strip()


def main():
    areas = sys.argv[1].split(",") if len(sys.argv) > 1 else ["core", "scheme"]
    allowlist_path = sys.argv[2] if len(sys.argv) > 2 else os.path.join(
        HERE, "allowlist_%s.tsv" % "_".join(areas))
    print("ENTER merge_proof areas=%s" % ",".join(areas))
    sites = load_sites(areas)
    entries = []
    excluded = {"timeout": 0, "pending": 0}
    test_killed = set()
    manifest_path = os.path.join(HERE, "manifest_%s.json" % "_".join(areas))
    with open(manifest_path, "r", encoding="utf-8") as handle:
        manifest = json.load(handle)
    for family in manifest["test_disposed"]:
        for site in family["killed"]:
            line, col = site.split(":")
            test_killed.add((family["file"], int(line), int(col), family["op"], family["to"]))
    for key, statuses in sorted(sites.items()):
        verdict = verdict_of(statuses)
        if verdict == "pending":
            excluded["pending"] += 1
            continue
        path, line, col, op, frm, to = key
        if verdict == "survived" and (path, line, col, op, to) in test_killed:
            verdict = "killed"
        entries.append((key, function_of(path, line, col), verdict))
    half = len(entries) // 2
    shards = [entries[:half], entries[half:]]
    shard_paths = []
    for index, shard in enumerate(shards):
        path = os.path.join(HERE, "proof_shard%d.json" % index)
        write_json(path, shard_json(shard))
        shard_paths.append(path)
        print("shard %d: %d mutants" % (index, len(shard)))
    merged_path = os.path.join(HERE, "proof_merged_%s.json" % "_".join(areas))
    proc = subprocess.run([GATE_EXE, "merge", merged_path] + shard_paths, capture_output=True,
                          text=True, cwd=REPO)
    print("merge rc=%d out=%s err=%s" % (proc.returncode, proc.stdout.strip(), proc.stderr.strip()))
    if proc.returncode != 0:
        raise SystemExit("merge failed")
    rc, out, err = run_gate(merged_path, allowlist_path)
    print("gate(post-retest truth) rc=%d out=%s err=%s" % (rc, out, err[:400]))
    snapshot_entries = []
    for key, function, verdict in entries:
        path, line, col, op, frm, to = key
        if (path, line, col, op, to) in test_killed and verdict == "killed":
            verdict = "survived"
        snapshot_entries.append((key, function, verdict))
    half = len(snapshot_entries) // 2
    for index, shard in enumerate([snapshot_entries[:half], snapshot_entries[half:]]):
        write_json(os.path.join(HERE, "proof_shard%d.json" % index), shard_json(shard))
    merged_snapshot = os.path.join(HERE, "proof_merged_snapshot_%s.json" % "_".join(areas))
    proc = subprocess.run([GATE_EXE, "merge", merged_snapshot] +
                          [os.path.join(HERE, "proof_shard%d.json" % i) for i in range(2)],
                          capture_output=True, text=True, cwd=REPO)
    if proc.returncode != 0:
        raise SystemExit("snapshot merge failed: %s" % proc.stderr)
    rc2, out2, err2 = run_gate(merged_snapshot, allowlist_path)
    print("gate(snapshot truth) rc=%d out=%s err=%s" % (rc2, out2, err2[:400]))
    print("excluded timeout=%d pending=%d" % (excluded["timeout"], excluded["pending"]))
    print("EXIT merge_proof")


if __name__ == "__main__":
    main()
