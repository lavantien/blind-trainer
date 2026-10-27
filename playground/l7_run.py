import subprocess
import sys

argv = sys.argv[1:]
proc = subprocess.run(argv, capture_output=True, text=True, timeout=600)
sys.stdout.write(proc.stdout)
sys.stderr.write(proc.stderr)
print(f"probe-exit={proc.returncode}")
