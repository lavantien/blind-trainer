import argparse
import json
import shutil
import sys
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import slog

TOOL = slog.Tool("dist")
REPO = Path(__file__).resolve().parent.parent
ZIP_PREFIX = "blind-trainer"
ZIP_SUFFIX = "windows-x64.zip"


def fail(message):
    return TOOL.line("fail", message)


def release_config():
    path = REPO / "config" / "config.json"
    doc = json.loads(path.read_text(encoding="utf-8"))
    return doc.get("release", {})


def staging_plan(repo, dist_root, release):
    """ordered (src, dst) copy list for the staged dist tree; raises when exe, config or fonts are missing."""
    staging = Path(dist_root) / release["staging_dir"]
    exe_src = Path(repo) / "build" / "bin" / release["exe"]
    if not exe_src.is_file():
        raise FileNotFoundError(f"{exe_src} missing, run python bt.py build first")
    config_src = Path(repo) / release["config_file"]
    if not config_src.is_file():
        raise FileNotFoundError(f"{config_src} missing")
    plan = [
        (exe_src, staging / release["exe"]),
        (config_src, staging / release["config_file"]),
    ]
    fonts = Path(repo) / release["fonts_dir"]
    if not fonts.is_dir():
        raise FileNotFoundError(f"{fonts} missing")
    font_files = sorted(child for child in fonts.iterdir() if child.is_file())
    if not font_files:
        raise FileNotFoundError(f"{fonts} has no font files")
    for font in font_files:
        plan.append((font, staging / release["fonts_dir"] / font.name))
    return plan


def zip_path(dist_root, version):
    return Path(dist_root) / f"{ZIP_PREFIX}-{version}-{ZIP_SUFFIX}"


def build_zip(staging_dir, target):
    if target.exists():
        target.unlink()
    with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(Path(staging_dir).rglob("*")):
            if path.is_file():
                archive.write(path, path.relative_to(Path(staging_dir).parent))
    return target


def main(argv=None):
    parser = argparse.ArgumentParser(
        prog="dist.py",
        description="stage the exe, fonts and config into the dist tree and zip it",
        allow_abbrev=False,
    )
    parser.add_argument("-Version", "--version", dest="version", required=True)
    args = parser.parse_args(argv)
    TOOL.entry("dist", f"version={args.version}")
    release = release_config()
    dist_root = REPO / "artifacts" / "dist"
    try:
        plan = staging_plan(REPO, dist_root, release)
        staging = Path(dist_root) / release["staging_dir"]
        if staging.exists():
            shutil.rmtree(staging)
        for src, dst in plan:
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)
        target = build_zip(staging, zip_path(dist_root, args.version))
    except FileNotFoundError as exc:
        fail(str(exc))
        return 1
    except (KeyError, OSError, ValueError) as exc:
        fail(f"dist failed: {exc}")
        return 2
    print(f"{target.name} {target.stat().st_size}")
    TOOL.exit("dist", 0)
    return 0


if __name__ == "__main__":
    sys.exit(main())
