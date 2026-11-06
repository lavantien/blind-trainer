import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
LANE_CONFIG = REPO / "artifacts" / "lane" / "lane-cov-app" / "config.json"
ASAN_CONFIG = REPO / "artifacts" / "lane" / "lane-cov-app" / "asan-config.json"


def main():
    clone = json.loads(LANE_CONFIG.read_text(encoding="utf-8"))
    build = clone["build"]
    for flag in ("-fsanitize=address", "-fsanitize-recover=address", "-fno-omit-frame-pointer"):
        if flag not in build["cflags"]:
            build["cflags"].append(flag)
    build["link_flags"] = [flag for flag in build["link_flags"] if "DEBUG" not in flag]
    build["release_flags"] = ["-O1"]
    for flag in ("-fsanitize=address",):
        if flag not in build["link_flags"]:
            build["link_flags"].append(flag)
    ASAN_CONFIG.write_text(json.dumps(clone, indent=1) + "\n", encoding="utf-8", newline="\n")
    print(ASAN_CONFIG)


if __name__ == "__main__":
    sys.exit(main())
