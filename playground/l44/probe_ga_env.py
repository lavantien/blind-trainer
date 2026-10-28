import sys

sys.path.insert(0, r"C:\Users\lavantien\dev\github\blind-trainer")

import bt

hub = bt.load_hub()
logger = bt.Logger()
for name in ("memofreq.exe", "ga-run.exe"):
    exe = bt.REPO_ROOT / "build" / "bin" / name
    code = bt.run_policy_exe(
        exe, [], f"{name}-pinned-env-probe", logger, env=bt.child_env(hub), echo=True
    )
    print(f"{name} pinned-env exit {code}", flush=True)
