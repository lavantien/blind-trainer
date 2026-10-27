import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SOURCE = REPO / "crates/bt-app/src/views/images.rs"
TARGET = REPO / "src/app/views/images_table.h"


def main():
    text = SOURCE.read_text(encoding="utf-8")
    start = text.index("static PAIR_IMAGES")
    end = text.index("];", start)
    flat = " ".join(line.strip() for line in text[start:end].splitlines())
    entries = re.findall(r'\(\s*"([A-Z]{2})",\s*"([^"]*)"\s*,?\s*\),', flat)
    if len(entries) != 620:
        sys.exit(f"expected 620 entries, parsed {len(entries)}")
    keys = [key for key, _ in entries]
    if keys != sorted(keys) or len(set(keys)) != len(keys):
        sys.exit("table not sorted and unique")
    out = [
        "#ifndef BT_APP_VIEWS_IMAGES_TABLE_H",
        "#define BT_APP_VIEWS_IMAGES_TABLE_H",
        "",
        '#include "app/views/images.h"',
        "",
        "const bt_view_image_entry BT_VIEW_IMAGES[BT_VIEW_IMAGES_COUNT] = {",
    ]
    for key, word in entries:
        out.append(f'    {{"{key}", "{word}"}},')
    out += ["};", "", "#endif", ""]
    TARGET.write_text("\n".join(out), encoding="utf-8", newline="\n")
    print(f"wrote {len(entries)} entries to {TARGET}")


if __name__ == "__main__":
    main()
