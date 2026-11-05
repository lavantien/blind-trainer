import argparse
import copy
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import slog

TOOL = slog.Tool("lane-build")
REPO = Path(__file__).resolve().parent.parent
HUB = REPO / "config" / "config.json"
LANE_ROOT = REPO / "artifacts" / "lane"
REDIRECTED_DIRS = ("bin", "bin_cov", "obj", "obj_cov", "gen", "test_tmp")
REDIRECTED_RAYLIB = ("obj_dir",)


def fail(message):
    return TOOL.line("fail", message)


def lane_dir(lane):
    if not lane.replace("-", "").replace("_", "").isalnum():
        raise ValueError(f"lane name must be alphanumeric with dashes: {lane}")
    return LANE_ROOT / lane


def lane_config_path(lane):
    return lane_dir(lane) / "config.json"


def write_clone(lane):
    hub = json.loads(HUB.read_text(encoding="utf-8"))
    clone = copy.deepcopy(hub)
    base = lane_dir(lane) / "build"
    for key in REDIRECTED_DIRS:
        clone["build"]["dirs"][key] = f"{base / key}".replace("\\", "/")
    if "raylib_build" not in clone["build"]:
        raise ValueError("config hub has no build.raylib_build section")
    for key in REDIRECTED_RAYLIB:
        clone["build"]["raylib_build"][key] = f"{base / 'raylib-obj'}".replace("\\", "/")
    path = lane_config_path(lane)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(clone, indent=1) + "\n", encoding="utf-8", newline="\n")
    return path


def remove_clone(lane):
    target = lane_dir(lane)
    if not target.is_dir():
        TOOL.line("absent", f"{target} already gone")
        return 0
    import shutil

    shutil.rmtree(target)
    TOOL.line("removed", f"{target}")
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(
        prog="lane_build.py",
        description="create or remove an isolated build config clone for a fan-out lane",
        allow_abbrev=False,
    )
    parser.add_argument("lane", help="lane name, alphanumeric with dashes")
    parser.add_argument(
        "--remove",
        action="store_true",
        help="delete the lane build tree and its config clone",
    )
    args = parser.parse_args(argv)
    TOOL.entry(f"lane-build {args.lane}")
    try:
        if args.remove:
            code = remove_clone(args.lane)
        else:
            path = write_clone(args.lane)
            TOOL.line("clone", f"{path}")
            print(path)
            code = 0
        TOOL.exit(f"lane-build {args.lane}", code)
        return code
    except (OSError, ValueError, KeyError) as exc:
        fail(str(exc))
        TOOL.exit(f"lane-build {args.lane}", 1)
        return 1


if __name__ == "__main__":
    sys.exit(main())
