import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO))

import bt

AREA_RUNS = (
    ("common", None),
    ("core", None),
    ("data", None),
    ("app", None),
    ("ga", None),
    ("tools", "mutate"),
)
SUITE_LINE = re.compile(r"^suite .+: \d+ passed, \d+ failed$", re.M)


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: suite_counts_macro.py <output-file>")
    out_path = Path(sys.argv[1])
    hub = bt.load_hub()
    all_suites = bt.discover_test_suites(REPO)
    bin_dir = REPO / bt.dirs_cfg(hub)["bin"]
    timeout = bt.build_cfg(hub)["test_timeout_secs"]
    env = bt.child_env(hub)
    lines = []
    for area, only in AREA_RUNS:
        suites = bt.filter_suites(all_suites, area, only, None)
        if not suites:
            raise SystemExit(f"suite-counts: no suites under tests/unit/{area}")
        units = [(s["exe_name"], [bin_dir / f"{s['exe_name']}.exe"]) for s in suites]
        results = bt.run_units(units, jobs=1, timeout=timeout, env=env)
        for result in results:
            if result.returncode != 0:
                print(f"[fail] {result.label}", flush=True)
                print((result.output or "")[-3000:], flush=True)
        for suite, result in zip(suites, results):
            counts = SUITE_LINE.findall(result.output or "")
            verdict = "ok" if result.returncode == 0 else f"rc={result.returncode}"
            joined = " | ".join(counts)
            lines.append(f"{area} {suite['exe_name']}: {verdict} {joined}")
    out_path.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    print("\n".join(lines))
    failed = [line for line in lines if "rc=" in line]
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
