import json
import os
import shutil
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
LOG = []


def note(line):
    print(line, flush=True)
    LOG.append(line)


class TourApp:
    def __init__(self, appdata, tag):
        env = dict(os.environ)
        env["APPDATA"] = str(appdata)
        self.proc = subprocess.Popen([str(EXE)], cwd=str(REPO), env=env)
        self.tag = tag
        self.hwnd = winlib.hwnd_for_title(TITLE)
        if not self.hwnd:
            self.stop()
            raise RuntimeError("window never appeared")
        winlib.wait_frames(1.0)
        winlib.focus_window(self.hwnd)
        winlib.wait_frames(0.5)
        if winlib.client_size(self.hwnd) != (1280, 800):
            self.stop()
            raise RuntimeError("unexpected client size")

    def key(self, name, settle=0.45):
        winlib.press(name)
        winlib.wait_frames(settle)

    def chars(self, text, settle=0.4):
        winlib.type_chars(text)
        winlib.wait_frames(settle)

    def wheel(self, down=True, settle=0.4):
        import ctypes

        user32 = ctypes.WinDLL("user32")
        user32.mouse_event(0x0800, 0, 0, -120 if down else 120, 0)
        winlib.wait_frames(settle)

    def shot(self, name):
        path = SHOTS / f"{self.tag}_{name}.png"
        winlib.capture(self.hwnd, str(path))
        note(f"[{self.tag}] shot {path.name}")
        return path

    def stop(self):
        if getattr(self, "hwnd", 0) and winlib.user32.IsWindow(self.hwnd):
            winlib.close_window(self.hwnd)
        if not winlib.wait_gone(self.proc.pid):
            self.proc.kill()
            self.proc.wait()
        self.proc.returncode = self.proc.poll()
        note(f"[{self.tag}] exited code {self.proc.returncode}")


def deep_tour(appdata):
    app = TourApp(appdata, "t")
    try:
        app.shot("00_menu")
        app.key("3", 0.8)
        app.shot("01_solve_ready")
        app.key("SPACE", 0.8)
        app.shot("02_solve_memo")
        app.key("SPACE", 0.8)
        app.shot("03_solve_exec")
        app.key("SPACE", 0.8)
        app.shot("04_solve_review")
        app.key("V", 0.8)
        app.shot("05_solve_replay")
        app.key("ESC", 0.6)
        app.key("2", 0.8)
        app.shot("06_memo_intro")
        app.key("SPACE", 0.8)
        winlib.wait_frames(3.0)
        app.shot("07_memo_stream")
        for _ in range(30):
            winlib.press("SPACE")
            winlib.wait_frames(0.12)
        winlib.wait_frames(0.5)
        app.shot("08_memo_late")
        app.key("SPACE", 0.6)
        app.shot("09_memo_rate")
        app.key("ESC", 0.6)
        app.key("1", 0.8)
        app.shot("10_drill_card")
        for _ in range(4):
            winlib.press("SPACE")
            winlib.wait_frames(0.25)
        app.shot("11_drill_reps")
        for _ in range(8):
            winlib.press("SPACE")
            winlib.wait_frames(0.2)
        app.shot("12_drill_rate")
        app.key("ESC", 0.6)
        app.key("4", 0.8)
        app.shot("13_algs")
        app.wheel(down=True)
        app.shot("14_algs_scrolled")
        app.key("ESC", 0.6)
        app.key("5", 0.8)
        app.shot("15_stats")
        app.key("ESC", 0.6)
    finally:
        app.stop()


def smoke_run(appdata, out_dir):
    env = dict(os.environ)
    env["APPDATA"] = str(appdata)
    proc = subprocess.run([str(EXE), "--smoke", str(out_dir)], cwd=str(REPO), env=env,
                          capture_output=True, text=True, timeout=170)
    note(f"[smoke] exit {proc.returncode} stdout tail {proc.stdout[-300:]!r} stderr tail {proc.stderr[-300:]!r}")
    return proc.returncode


def main():
    SHOTS.mkdir(parents=True, exist_ok=True)
    temp = Path(tempfile.mkdtemp(prefix="adv79b-tour-"))
    try:
        deep_tour(temp)
    finally:
        shutil.rmtree(temp, ignore_errors=True)
        note(f"purged temp {temp}")
    (REPO / "playground" / "adv79b" / "tour.log").write_text("\n".join(LOG) + "\n")


if __name__ == "__main__":
    main()
