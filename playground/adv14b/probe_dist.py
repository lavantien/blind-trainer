import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "scripts"))
import dist
import memguard

with tempfile.TemporaryDirectory() as tmp:
    repo = Path(tmp)
    exe = repo / "build" / "bin" / "blind-trainer.exe"
    exe.parent.mkdir(parents=True)
    exe.write_text("exe")
    release = {"staging_dir": "s", "exe": "blind-trainer.exe", "config_file": "config/config.json", "fonts_dir": "assets/fonts"}
    plan = dist.staging_plan(repo, repo / "out", release)
    print(f"probe5 dist plan with NO config and NO fonts: len={len(plan)} entries={[(str(s), str(d)) for s, d in plan]}")
    print("probe5b missing release keys:")
    try:
        dist.staging_plan(repo, repo / "out", {})
    except KeyError as exc:
        print(f"probe5b KeyError {exc} (raw traceback path, no slog fail)")

zombie = subprocess.Popen(
    [sys.executable, "-c", "import sys; sys.exit(259)"],
    creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
)
zombie.wait()
gone = memguard.wait_process_gone(zombie.pid, 0.5)
print(f"probe6 wait_process_gone on naturally-exited-259 pid (handle held): {gone} (False = kill verification lies)")
del zombie

print(f"probe7 sleeper exists: {(REPO / 'build' / 'bin' / 'sleeper.exe').is_file()}")
