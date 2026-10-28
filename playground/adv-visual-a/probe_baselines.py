import hashlib
import json
import sys
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[2]
BASES = REPO / "tests" / "visual" / "baselines"
SHOTS = REPO / "artifacts" / "e2e"


def main():
    print("entry probe_baselines")
    config = json.loads((REPO / "config" / "config.json").read_text(encoding="utf-8"))
    width = config["app"]["window"]["width"]
    height = config["app"]["window"]["height"]
    shots = config["e2e"]["shots"]
    slugs = set()
    hashes = {}
    problems = []
    for png in sorted(BASES.glob("*.png")):
        slug = png.stem
        slugs.add(slug)
        digest = hashlib.sha256(png.read_bytes()).hexdigest()
        hashes[slug] = digest
        with Image.open(png) as img:
            img.load()
            size_ok = img.size == (width, height)
            colors = img.convert("RGBA").getcolors(maxcolors=2)
            print(f"pair {slug} dims={img.size} mode={img.mode} sha256={digest[:12]}")
            if not size_ok:
                problems.append(f"{slug}: dims {img.size} != {width}x{height}")
            if colors is not None:
                problems.append(f"{slug}: solid blank with {colors}")
    want = set(shots)
    if slugs != want:
        problems.append(f"slug set mismatch: only-in-baselines={sorted(slugs - want)} only-in-config={sorted(want - slugs)}")
    if len(set(hashes.values())) != len(hashes):
        problems.append("duplicate hashes across distinct states")
    for slug in sorted(want):
        shot = SHOTS / f"{slug}.png"
        if shot.is_file():
            same = hashlib.sha256(shot.read_bytes()).hexdigest() == hashes.get(slug)
            print(f"regen {slug} byte-identical={same}")
        else:
            problems.append(f"{slug}: no regenerated shot to compare")
    print(f"exit probe_baselines problems={problems}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
