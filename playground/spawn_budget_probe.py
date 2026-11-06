import subprocess
import sys
import time
import os

exe = sys.argv[1]
config = sys.argv[2]
budgets = [int(value) for value in sys.argv[3].split(",")]
env = dict(os.environ)
env["BT_CONFIG"] = config
for budget in budgets:
    env["BT_MEMOFREQ_ALLOC_FAIL_OK"] = str(budget)
    started = time.monotonic()
    try:
        proc = subprocess.run(
            [exe], env=env, capture_output=True, text=True, timeout=20, cwd=os.getcwd()
        )
        elapsed = time.monotonic() - started
        tail = (proc.stderr or proc.stdout).strip().splitlines()
        print(f"budget {budget}: exit {proc.returncode} in {elapsed:.2f}s :: {tail[:1]}")
    except subprocess.TimeoutExpired:
        elapsed = time.monotonic() - started
        print(f"budget {budget}: TIMEOUT after {elapsed:.2f}s")
