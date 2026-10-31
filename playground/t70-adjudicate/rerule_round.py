import os
import subprocess
import sys

T62 = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "allowlist-port"))
sys.path.insert(0, T62)

import union_core_scheme as t62

REPO = t62.REPO
MUT_DIR = os.path.join(REPO, "artifacts", "mutants")
MUTATE_EXE = os.path.join(REPO, "build", "bin", "mutate.exe")

SITE_RESETS = {
    "scheme_classifier_test": [("src/scheme/classifier.c", {("288", "21"), ("301", "9")})],
}
FUNCTION_RESETS = {
    "common_json_test": [("src/common/json.c",
                          {"append_utf8", "utf8_valid", "parse_escaped_codepoint", "read_hex4"})],
    "data_repo_algs_test": [("src/data/paths.c", {"bt_user_algs_eq"})],
    "data_repo_solves_test": [("src/data/repo_solves.c", {"bt_solve_log_eq"})],
    "data_repo_srs_test": [("src/data/repo_srs.c", {"bt_srs_store_eq"})],
    "data_seedpack_test": [("src/data/seedpack/parse.c", {"blank_line"})],
}

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


def reset_ledger(suite, site_keys, function_keys):
    path = os.path.join(MUT_DIR, "ledger-%s.tsv" % suite)
    with open(path, "r", encoding="utf-8", newline="") as handle:
        raw = handle.read()
    lines = raw.split("\n")
    reset = 0
    for i in range(1, len(lines)):
        fields = lines[i].split("\t")
        if len(fields) < 11 or fields[7] != "survived":
            continue
        if (fields[1], fields[2], fields[3]) in site_keys:
            pass
        elif (fields[1], function_of(fields[1], int(fields[2]), int(fields[3]))) in function_keys:
            pass
        else:
            continue
        fields[7] = "pending"
        lines[i] = "\t".join(fields)
        reset += 1
    with open(path, "w", encoding="utf-8", newline="") as handle:
        handle.write("\n".join(lines))
    return reset


def child_env():
    env = os.environ.copy()
    env["BT_REF_GROUND"] = "ref_ground"
    env["BT_CONFIG"] = os.path.join(REPO, "config", "config.json")
    env["BT_ARTIFACTS"] = "artifacts"
    return env


def area_sources(area):
    root = os.path.join(REPO, "src", area)
    found = []
    for base, _, names in os.walk(root):
        for name in sorted(names):
            if name.endswith(".c"):
                full = os.path.join(base, name)
                found.append(os.path.relpath(full, REPO).replace("\\", "/"))
    return sorted(found)


def main():
    print("ENTER rerule_round")
    suites = {}
    for suite, pairs in SITE_RESETS.items():
        site_keys = set()
        function_keys = set()
        for file, sites in pairs:
            for line, col in sites:
                site_keys.add((file, line, col))
        suites[suite] = (site_keys, function_keys)
    for suite, pairs in FUNCTION_RESETS.items():
        site_keys, function_keys = suites.get(suite, (set(), set()))
        for file, names in pairs:
            for name in names:
                function_keys.add((file, name))
        suites[suite] = (site_keys, function_keys)
    env = child_env()
    for suite in sorted(suites):
        site_keys, function_keys = suites[suite]
        reset = reset_ledger(suite, site_keys, function_keys)
        print("suite %s reset %d rows" % (suite, reset))
        if reset == 0:
            continue
        area = suite.split("_", 1)[0]
        argv = [MUTATE_EXE, "run", "--ledger",
                os.path.join(MUT_DIR, "ledger-%s.tsv" % suite), "--exe", suite]
        argv += area_sources(area)
        proc = subprocess.run(argv, capture_output=True, text=True, cwd=REPO, env=env)
        tail = (proc.stdout + proc.stderr).strip().splitlines()
        for line in tail[-4:]:
            print("  %s" % line)
        if proc.returncode != 0:
            raise SystemExit("rerule failed for %s rc=%d" % (suite, proc.returncode))
    union_argv = [MUTATE_EXE, "union-report"]
    for name in sorted(os.listdir(MUT_DIR)):
        if name.startswith("ledger-") and name.endswith(".tsv"):
            union_argv += ["--ledger", os.path.join(MUT_DIR, name)]
    union_argv += ["--out", os.path.join(MUT_DIR, "outcomes.json")]
    proc = subprocess.run(union_argv, capture_output=True, text=True, cwd=REPO, env=env)
    print("union rc=%d %s%s" % (proc.returncode, proc.stdout.strip(), proc.stderr.strip()))
    if proc.returncode != 0:
        raise SystemExit("union-report failed")
    print("EXIT rerule_round")


if __name__ == "__main__":
    main()
