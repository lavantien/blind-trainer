import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import cov_scope_verify as measurer

cov = measurer.write_cov_config()
for tool in sorted(set(measurer.TOOLS.values())):
    exe = Path(cov["build"]["dirs"]["bin_cov"]) / f"{tool}.exe"
    if not exe.is_file():
        print(tool, "exe missing")
        continue
    exe_time = exe.stat().st_mtime
    sources, inputs = measurer.tool_link_inputs(cov, tool)
    newer = [
        str(p.relative_to(measurer.REPO))
        for p in [*sources, *inputs]
        if not p.is_file() or p.stat().st_mtime >= exe_time
    ]
    print(tool, "exe", time.strftime("%H:%M:%S", time.localtime(exe_time)), "stale inputs:", newer)
