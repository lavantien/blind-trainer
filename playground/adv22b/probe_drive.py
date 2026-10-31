import subprocess
import sys
from pathlib import Path

PROBE = Path(__file__).resolve().parent / "faultprobe.exe"
CWD = Path(__file__).resolve().parent

ABORT_SITES = [
    "ga.run.scratch",
    "ga.run.islands",
    "ga.run.stagnation",
    "ga.run.island_best",
    "ga.run.population",
    "ga.run.individual",
    "ga.run.scores",
    "ga.run.order",
    "ga.run.best",
    "ga.run.per_gen",
    "ga.run.next",
    "ga.run.is_elite",
    "ga.run.child",
    "ga.run.outcome",
    "ga.ox.fill",
    "ga.mutate_replace.used",
]
MIGRANT_SITES = ["ga.run.migrants", "ga.run.migrant_scores", "ga.run.migrant"]
DIVERSITY_SITES = ["ga.run.diversity_order"]
GRACEFUL = {
    "ga.build_env.conf_table": ("envonly", "conf table allocation failed"),
    "ga.build_env.flow_table": ("envonly", "flow table allocation failed"),
    "ga.dist.read_buffer": ("loaders", "allocation failed reading probe-freq.json"),
    "ga.dist.cases": ("loaders", "freq cases allocation failed"),
    "ga.dist.adjacent": ("loaders", "freq adjacent allocation failed"),
    "ga.dist.cooccurring": ("loaders", "freq cooccurring allocation failed"),
    "ga.read_file.buffer": ("loaders", "allocation failed reading probe-words.txt"),
    "ga.strset.items": ("loaders", "wordlist items allocation failed at entry"),
    "ga.strset.entry": ("loaders", "wordlist entry allocation failed at entry"),
    "ga.freqs.keys": ("loaders", "frequency keys allocation failed at row"),
    "ga.freqs.values": ("loaders", "frequency values allocation failed at row"),
    "ga.freqs.entry": ("loaders", "frequency entry allocation failed at row"),
    "ga.build_inventory.sorted": ("loaders", "inventory sorted allocation failed"),
    "ga.build_inventory.list": ("loaders", "inventory list allocation failed"),
    "ga.build_inventory.covered": ("loaders", "inventory covered allocation failed"),
    "ga.build_inventory.word_copy": ("loaders", "inventory word copy allocation failed"),
    "ga.build_pool.ranked": ("loaders", "allocation failed at ga.build_pool.ranked"),
}


def run(mode, fail_site=None):
    argv = [str(PROBE), mode] + ([fail_site] if fail_site else [])
    proc = subprocess.run(argv, capture_output=True, text=True, cwd=str(CWD), timeout=120)
    return proc.returncode, proc.stdout + proc.stderr


def main():
    failures = []
    seen_union = set()
    for mode in ("legacy", "upgraded", "fraction", "loaders", "envonly"):
        code, out = run(mode)
        tail = [line for line in out.splitlines() if line.startswith("probe ")]
        sites = {line.split(" ", 1)[1] for line in out.splitlines() if line.startswith("site ")}
        seen_union |= sites
        balance = [line for line in out.splitlines() if line.startswith("probe allocs=")]
        print(f"mode {mode} exit={code} {balance}")
        if code != 0:
            failures.append(f"mode {mode} exited {code}")
            continue
        if "live=0" not in (balance[0] if balance else ""):
            failures.append(f"mode {mode} leaked: {balance}")

    claimed = set(ABORT_SITES) | set(MIGRANT_SITES) | set(DIVERSITY_SITES) | set(GRACEFUL)
    unreached = claimed - seen_union
    print(f"seen union covers {len(seen_union)} of {len(claimed)} claimed sites")
    if unreached:
        failures.append(f"sites never reached in any no-fault mode: {sorted(unreached)}")

    for site in ABORT_SITES:
        code, out = run("upgraded", site)
        want = f"allocation failed at {site}"
        ok = code == 134 and want in out and "abort:" in out
        print(f"fault {site} exit={code} hit={want in out}")
        if not ok:
            failures.append(f"fault {site} exit={code} message={'hit' if want in out else 'miss'}")
    for site in MIGRANT_SITES:
        code, out = run("upgraded", site)
        want = f"allocation failed at {site}"
        ok = code == 134 and want in out
        print(f"fault {site} exit={code} hit={want in out}")
        if not ok:
            failures.append(f"fault {site} exit={code} message={'hit' if want in out else 'miss'}")
    for site in DIVERSITY_SITES:
        code, out = run("upgraded", site)
        want = f"allocation failed at {site}"
        ok = code == 134 and want in out
        print(f"fault {site} exit={code} hit={want in out}")
        if not ok:
            failures.append(f"fault {site} exit={code} message={'hit' if want in out else 'miss'}")
    for site, (mode, message) in GRACEFUL.items():
        code, out = run(mode, site)
        ok = message in out
        print(f"fault {site} exit={code} hit={ok}")
        if not ok:
            failures.append(f"fault {site} exit={code} message missed: {message}")

    if failures:
        print("FAILURES:")
        for line in failures:
            print(f"  {line}")
        return 1
    print("all fault sites verified")
    return 0


if __name__ == "__main__":
    sys.exit(main())
