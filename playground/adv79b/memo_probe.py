import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
EXE = REPO / "build" / "bin" / "bt-app.exe"
SHOTS = REPO / "playground" / "adv79b" / "shots"
sys.path.insert(0, str(Path(__file__).resolve().parent))
import winlib

TITLE = "blind trainer"


def main():
    SHOTS.mkdir(parents=True, exist_ok=True)
    temp = Path(tempfile.mkdtemp(prefix="adv79b-memo-"))
    env = dict(os.environ)
    env["APPDATA"] = str(temp)
    out = open(SHOTS.parent / "memo_probe.out", "w")
    proc = subprocess.Popen([str(EXE)], cwd=str(REPO), env=env, stdout=out, stderr=out)
    hwnd = winlib.hwnd_for_title(TITLE)
    if not hwnd:
        print("no window", flush=True)
        proc.kill()
        return
    winlib.wait_frames(1.0)
    winlib.focus_window(hwnd)
    winlib.wait_frames(0.5)
    winlib.press("2")
    winlib.wait_frames(0.8)
    winlib.capture(hwnd, str(SHOTS / "p_00_memo_intro.png"))
    winlib.press("SPACE")
    winlib.wait_frames(2.5)
    winlib.capture(hwnd, str(SHOTS / "p_01_memo_stream.png"))
    alive = True
    for i in range(40):
        winlib.press("SPACE")
        winlib.wait_frames(0.12)
        if not winlib.user32.IsWindow(hwnd):
            alive = False
            print(f"window died after space {i}", flush=True)
            break
    if alive:
        winlib.wait_frames(0.5)
        winlib.capture(hwnd, str(SHOTS / "p_02_memo_rate.png"))
        winlib.press("3")
        winlib.wait_frames(0.4)
        winlib.capture(hwnd, str(SHOTS / "p_03_memo_rated.png"))
    code = proc.wait(timeout=10) if proc.poll() is not None else None
    if code is None:
        winlib.close_window(hwnd)
        try:
            code = proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            proc.kill()
            code = "killed"
    out.close()
    print(f"proc exit {code}", flush=True)
    import shutil

    shutil.rmtree(temp, ignore_errors=True)


if __name__ == "__main__":
    main()
