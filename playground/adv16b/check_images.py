import re
import sys

ROOT = __file__.split("playground")[0]


def rust_entries():
    text = open(ROOT + "crates/bt-app/src/views/images.rs", encoding="utf-8").read()
    start = text.index("static PAIR_IMAGES")
    end = text.index("];", start)
    body = text[start:end]
    return re.findall(r'\(\s*"([^"]*)"\s*,\s*"([^"]*)"\s*[,)]', body)


def c_entries():
    text = open(ROOT + "src/app/views/images_table.h", encoding="utf-8").read()
    start = text.index("BT_VIEW_IMAGES[BT_VIEW_IMAGES_COUNT] = {")
    end = text.index("};", start)
    body = text[start:end]
    return re.findall(r'\{\s*"([^"]*)"\s*,\s*"([^"]*)"\s*\}', body)


def main():
    rust = rust_entries()
    c = c_entries()
    failures = []
    if len(rust) != 620:
        failures.append(f"rust count {len(rust)} != 620")
    if len(c) != 620:
        failures.append(f"c count {len(c)} != 620")
    if rust != c:
        for index, (r, k) in enumerate(zip(rust, c)):
            if r != k:
                failures.append(f"row {index}: rust {r} != c {k}")
        if len(rust) != len(c):
            failures.append("row counts differ, diff truncated to shared prefix")
    keys = [k for k, _ in rust]
    if keys != sorted(set(keys)):
        failures.append("rust keys not sorted-unique")
    for key, word in c:
        if len(key) != 2 or not key.isascii() or not key.isupper():
            failures.append(f"bad c key {key!r}")
        if not word:
            failures.append(f"empty word for {key!r}")
    for line in failures[:20]:
        print(line)
    print("TABLE-OK" if not failures else f"TABLE-BAD ({len(failures)} problems)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
