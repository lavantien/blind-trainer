import argparse
import os
import subprocess
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
LOCK = REPO_ROOT / "artifacts" / "commit.lock"
STALE_SECS = 120


def sh(argv, check=True):
    proc = subprocess.run(
        argv,
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="backslashreplace",
    )
    if check and proc.returncode != 0:
        sys.stderr.write(proc.stdout + proc.stderr)
        raise SystemExit(f"commit_lane: command failed: {' '.join(argv)}")
    return proc


def lock_is_stale():
    try:
        age = time.time() - LOCK.stat().st_mtime
    except OSError:
        return True
    return age > STALE_SECS


def acquire():
    LOCK.parent.mkdir(parents=True, exist_ok=True)
    for _ in range(600):
        try:
            fd = os.open(str(LOCK), os.O_CREAT | os.O_EXCL | os.O_WRONLY)
            os.write(fd, f"{os.getpid()}\n".encode())
            os.close(fd)
            return
        except FileExistsError:
            if lock_is_stale():
                sys.stderr.write("commit_lane: breaking stale commit lock\n")
                LOCK.unlink(missing_ok=True)
                continue
            time.sleep(0.5)
    raise SystemExit("commit_lane: commit lock busy for 300s")


def release():
    LOCK.unlink(missing_ok=True)


def staged_files():
    proc = sh(["git", "diff", "--cached", "--name-only"], check=False)
    return [line for line in proc.stdout.splitlines() if line]


def main():
    parser = argparse.ArgumentParser(
        prog="commit_lane.py",
        description="stage exact paths, take the commit lock, verify the staged list, commit",
    )
    parser.add_argument("-m", "--message", required=True, help="conventional commit message")
    parser.add_argument("paths", nargs="+", help="repo-relative files or directories to stage")
    args = parser.parse_args()

    acquire()
    try:
        sh(["git", "add", "-f", "--"] + list(args.paths))
        staged = staged_files()
        intended = set()
        for path in args.paths:
            target = REPO_ROOT / path
            if target.is_dir():
                intended.update(
                    p.relative_to(REPO_ROOT).as_posix()
                    for p in target.rglob("*")
                    if p.is_file()
                )
            else:
                intended.add(Path(path).as_posix())
        foreign = [f for f in staged if f not in intended]
        if foreign:
            raise SystemExit(
                "commit_lane: foreign staged files, refusing: " + ", ".join(foreign)
            )
        if not staged:
            raise SystemExit("commit_lane: nothing staged from the given paths")
        proc = sh(["git", "commit", "-m", args.message, "--"] + list(args.paths))
        sys.stdout.write(proc.stdout)
        committed = sh(
            ["git", "show", "--name-only", "--format=", "HEAD"]
        ).stdout.splitlines()
        stray = [f for f in committed if f not in intended]
        if stray:
            raise SystemExit(
                "commit_lane: commit swept foreign files: " + ", ".join(stray)
            )
    finally:
        release()


if __name__ == "__main__":
    main()
