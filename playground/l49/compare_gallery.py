import hashlib
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
OLD = Path(__file__).parent / "head_shots"


def main():
    OLD.mkdir(exist_ok=True)
    shots = sorted((REPO / "docs" / "screenshots").glob("*.png"))
    changed = []
    same = []
    for shot in shots:
        old = OLD / shot.name
        if not old.is_file():
            old.write_bytes(
                subprocess.run(
                    ["git", "-C", str(REPO), "show", f"HEAD:docs/screenshots/{shot.name}"],
                    capture_output=True, check=True,
                ).stdout
            )
        a = hashlib.sha256(shot.read_bytes()).hexdigest()
        b = hashlib.sha256(old.read_bytes()).hexdigest()
        (changed if a != b else same).append(shot.name)
    for name in changed:
        print(f"changed: {name}")
    for name in same:
        print(f"identical: {name}")
    print(f"gallery vs HEAD: {len(changed)} changed, {len(same)} identical, {len(shots)} total")
    return 0


if __name__ == "__main__":
    sys.exit(main())
