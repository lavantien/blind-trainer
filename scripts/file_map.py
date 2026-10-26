import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))

import bt

CRATE_AREA = {
    "bt-app": "app",
    "bt-core": "core",
    "bt-data": "data",
    "bt-ga": "ga",
    "bt-puzzles": "puzzles",
    "bt-scheme": "scheme",
}
DEFAULT_OUT = "artifacts/progress/t15-map.md"


def crate_file(area, rel):
    directory = "/".join(rel.parts[:-1])
    prefix = f"src/{area}/{directory}" if directory else f"src/{area}"
    stem = Path(rel).stem
    return f"{prefix}/{stem}.c", f"{prefix}/{stem}.h"


def map_crate(root, crate, area):
    rows = []
    src_dir = root / "crates" / crate / "src"
    for source in sorted(src_dir.rglob("*.rs")):
        rel = source.relative_to(src_dir)
        c_file, h_file = crate_file(area, rel)
        rows.append((source.relative_to(root).as_posix(), c_file, h_file))
    tests_dir = root / "crates" / crate / "tests"
    if tests_dir.is_dir():
        for source in sorted(tests_dir.glob("*.rs")):
            target = f"tests/unit/{area}/{source.stem}_test.c"
            rows.append((source.relative_to(root).as_posix(), target, ""))
    return rows


def map_tools(root):
    rows = []
    tools_dir = root / "tools"
    if not tools_dir.is_dir():
        return rows
    for tool_dir in sorted(p for p in tools_dir.iterdir() if p.is_dir()):
        for source in sorted((tool_dir / "src").rglob("*.rs")):
            rel = source.relative_to(root).as_posix()
            rows.append(
                (
                    rel,
                    f"tools/{tool_dir.name}/{source.stem}.c",
                    f"tools/{tool_dir.name}/{source.stem}.h",
                )
            )
        tests_dir = tool_dir / "tests"
        if tests_dir.is_dir():
            for source in sorted(tests_dir.glob("*.rs")):
                rel = source.relative_to(root).as_posix()
                rows.append((rel, f"tools/{tool_dir.name}/{source.stem}_test.c", ""))
    return rows


def map_fuzz(root):
    rows = []
    targets = root / "fuzz" / "fuzz" / "fuzz_targets"
    if targets.is_dir():
        for source in sorted(targets.glob("*.rs")):
            rel = source.relative_to(root).as_posix()
            rows.append(
                (rel, f"tests/fuzz/{source.stem}.c", f"tests/fuzz/{source.stem}.h")
            )
    standalone = root / "fuzz" / "fuzz" / "src" / "bin" / "standalone.rs"
    if standalone.is_file():
        rows.append(
            (standalone.relative_to(root).as_posix(), "tests/fuzz/standalone.c", "")
        )
    return rows


def map_e2e(root):
    rows = []
    src_dir = root / "tests" / "e2e" / "src"
    if src_dir.is_dir():
        for source in sorted(src_dir.rglob("*.rs")):
            rel = source.relative_to(root).as_posix()
            rows.append(
                (rel, f"tests/e2e/{source.stem}.c", f"tests/e2e/{source.stem}.h")
            )
    return rows


def report_text(rows):
    lines = [
        "# t15 exhaustive rust to c file map",
        "",
        "Mapping rule: crates/<crate>/src/<mod>.rs becomes src/<area>/<mod>.c plus",
        "<mod>.h with the same stem, rust tests become tests/unit/<area>/<mod>_test.c,",
        "tools/<t>/src becomes tools/<t>/*.c with main.c, fuzz targets become",
        "tests/fuzz/<name>.c, and the e2e crate becomes the c23 e2e driver.",
        "",
        "| rust source | c source | c header |",
        "|---|---|---|",
    ]
    for rust, c_file, h_file in rows:
        lines.append(f"| {rust} | {c_file} | {h_file} |")
    return "\n".join(lines) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(prog="file_map.py")
    parser.add_argument("--out", default=DEFAULT_OUT)
    args = parser.parse_args(argv if argv is not None else sys.argv[1:])
    root = bt.REPO_ROOT
    rows = []
    for crate in sorted(CRATE_AREA):
        if (root / "crates" / crate).is_dir():
            rows += map_crate(root, crate, CRATE_AREA[crate])
    rows += map_tools(root)
    rows += map_fuzz(root)
    rows += map_e2e(root)
    out_path = root / args.out
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(report_text(rows), encoding="utf-8")
    print(f"wrote {out_path}: {len(rows)} rust files mapped")
    return 0


if __name__ == "__main__":
    sys.exit(main())
