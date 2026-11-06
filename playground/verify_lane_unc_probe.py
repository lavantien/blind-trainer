import os
import subprocess

root = os.getcwd()
value = "\\\\127.0.0.1\\no-such-share\\" + "a\\" * 1100
env = os.environ.copy()
env["BT_ARTIFACTS"] = value
env.pop("BT_REF_GROUND", None)
env.pop("BT_CONFIG", None)
exe = os.path.join(root, "artifacts", "lane", "lane-cov-verify", "build", "bin", "verify.exe")
proc = subprocess.run([exe], capture_output=True, text=True, env=env, timeout=120)
print("len", len(value))
print("code", proc.returncode)
print("stderr head:", proc.stderr[:200])
print("stderr tail:", proc.stderr[-300:])
