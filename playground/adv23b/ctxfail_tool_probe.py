import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

import boundary_probes as bp

lane, config = bp.make_lane("cuda-ctxfail-neg1", lambda hub: hub["cuda"].update(enable=True))
proc = bp.run_ga(bp.CUDA_GA, config, lane, "ga-cuda-ctxfail-neg1.log",
                 extra_env={"CUDA_VISIBLE_DEVICES": "-1"})
ref_lane = bp.SCRATCH / "lanes" / "cpu-ref"
ref = bp.lane_state(ref_lane)
state = bp.lane_state(lane)
print(f"exit={proc.returncode} outputs_match_cpu={state == ref} stderr_empty="
      f"{not proc.stderr.strip()}")
print((proc.stdout or "").strip())
sys.exit(0 if proc.returncode == 0 and state == ref else 1)
