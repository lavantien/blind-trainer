import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
COV = REPO / "artifacts" / "lane" / "lane-cov-data" / "cov"
DEAD = re.compile(r"^\s*(\d+)\|\s+0\|")


def main():
    exes = sorted(COV.glob("bin/data_*.exe")) + sorted(COV.glob("bin/bt-hello.exe"))
    profdata = COV / "coverage.profdata"
    pattern = sys.argv[1] if len(sys.argv) > 1 else ""
    sources = sorted(p for p in (REPO / "src" / "data").rglob("*.c"))
    for source in sources:
        rel = source.as_posix()
        if pattern != "" and pattern not in rel:
            continue
        argv = ["llvm-cov", "show", f"--instr-profile={profdata}",
                "--line-coverage-lt=100", str(exes[0])]
        for exe in exes[1:]:
            argv.append(f"--object={exe}")
        argv.append(str(source))
        proc = subprocess.run(argv, cwd=REPO, capture_output=True, text=True,
                              encoding="utf-8", errors="backslashreplace")
        dead = []
        for line in (proc.stdout or "").splitlines():
            m = DEAD.match(line)
            if m:
                dead.append(int(m.group(1)))
        if not dead:
            continue
        packed = []
        for line in dead:
            if packed and line == packed[-1][1] + 1:
                packed[-1][1] = line
            else:
                packed.append([line, line])
        print(f"{source.name}: {', '.join(f'{a}-{b}' if a != b else f'{a}' for a, b in packed)}")


if __name__ == "__main__":
    sys.exit(main())
