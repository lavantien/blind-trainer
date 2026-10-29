import re
from pathlib import Path

rust = Path("crates/bt-app/src/views/images.rs").read_text(encoding="utf-8")
strict = re.compile(r'\(\s*"([^"]+)"\s*,\s*"((?:[^"\\]|\\.)*)"\s*\)')
loose = re.compile(r'\(\s*"([^"]+)"\s*,\s*"([^"]*)"\s*\)')
p1 = strict.findall(rust)
p2 = loose.findall(rust)
print("strict", len(p1), "loose", len(p2))
print("strict SH:", any(k == "SH" for k, _ in p1), "loose SH:", any(k == "SH" for k, _ in p2))
