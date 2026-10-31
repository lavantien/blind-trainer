import os
import subprocess
import sys

repo = r"C:\Users\lavantien\dev\github\blind-trainer"
cwd = sys.argv[1] if len(sys.argv) > 1 else repo
env = {k: v for k, v in os.environ.items() if not k.startswith("=")}
env["TMP"] = os.environ.get("TEMP", "")
env["TEMP"] = os.environ.get("TEMP", "")
proc = subprocess.run(
    [os.path.join(repo, "build", "bin", "core_ref_ground_path_test.exe")],
    cwd=cwd,
    env=env,
    capture_output=True,
    text=True,
)
print(proc.stdout, end="")
print(proc.stderr, end="", file=sys.stderr)
print(f"exit {proc.returncode} cwd={cwd} temp={env['TEMP']}")
sys.exit(proc.returncode)
