import json
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
TARGET = "x86_64-pc-windows-msvc"
CFLAGS = ["-Wall", "-Wextra", "-Werror", "-ffp-contract=off", "-g0", "-O2", "-DNDEBUG",
          "-D_CRT_SECURE_NO_WARNINGS"]
COV_FLAGS = ["-fprofile-instr-generate", "-fcoverage-mapping"]


def sh(argv, env=None, timeout=120):
    proc = subprocess.run(
        [str(a) for a in argv], capture_output=True, text=True, env=env, timeout=timeout
    )
    print(f"$ {' '.join(str(a) for a in argv)}")
    print(f"  code={proc.returncode}")
    if proc.returncode != 0:
        print(((proc.stderr or "") + (proc.stdout or ""))[-2000:])
    return proc


def main():
    src = HERE / "a.c"
    src.write_text(
        "#include <stdio.h>\n\nstatic int classify(int v) {\n"
        "    if (v > 10) {\n        return 2;\n    }\n    return v % 2;\n}\n\n"
        "int main(void) {\n"
        '    printf("%d\\n", classify(3) + classify(30));\n'
        "    return 0;\n}\n",
        encoding="utf-8",
    )
    exe = HERE / "a.exe"
    argv = ["clang", "-target", TARGET, "-std=c23", *CFLAGS, *COV_FLAGS,
            str(src), "-o", str(exe), "-fuse-ld=lld-link",
            "-Xlinker", "/SUBSYSTEM:CONSOLE"]
    if sh(argv).returncode != 0:
        print("FAIL: instrumented link failed")
        return 1
    raw = HERE / "a.profraw"
    env = {**os.environ, "LLVM_PROFILE_FILE": str(raw)}
    if sh([exe], env=env).returncode != 0:
        print("FAIL: instrumented run failed")
        return 1
    if not raw.is_file():
        print("FAIL: no profraw written")
        return 1
    print(f"profraw bytes: {raw.stat().st_size}")
    prof = HERE / "a.profdata"
    if sh(["llvm-profdata", "merge", f"--output={prof}", str(raw)]).returncode != 0:
        print("FAIL: profdata merge failed")
        return 1
    out = HERE / "cov.json"
    proc = sh(["llvm-cov", "export", f"--instr-profile={prof}", str(exe)])
    if proc.returncode != 0:
        print("FAIL: llvm-cov export failed")
        return 1
    out.write_text(proc.stdout, encoding="utf-8")
    data = json.loads(proc.stdout)["data"][0]
    print(f"totals regions: {data['totals']['regions']}")
    first = data["files"][0]
    print(f"file keys: {sorted(first)}")
    print(f"file summary regions: {first['summary']['regions']}")
    fn = data["functions"][0]
    print(f"function keys: {sorted(fn)}")
    print(f"function sample: name={fn['name']!r} count={fn['count']} filenames={fn['filenames']}")
    print(f"file filename: {first['filename']}")
    print("OK: full chain works")


if __name__ == "__main__":
    sys.exit(main())
