import json
import subprocess
import sys
import tempfile
from pathlib import Path

PROBE = Path(__file__).resolve().parent / "boundprobe.exe"
CWD = Path(__file__).resolve().parent

DEATH_CASES = {
    "die_over": "migrate count plus elite must stay at or below",
    "die_frac0": "elite must be at least 1",
    "die_frac1": "elite fraction must stay within 0 to 1",
}
PLAIN_CASES = ["islands1", "stagnation0", "migrate0", "migratemax", "restart", "scales", "fraction"]

GA = {
    "count": 2,
    "migrate_every": 1,
    "migrate_count": 1,
    "stagnation_threshold": 2,
    "swap_scale": 1.5,
    "replace_scale": 0.25,
    "policy": "fraction",
    "fraction": 0.3,
    "diversity_threshold": 0.2,
    "inject_fraction": 0.75,
}
CONFIG_LITERALS = {
    "baseline": ({"ga": {"islands": {"count": GA["count"], "migrate_every": GA["migrate_every"],
                                     "migrate_count": GA["migrate_count"]},
                         "adaptive_mutation": {"stagnation_threshold": GA["stagnation_threshold"],
                                               "swap_scale": GA["swap_scale"],
                                               "replace_scale": GA["replace_scale"]},
                         "elite_carry": {"policy": GA["policy"], "fraction": GA["fraction"]},
                         "restart": {"diversity_threshold": GA["diversity_threshold"],
                                     "inject_fraction": GA["inject_fraction"]}}}, True, None),
    "wrong_count_string": ({"ga": {"islands": {"count": "2"}}}, False, "ga.islands.count"),
    "wrong_every_string": ({"ga": {"islands": {"count": 2, "migrate_every": "1"}}}, False,
                           "ga.islands.migrate_every"),
    "wrong_count_bool": ({"ga": {"islands": {"count": 2, "migrate_every": 1, "migrate_count": True}}},
                         False, "ga.islands.migrate_count"),
    "wrong_stagnation_float": ({"ga": {"adaptive_mutation": {"stagnation_threshold": 1.5}}}, False,
                               "ga.adaptive_mutation.stagnation_threshold"),
    "wrong_swap_scale_string": ({"ga": {"adaptive_mutation": {"swap_scale": "1.0"}}}, False,
                                "ga.adaptive_mutation.swap_scale"),
    "wrong_policy_number": ({"ga": {"elite_carry": {"policy": 5}}}, False, "ga.elite_carry.policy"),
    "wrong_fraction_string": ({"ga": {"elite_carry": {"policy": "fraction", "fraction": "0.5"}}},
                              False, "ga.elite_carry.fraction"),
    "wrong_diversity_bool": ({"ga": {"restart": {"diversity_threshold": True}}}, False,
                             "ga.restart.diversity_threshold"),
    "wrong_inject_null": ({"ga": {"restart": {"inject_fraction": None}}}, False,
                          "ga.restart.inject_fraction"),
    "boundary_every0_count0": ({"ga": {"islands": {"count": 3, "migrate_every": 0,
                                                   "migrate_count": 0}}}, True, None),
    "boundary_fraction_zero": ({"ga": {"elite_carry": {"policy": "fraction", "fraction": 0.0}}},
                               True, None),
    "boundary_fraction_0999": ({"ga": {"elite_carry": {"policy": "fraction", "fraction": 0.999}}},
                               True, None),
    "boundary_fraction_one": ({"ga": {"elite_carry": {"policy": "fraction", "fraction": 1.0}}},
                              False, "ga.elite_carry.fraction"),
    "boundary_scale_max": ({"ga": {"adaptive_mutation": {"stagnation_threshold": 1,
                                                         "swap_scale": 1000000.0,
                                                         "replace_scale": 1000000.0}}}, True, None),
    "boundary_scale_over": ({"ga": {"adaptive_mutation": {"swap_scale": 1000000.5}}}, False,
                            "ga.adaptive_mutation.swap_scale"),
    "boundary_inject_edges": ({"ga": {"restart": {"diversity_threshold": 1.0,
                                                  "inject_fraction": 1.0}}}, True, None),
    "boundary_stagnation0": ({"ga": {"adaptive_mutation": {"stagnation_threshold": 0,
                                                           "swap_scale": 0.0,
                                                           "replace_scale": 0.0}}}, True, None),
    "boundary_count_min": ({"ga": {"islands": {"count": 1}}}, True, None),
    "boundary_count_zero": ({"ga": {"islands": {"count": 0}}}, False, "ga.islands.count"),
    "boundary_every_without_islands": ({"ga": {"islands": {"count": 1, "migrate_every": 1,
                                                           "migrate_count": 1}}}, False,
                                       "ga.islands.count"),
}


def run(argv, timeout=60):
    proc = subprocess.run([str(PROBE), *argv], capture_output=True, text=True, cwd=str(CWD),
                          timeout=timeout)
    return proc.returncode, proc.stdout + proc.stderr


def main():
    failures = []
    for case in PLAIN_CASES:
        code, out = run([case])
        first = out.splitlines()[0] if out.splitlines() else ""
        print(f"case {case} exit={code} {first}")
        if code != 0:
            failures.append(f"case {case} exited {code}: {out[-500:]}")
        if "monotone=0" in out or "MISMATCH" in out:
            failures.append(f"case {case} invariant broken: {out}")
    for case, want in DEATH_CASES.items():
        code, out = run([case])
        ok = code == 134 and want in out and "abort:" in out
        print(f"death {case} exit={code} hit={want in out}")
        if not ok:
            failures.append(f"death {case} exit={code} want={want!r} out={out[-300:]}")
    code, out = run(["die_wrap"], timeout=90)
    print(f"wrap exit={code} out={out[-400:]!r}")
    with tempfile.TemporaryDirectory() as tmp:
        for name, (payload, accept, pointer) in CONFIG_LITERALS.items():
            path = Path(tmp) / f"{name}.json"
            path.write_text(json.dumps(payload), encoding="utf-8")
            code, out = run(["cfg", str(path)])
            line = out.splitlines()[0] if out.splitlines() else ""
            got_accept = "accept=1" in line
            ok = got_accept == accept and (pointer is None or pointer in line)
            print(f"cfg {name} exit={code} ok={ok} {line[:160]}")
            if code != 0 or not ok:
                failures.append(f"cfg {name} exit={code} line={line[:200]}")
    if failures:
        print("FAILURES:")
        for line in failures:
            print(f"  {line}")
        return 1
    print("boundary sweep done")
    return 0


if __name__ == "__main__":
    sys.exit(main())
