import json
import sys
from pathlib import Path

src = Path(sys.argv[1])
dst = Path(sys.argv[2])
baselines = sys.argv[3]
artifacts = sys.argv[4]
extra_shot = len(sys.argv) > 5

cfg = json.loads(src.read_text(encoding="utf-8"))
cfg["paths"]["baselines"] = baselines
cfg["paths"]["artifacts"] = artifacts
if extra_shot:
    cfg["e2e"]["shots"] = [*cfg["e2e"]["shots"], "bogus-shot"]
dst.write_text(json.dumps(cfg, indent=2) + "\n", encoding="utf-8")
print("wrote", dst)
