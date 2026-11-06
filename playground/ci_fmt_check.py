import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent


def sh_bytes(argv):
    return subprocess.run(argv, cwd=REPO, capture_output=True)


def tracked_c_like():
    proc = subprocess.run(
        ["git", "ls-files", "*.c", "*.h"],
        cwd=REPO,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    return [line for line in proc.stdout.splitlines() if line]


def main():
    argv = sys.argv[1:]
    if argv:
        names = argv
    else:
        names = tracked_c_like()
    bad = []
    for name in names:
        blob = sh_bytes(["git", "show", f"HEAD:{name}"])
        if blob.returncode != 0:
            print(f"absent at HEAD: {name}")
            continue
        proc = sh_bytes(
            [
                "clang-format",
                "--dry-run",
                "--Werror",
                f"--assume-filename={name}",
            ]
        )
        checked = subprocess.run(
            proc.args,
            cwd=REPO,
            input=blob.stdout,
            capture_output=True,
        )
        if checked.returncode != 0:
            bad.append(name)
    for name in bad:
        print(f"fmt violations at HEAD: {name}")
    print(f"{len(bad)} of {len(names)} checked files violate clang-format at HEAD")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
