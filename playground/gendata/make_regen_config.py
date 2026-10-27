import json
from pathlib import Path

root = Path(__file__).resolve().parents[2]
out_dir = root / "playground/gendata/regen"
out_dir.mkdir(parents=True, exist_ok=True)
config = json.loads((root / "config/config.json").read_text(encoding="utf-8"))
config["paths"]["data_vendor"] = str(out_dir).replace("\\", "/")
target = root / "playground/gendata/regen_config.json"
target.write_text(json.dumps(config, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
print(f"wrote {target} with data_vendor {config['paths']['data_vendor']}")
