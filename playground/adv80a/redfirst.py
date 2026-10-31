import re
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
COMMIT = "27f75d1"
FILES = ("src/app/usage_script.c", "src/app/usage_script.h")
LOG = HERE / "redfirst.log"


def log(text):
    print(text, flush=True)


def write_version(scratch, relative, version):
    show = subprocess.run(
        ["git", "-C", str(REPO), "show", f"{version}:{relative}"],
        capture_output=True,
    )
    if show.returncode != 0:
        raise RuntimeError(f"git show {version}:{relative} failed: {show.stderr.decode(errors='replace')}")
    (scratch / relative).write_bytes(show.stdout)


def run_tests(scratch, label):
    proc = subprocess.run(
        [sys.executable, "bt.py", "test-unit", "app"],
        cwd=scratch,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="backslashreplace",
        timeout=1800,
    )
    with open(LOG, "a", encoding="utf-8") as handle:
        handle.write(f"===== {label} exit {proc.returncode} =====\n")
        handle.write(proc.stdout)
        handle.write(proc.stderr)
        handle.write("\n")
    return proc.returncode, proc.stdout


def failing_suites(stdout):
    failed = set(re.findall(r"\[fail\] app_(\w+_test)", stdout))
    return failed


def main():
    scratch = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else None
    owns_scratch = scratch is None
    if scratch is None:
        scratch = REPO
    LOG.write_text("", encoding="utf-8")
    for relative in FILES:
        write_version(scratch, relative, f"{COMMIT}^")
    log("parent usage_script restored, running red pass")
    red_code, red_out = run_tests(scratch, "red, parent usage_script")
    red_failed = failing_suites(red_out)
    log(f"red pass exit {red_code}, failed suites {sorted(red_failed)}")
    for relative in FILES:
        write_version(scratch, relative, COMMIT)
    log("landed usage_script restored, running green pass")
    green_code, green_out = run_tests(scratch, "green, landed usage_script")
    green_failed = failing_suites(green_out)
    log(f"green pass exit {green_code}, failed suites {sorted(green_failed)}")
    holds = red_code != 0 and {"usage_script_test", "usage_test"} <= red_failed and green_code == 0
    log(f"red-first holds: {holds}")
    return 0 if holds else 1


if __name__ == "__main__":
    sys.exit(main())
