import re
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[2]


def parse(path):
    regions = {}
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        head = re.match(r"== (\S+)", line)
        if head:
            current = head.group(1)
            continue
        fn = re.match(r"\s+fn=(\S+) fn_count=\d+: (.*)", line)
        if fn:
            for span in re.findall(r"\[([0-9]+:[0-9]+-[0-9]+:[0-9]+)\] (\S+)", fn.group(2)):
                regions[(current, fn.group(1), span[0])] = span[1]
    return regions


def main():
    before = parse(root / "artifacts/progress/gendata-gen4-regions3.log")
    after = parse(root / sys.argv[1])
    closed = sorted(set(before) - set(after))
    opened = sorted(set(after) - set(before))
    print(f"before={len(before)} after={len(after)} closed={len(closed)} opened={len(opened)}")
    for key in closed:
        print(f"closed {key[0]} {key[1]} [{key[2]}] was {before[key]}")
    for key in opened:
        print(f"OPENED {key[0]} {key[1]} [{key[2]}] is {after[key]}")
    still = {}
    for key in after:
        still[key] = after[key]
    by_file = {}
    for (file, fn, span), tag in still.items():
        by_file.setdefault(file, set()).add((fn, span, tag))
    for file in sorted(by_file):
        print(f"== still dark {file}: {len(by_file[file])}")
        for fn, span, tag in sorted(by_file[file]):
            print(f"  {fn} [{span}] {tag}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
