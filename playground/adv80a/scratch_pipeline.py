import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
COMMIT = "27f75d1"
FILES = ("src/app/usage_script.c", "src/app/usage_script.h")
EXCLUDE_DIRS = (".git", "build", "artifacts", "playground")


def log(text):
    print(text, flush=True)


def copy_repo(scratch):
    cmd = ["robocopy", str(REPO), str(scratch), "/E", "/NFL", "/NDL", "/NJH", "/NJS", "/NP"]
    for name in EXCLUDE_DIRS:
        cmd += ["/XD", str(REPO / name)]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode >= 8:
        raise RuntimeError(f"robocopy failed {proc.returncode}: {proc.stdout} {proc.stderr}")


def overlay_head(scratch):
    archive = scratch.parent / "adv80a-head.tar"
    show = subprocess.run(
        ["git", "-C", str(REPO), "archive", "--format=tar", "-o", str(archive), "HEAD"],
        capture_output=True,
    )
    if show.returncode != 0:
        raise RuntimeError(f"git archive failed: {show.stderr.decode(errors='replace')}")
    with tarfile.open(archive) as tar:
        tar.extractall(scratch)
    archive.unlink()


def write_version(scratch, relative, version):
    show = subprocess.run(
        ["git", "-C", str(REPO), "show", f"{version}:{relative}"],
        capture_output=True,
    )
    if show.returncode != 0:
        raise RuntimeError(f"git show {version}:{relative} failed: {show.stderr.decode(errors='replace')}")
    (scratch / relative).write_bytes(show.stdout)


def run(scratch, args, label, timeout):
    started = time.time()
    proc = subprocess.run(
        [sys.executable, *args],
        cwd=scratch,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="backslashreplace",
        timeout=timeout,
    )
    log(f"{label}: exit {proc.returncode} in {time.time() - started:.0f}s")
    if proc.returncode != 0:
        log(f"{label} stdout tail: {proc.stdout[-3000:]}")
        log(f"{label} stderr tail: {proc.stderr[-3000:]}")
    return proc


def main():
    scratch = Path(tempfile.gettempdir()) / "adv80a-scratch"
    if scratch.exists():
        shutil.rmtree(scratch)
    scratch.mkdir(parents=True)
    code = 0
    try:
        log(f"copying repo into {scratch}")
        copy_repo(scratch)
        shutil.copytree(REPO / "build" / "vendor", scratch / "build" / "vendor")
        overlay_head(scratch)
        exe = scratch / "build" / "bin" / "bt-app.exe"
        if not exe.is_file():
            run(scratch, ["bt.py", "build"], "build", 900)
        if not exe.is_file():
            log("FAIL scratch build produced no bt-app.exe")
            return 1
        drive = run(
            scratch,
            [str(HERE / "drive_and_compare.py"), str(scratch)],
            "drive",
            1200,
        )
        code = code or drive.returncode
        redfirst = run(
            scratch,
            [str(HERE / "redfirst.py"), str(scratch)],
            "redfirst",
            1800,
        )
        code = code or redfirst.returncode
        return code
    finally:
        shutil.rmtree(scratch, ignore_errors=True)
        log("scratch purged")


if __name__ == "__main__":
    sys.exit(main())
