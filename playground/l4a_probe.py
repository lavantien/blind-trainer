import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build" / "playground-l4a"
OBJ = BUILD / "obj"
BIN = BUILD / "bin"

FLAGS = [
    "-target",
    "x86_64-pc-windows-msvc",
    "-std=c23",
    "-Wall",
    "-Wextra",
    "-Werror",
    "-ffp-contract=off",
    "-g0",
    "-O2",
    "-DNDEBUG",
    "-D_CRT_SECURE_NO_WARNINGS",
]

COMMON = [
    "src/common/bt_trace.c",
    "src/common/strbuf.c",
    "src/common/rng.c",
    "src/common/paths.c",
    "src/common/json.c",
    "src/common/config.c",
    "src/common/bt_harness.c",
]
CORE = ["src/core/alg.c", "src/core/cube.c", "src/core/moves.c"]
PUZZLES = [
    "src/puzzles/geometry.c",
    "src/puzzles/tier1.c",
    "src/puzzles/nxn.c",
    "src/puzzles/sq1.c",
]
SUITES = {
    "puzzles_geometry_test": "tests/unit/puzzles/geometry_test.c",
    "puzzles_tier1_test": "tests/unit/puzzles/tier1_test.c",
    "puzzles_nxn_oracle_test": "tests/unit/puzzles/nxn_oracle_test.c",
    "puzzles_sq1_oracle_test": "tests/unit/puzzles/sq1_oracle_test.c",
}


def run(argv, label, env=None):
    proc = subprocess.run(
        [str(part) for part in argv],
        capture_output=True,
        text=True,
        cwd=str(ROOT),
        env=env,
    )
    if proc.returncode != 0:
        tail = (proc.stderr or "") + (proc.stdout or "")
        print(f"[fail] {label}\n{tail[-3000:]}")
        sys.exit(1)
    return proc


def main():
    clang = "clang"
    OBJ.mkdir(parents=True, exist_ok=True)
    BIN.mkdir(parents=True, exist_ok=True)
    objects = []
    for rel in COMMON + CORE + PUZZLES:
        obj = OBJ / (Path(rel).stem + ".obj")
        run(
            [clang, *FLAGS, "-I", "src", "-I", "tests", "-c", rel, "-o", str(obj)],
            f"compile {rel}",
        )
        objects.append(obj)
    for suite, rel in SUITES.items():
        test_obj = OBJ / (suite + ".obj")
        run(
            [clang, *FLAGS, "-I", "src", "-I", "tests", "-c", rel, "-o", str(test_obj)],
            f"compile {rel}",
        )
        header = Path(rel).read_text(encoding="utf-8")
        pairs = re.findall(
            r"BT_TEST\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*,\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)",
            header,
        )
        pairs = sorted(set(pairs))
        count = 0
        lines = ['#include "common/bt_harness.h"', ""]
        for group, name in pairs:
            lines.append(f"extern void bt_test_{group}_{name}(void);")
        lines += ["", "int main(void) {", "    bt_test_entry tests[] = {"]
        for group, name in pairs:
            lines.append(f'        {{"{group}.{name}", bt_test_{group}_{name}}},')
            count += 1
        lines += [
            "    };",
            f'    return bt_harness_run("{suite}", tests, {count});',
            "}",
            "",
        ]
        main_source = BUILD / f"{suite}_main.c"
        main_source.write_text("\n".join(lines), encoding="utf-8")
        main_obj = OBJ / (suite + "_main.obj")
        run(
            [clang, *FLAGS, "-I", "src", "-c", str(main_source), "-o", str(main_obj)],
            f"compile {suite} main",
        )
        exe = BIN / f"{suite}.exe"
        run(
            [
                clang,
                *FLAGS,
                "-fuse-ld=lld-link",
                "-Xlinker",
                "/SUBSYSTEM:CONSOLE",
                *[str(o) for o in objects],
                str(test_obj),
                str(main_obj),
                "-o",
                str(exe),
            ],
            f"link {suite}",
        )
        env = dict(os.environ)
        env["BT_REF_GROUND"] = "ref_ground"
        proc = run([str(exe)], f"run {suite}", env)
        print(f"[pass] {suite}: {proc.stdout.strip().splitlines()[-1]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
