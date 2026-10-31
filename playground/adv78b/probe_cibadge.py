import hashlib
import json
import os
import subprocess
import sys
import tempfile
import xml.dom.minidom
from pathlib import Path

REPO = Path(r"C:\Users\lavantien\dev\github\blind-trainer")
SCRIPT = REPO / "scripts" / "ci_badge.py"
HERE = Path(__file__).resolve().parent
CASES = HERE / "cases"
COMMITTED = REPO / "docs" / "badges" / "ci.svg"

REAL_RUN = {
    "conclusion": "success",
    "createdAt": "2026-10-31T15:56:23Z",
    "databaseId": 41217224663,
    "displayTitle": "docs(progress): ci fix round 2 closed, cold-clone chain green from fb...",
    "event": "push",
    "headSha": "1d763327e019233da29f0e8b17b839be02bec9a0",
    "status": "completed",
}

lines = []


def note(text):
    lines.append(text)
    print(text)


def sha(data):
    return hashlib.sha256(data).hexdigest()[:16]


def write_case(name, payload, bat=None):
    d = CASES / name
    d.mkdir(parents=True, exist_ok=True)
    if isinstance(payload, str):
        payload = payload.encode("utf-8")
    (d / "payload.json").write_bytes(payload)
    body = bat if bat is not None else "@type \"%~dp0payload.json\"\n"
    (d / "gh.bat").write_text(body, encoding="ascii")
    return d


def run_script(case_dir, argv=(), env=None, use_case_path=True):
    out = case_dir / "ci.svg"
    e = dict(os.environ)
    if use_case_path:
        e["PATH"] = str(case_dir) + os.pathsep + e["PATH"]
    if env:
        e.update(env)
    proc = subprocess.run(
        [sys.executable, str(SCRIPT), "--out", str(out), *argv],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="backslashreplace",
        env=e,
        timeout=90,
    )
    svg = out.read_bytes() if out.exists() else None
    return {
        "rc": proc.returncode,
        "stdout": proc.stdout.strip().splitlines()[:1],
        "svg": svg,
        "out": out,
    }


def show(tag, r):
    svg_state = "none" if r["svg"] is None else sha(r["svg"])
    note(f"{tag}: rc={r['rc']} svg={svg_state} stdout={r['stdout']}")


def main():
    passed_bytes = None
    determinism_ref = None

    note("== stub: exact live run payload ==")
    d = write_case("passed_real", json.dumps([REAL_RUN]))
    r = run_script(d)
    show("passed_real", r)
    passed_bytes = r["svg"]
    note(f"passed_real bytes == committed: {passed_bytes == COMMITTED.read_bytes()}")

    note("== determinism: same payload, second invocation, fresh dir ==")
    d2 = write_case("passed_real_again", json.dumps([REAL_RUN]))
    r2 = run_script(d2)
    show("passed_real_again", r2)
    note(f"identical bytes across runs: {r2['svg'] == passed_bytes}")
    determinism_ref = r2["svg"]

    note("== determinism: different run id and sha, same verdict ==")
    other = dict(REAL_RUN)
    other["databaseId"] = 99999999999
    other["headSha"] = "aaaabbbbccccddddeeeeffff0000111122223333"
    other["createdAt"] = "2026-01-02T03:04:05Z"
    d3 = write_case("passed_other_run", json.dumps([other]))
    r3 = run_script(d3)
    show("passed_other_run", r3)
    note(f"svg identical to ref, only stdout detail differs: {r3['svg'] == determinism_ref}")

    note("== verdict mapping: failed conclusion ==")
    fail_run = dict(REAL_RUN)
    fail_run["conclusion"] = "failure"
    rf = run_script(write_case("failed", json.dumps([fail_run])))
    show("failed", rf)

    note("== verdict mapping: timed_out conclusion (gap probe) ==")
    to_run = dict(REAL_RUN)
    to_run["conclusion"] = "timed_out"
    rt = run_script(write_case("timed_out", json.dumps([to_run])))
    show("timed_out", rt)

    note("== empty list: no run exists ==")
    re_ = run_script(write_case("empty", "[]"))
    show("empty", re_)

    note("== truncated json ==")
    rt2 = run_script(write_case("truncated", '[{"databaseId":1,"sta'))
    show("truncated", rt2)

    note("== non-list json ==")
    rn = run_script(write_case("nonlist", '{"status": "completed"}'))
    show("nonlist", rn)

    note("== invalid utf-8 byte inside a json string ==")
    payload = b'[{"status":"completed","conclusion":"success","headSha":"a\xffb","databaseId":1}]'
    ru = run_script(write_case("bad_utf8", payload))
    show("bad_utf8", ru)

    note("== unicode in adjacent fields (emoji, cjk, rtl) ==")
    uni_run = dict(REAL_RUN)
    uni_run["displayTitle"] = "ci \U0001f9ea \u4f60\u597d \u05e9\u05dc\u05d5\u05dd"
    uni_run["workflowName"] = "ci\u00e9"
    ru2 = run_script(write_case("unicode", json.dumps([uni_run], ensure_ascii=False).encode("utf-8")))
    show("unicode", ru2)

    note("== long array, 50 entries, first passed ==")
    runs = [dict(REAL_RUN) for _ in range(50)]
    for i, run in enumerate(runs):
        run["databaseId"] = 40000000000 + i
    rl = run_script(write_case("longarr", json.dumps(runs)))
    show("longarr", rl)
    note(f"longarr svg == passed bytes: {rl['svg'] == passed_bytes}")

    note("== long array, first failed, rest passed (runs[0] semantics) ==")
    runs2 = [dict(x) for x in runs]
    runs2[0]["conclusion"] = "failure"
    rl2 = run_script(write_case("longarr_fail_first", json.dumps(runs2)))
    show("longarr_fail_first", rl2)
    note(f"longarr_fail_first svg == failed bytes: {rl2['svg'] == rf['svg']}")

    note("== null headSha and databaseId ==")
    null_run = dict(REAL_RUN)
    null_run["headSha"] = None
    null_run["databaseId"] = None
    rn2 = run_script(write_case("null_ids", json.dumps([null_run])))
    show("null_ids", rn2)

    note("== string databaseId ==")
    str_run = dict(REAL_RUN)
    str_run["databaseId"] = "41217224663"
    rs = run_script(write_case("str_dbid", json.dumps([str_run])))
    show("str_dbid", rs)

    note("== gh auth failure ==")
    ra = run_script(write_case("authfail", "", bat="@echo gh: To get started with GitHub CLI, please run: gh auth login\n@exit /b 1\n"))
    show("authfail", ra)
    note(f"authfail wrote no file: {ra['svg'] is None}")

    note("== gh auth failure with --offline ==")
    rao = run_script(write_case("authfail_offline", "", bat="@echo gh: auth required\n@exit /b 1\n"), argv=("--offline",))
    show("authfail_offline", rao)

    note("== gh absent from PATH ==")
    rm = run_script(CASES / "passed_real", env={"PATH": os.environ["WINDIR"] + "\\System32"}, use_case_path=False)
    show("no_gh", rm)

    note("== gh absent from PATH with --offline ==")
    rmo = run_script(CASES / "passed_real", env={"PATH": os.environ["WINDIR"] + "\\System32"}, use_case_path=False, argv=("--offline",))
    show("no_gh_offline", rmo)

    note("== real gh, live query, probe out path ==")
    rr_dir = HERE / "real"
    rr_dir.mkdir(exist_ok=True)
    rr = run_script(rr_dir, use_case_path=False)
    show("real_gh", rr)
    note(f"real_gh svg == committed bytes: {rr['svg'] == COMMITTED.read_bytes()}")

    note("== real gh ordering: limit 5 must be newest first ==")
    e = dict(os.environ)
    p = subprocess.run(
        ["gh", "run", "list", "--workflow", "ci.yml", "--branch", "main", "--limit", "5",
         "--json", "createdAt,databaseId,conclusion"],
        capture_output=True, text=True, encoding="utf-8", env=e, timeout=60,
    )
    got = json.loads(p.stdout)
    order_ok = all(got[i]["createdAt"] >= got[i + 1]["createdAt"] for i in range(len(got) - 1))
    note(f"gh_limit5 rc={p.returncode} newest_first={order_ok} count={len(got)}")

    note("== real gh: branch with zero runs returns empty list ==")
    p2 = subprocess.run(
        ["gh", "run", "list", "--workflow", "ci.yml", "--branch", "adv78b-no-such-branch", "--limit", "1",
         "--json", "status,conclusion"],
        capture_output=True, text=True, encoding="utf-8", env=e, timeout=60,
    )
    note(f"gh_empty_branch rc={p2.returncode} stdout={p2.stdout.strip()!r}")

    note("== real gh without auth: empty GH_CONFIG_DIR, tokens stripped ==")
    cfg = HERE / "ghcfg"
    cfg.mkdir(exist_ok=True)
    noauth = dict(os.environ)
    for k in ("GH_TOKEN", "GITHUB_TOKEN", "GH_ENTERPRISE_TOKEN", "GITHUB_ENTERPRISE_TOKEN"):
        noauth.pop(k, None)
    noauth["GH_CONFIG_DIR"] = str(cfg)
    out_noauth = HERE / "noauth"
    out_noauth.mkdir(exist_ok=True)
    p3 = subprocess.run(
        [sys.executable, str(SCRIPT), "--out", str(out_noauth / "ci.svg")],
        capture_output=True, text=True, encoding="utf-8", errors="backslashreplace",
        env=noauth, timeout=90,
    )
    note(f"real_noauth rc={p3.returncode} svg_exists={(out_noauth / 'ci.svg').exists()} stderr_tail={p3.stderr.strip().splitlines()[-1:]}")

    note("== real gh without auth with --offline ==")
    p4 = subprocess.run(
        [sys.executable, str(SCRIPT), "--out", str(out_noauth / "ci_offline.svg"), "--offline"],
        capture_output=True, text=True, encoding="utf-8", errors="backslashreplace",
        env=noauth, timeout=90,
    )
    off = out_noauth / "ci_offline.svg"
    note(f"real_noauth_offline rc={p4.returncode} svg_exists={off.exists()} verdict_line={p4.stdout.strip().splitlines()[:1]}")

    note("== hang: gh never answers, script must hit its own 30s timeout ==")
    hd = write_case("hang", "", bat="@ping -n 60 127.0.0.1 > nul\n")
    rh = run_script(hd)
    show("hang", rh)

    note("== xml validity of every verdict svg plus the committed file ==")
    sys.path.insert(0, str(REPO / "scripts"))
    from badgesvg import GREY, GREEN, RED, YELLOW, badge_svg

    for verdict, color in (("passed", GREEN), ("failed", RED), ("running", YELLOW), ("unknown", GREY)):
        svg = badge_svg("CI", verdict, color=color)
        try:
            xml.dom.minidom.parseString(svg)
            ok = "parses"
        except Exception as exc:
            ok = f"BREAKS: {exc}"
        note(f"xml {verdict}: {ok}")
    try:
        xml.dom.minidom.parseString(COMMITTED.read_bytes())
        note("xml committed: parses")
    except Exception as exc:
        note(f"xml committed: BREAKS: {exc}")

    (HERE / "results.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("wrote", HERE / "results.txt")


if __name__ == "__main__":
    main()
