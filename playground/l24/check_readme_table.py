import re
import subprocess
from pathlib import Path

repo = Path(__file__).resolve().parents[2]
help_out = subprocess.run(
    ["python", "bt.py", "help"], capture_output=True, text=True, cwd=repo
).stdout
help_rows = [line for line in help_out.strip().splitlines()[3:] if line.strip()]

text = (repo / "README.md").read_text(encoding="utf-8")
start = text.index("help                     show this help")
end = text.index("clean-c ", start)
end = text.index("\n", end)
readme_rows = [line for line in text[start:end].splitlines() if line.strip()]

if help_rows == readme_rows:
    print(f"match: {len(help_rows)} rows")
else:
    print("MISMATCH")
    for a, b in zip(help_rows, readme_rows):
        if a != b:
            print(f"help:   {a}")
            print(f"readme: {b}")
    print(f"lengths help={len(help_rows)} readme={len(readme_rows)}")
