import subprocess
import sys

GENERATED = subprocess.run(
    [sys.executable, "-I", "playground/l7_gate_literals.py"], capture_output=True, text=True, check=True
).stdout.rstrip("\n")

PATH = "tools/mutants-gate/tests.c"
LINES = open(PATH, encoding="utf-8").read().splitlines()
start = next(i for i, line in enumerate(LINES) if line.startswith("static const char FX_CAUGHT_ONLY[]"))
end = next(i for i, line in enumerate(LINES) if line.startswith("static const char FX_COMMENTED_ALLOWLIST[]"))
updated = LINES[:start] + GENERATED.splitlines() + [""] + LINES[end:]
open(PATH, "w", encoding="utf-8", newline="\n").write("\n".join(updated) + "\n")
print(f"spliced fixtures lines {start}..{end} -> {len(GENERATED.splitlines())} generated lines")
