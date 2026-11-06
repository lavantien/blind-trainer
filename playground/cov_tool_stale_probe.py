import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import cov_scope_verify as measurer

cov = measurer.write_cov_config()
for tool in sorted(set(measurer.TOOLS.values())):
    print(tool, "stale" if measurer.tool_exe_stale(cov, tool) else "fresh")
