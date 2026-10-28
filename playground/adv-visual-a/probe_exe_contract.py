import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
EXE = REPO / "build" / "bin" / "visual-diff.exe"
ROOT = Path(__file__).resolve().parent
REAL_SHOTS = REPO / "artifacts" / "e2e"


def stage(name):
    path = ROOT / name
    if path.is_dir():
        shutil.rmtree(path)
    path.mkdir(parents=True)
    return path


def run_exe(argv, env_extra=None, label=""):
    env = dict(os.environ)
    env.pop("BT_CONFIG", None)
    env.pop("BT_ARTIFACTS", None)
    env.update(env_extra or {})
    proc = subprocess.run([str(EXE), *argv], capture_output=True, text=True, env=env, cwd=str(ROOT))
    print(f"[{label}] exit={proc.returncode}")
    out = (proc.stdout + proc.stderr).strip()
    for line in out.splitlines():
        print(f"[{label}]   {line}")
    return proc.returncode, out


def case_both_absent():
    print("entry case_both_absent")
    code, _ = run_exe([str(ROOT / "absent-a"), str(ROOT / "absent-b")], label="both-absent")
    print(f"exit case_both_absent code={code} verdict={'GREEN' if code == 0 else 'FAIL'}")


def case_both_empty():
    print("entry case_both_empty")
    a = stage("empty-a")
    b = stage("empty-b")
    code, _ = run_exe([str(a), str(b)], label="both-empty")
    print(f"exit case_both_empty code={code} verdict={'GREEN' if code == 0 else 'FAIL'}")


def case_bases_empty():
    print("entry case_bases_empty")
    a = stage("shots-c")
    b = stage("bases-c")
    shutil.copyfile(REAL_SHOTS / "menu.png", a / "menu.png")
    code, out = run_exe([str(a), str(b)], label="bases-empty")
    print(f"exit case_bases_empty code={code}")


def case_extra_png():
    print("entry case_extra_png")
    a = stage("shots-d")
    b = stage("bases-d")
    shutil.copyfile(REAL_SHOTS / "menu.png", a / "menu.png")
    shutil.copyfile(REAL_SHOTS / "menu.png", a / "extra.png")
    shutil.copyfile(REAL_SHOTS / "menu.png", b / "menu.png")
    code, out = run_exe([str(a), str(b)], label="extra-png")
    print(f"exit case_extra_png code={code}")


def case_corrupt_baseline():
    print("entry case_corrupt_baseline")
    a = stage("shots-e")
    b = stage("bases-e")
    shutil.copyfile(REAL_SHOTS / "menu.png", a / "menu.png")
    data = bytearray((REAL_SHOTS / "menu.png").read_bytes())
    data[len(data) // 2] ^= 0xFF
    (b / "menu.png").write_bytes(data)
    code, out = run_exe([str(a), str(b)], label="corrupt-baseline")
    print(f"exit case_corrupt_baseline code={code}")


def case_update_stale_removal():
    print("entry case_update_stale_removal")
    a = stage("shots-f")
    b = stage("bases-f")
    shutil.copyfile(REAL_SHOTS / "menu.png", a / "menu.png")
    shutil.copyfile(REAL_SHOTS / "menu.png", b / "menu.png")
    shutil.copyfile(REAL_SHOTS / "menu.png", b / "stale-old.png")
    (b / "keep-me.md").write_text("doc", encoding="utf-8")
    reports = stage("reports-f")
    code, out = run_exe(["--update", str(a), str(b)], {"BT_ARTIFACTS": str(reports)}, "update-stale")
    left = sorted(p.name for p in b.iterdir())
    print(f"exit case_update_stale_removal code={code} left={left}")


def case_artifacts_hijack():
    print("entry case_artifacts_hijack")
    a = stage("shots-g")
    b = stage("bases-g")
    shutil.copyfile(REAL_SHOTS / "menu.png", a / "menu.png")
    shutil.copyfile(REAL_SHOTS / "menu.png", b / "menu.png")
    hijack = stage("hijack-g")
    code, out = run_exe([str(a), str(b)], {"BT_ARTIFACTS": str(hijack)}, "artifacts-hijack")
    wrote = sorted(str(p.relative_to(hijack)) for p in hijack.rglob("*") if p.is_file())
    print(f"exit case_artifacts_hijack code={code} wrote_under_hijack={wrote}")


def case_selftest():
    print("entry case_selftest")
    work = stage("selftest-work")
    reports = stage("reports-selftest")
    code, out = run_exe(["--selftest", str(work)], {"BT_ARTIFACTS": str(reports)}, "selftest")
    print(f"exit case_selftest code={code}")


def main():
    print(f"entry probe_exe_contract exe={EXE}")
    case_both_absent()
    case_both_empty()
    case_bases_empty()
    case_extra_png()
    case_corrupt_baseline()
    case_update_stale_removal()
    case_artifacts_hijack()
    case_selftest()
    print("exit probe_exe_contract 0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
