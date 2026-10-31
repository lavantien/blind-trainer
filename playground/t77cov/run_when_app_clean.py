import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WATCH = ("src/app", "tests/unit/app")
POLL_SECS = 20
MAX_WAITS = 120
ATTEMPTS = 3


def sh(argv):
    return subprocess.run(
        argv, cwd=REPO, capture_output=True, text=True, encoding="utf-8",
        errors="backslashreplace",
    )


def app_dirty():
    proc = sh(["git", "status", "--porcelain", "--", *WATCH])
    return bool(proc.stdout.strip())


def main():
    log = open(REPO / "artifacts" / "progress" / "t77-cov-run3.log", "w", encoding="utf-8")
    for attempt in range(1, ATTEMPTS + 1):
        waits = 0
        while app_dirty() and waits < MAX_WAITS:
            if waits == 0:
                log.write(f"attempt {attempt}: app tree dirty, waiting\n")
                log.flush()
            time.sleep(POLL_SECS)
            waits += 1
        if app_dirty():
            log.write(f"attempt {attempt}: app tree stayed dirty for {MAX_WAITS} polls\n")
            log.close()
            return 3
        log.write(f"attempt {attempt}: app tree clean, running coverage\n")
        log.flush()
        proc = subprocess.run(
            [sys.executable, "bt.py", "coverage"], cwd=REPO, stdout=log,
            stderr=subprocess.STDOUT, text=True,
        )
        log.flush()
        if proc.returncode == 0:
            log.write(f"EXIT:0\n")
            log.close()
            return 0
        log.write(f"coverage exited {proc.returncode}\n")
        log.flush()
    log.write("EXIT:1\n")
    log.close()
    return 1


if __name__ == "__main__":
    sys.exit(main())
