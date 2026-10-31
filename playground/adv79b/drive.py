import json
import os
import shutil
import sqlite3
import subprocess
import sys
import tempfile
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
EXE = REPO / "build" / "bin" / "bt-app.exe"
SHOTS = REPO / "playground" / "adv79b" / "shots"
TITLE = "blind trainer"
sys.path.insert(0, str(Path(__file__).resolve().parent))
from layout import settings_layout
import winlib

LOG = []


def note(line):
    print(line, flush=True)
    LOG.append(line)


def read_settings_db(appdata):
    db = Path(appdata) / "blind-trainer" / "blind-trainer.db"
    if not db.exists():
        return {"missing": str(db)}
    conn = sqlite3.connect(f"file:{db}?mode=ro", uri=True)
    try:
        rows = conn.execute("SELECT key, value FROM settings").fetchall()
    finally:
        conn.close()
    return {key: json.loads(value) for key, value in rows}


class App:
    def __init__(self, appdata, tag):
        self.appdata = appdata
        self.tag = tag
        env = dict(os.environ)
        env["APPDATA"] = str(appdata)
        self.proc = subprocess.Popen([str(EXE)], cwd=str(REPO), env=env)
        self.hwnd = winlib.hwnd_for_title(TITLE)
        if not self.hwnd:
            self.stop()
            raise RuntimeError("app window never appeared")
        winlib.wait_frames(1.0)
        winlib.focus_window(self.hwnd)
        winlib.wait_frames(0.5)
        size = winlib.client_size(self.hwnd)
        note(f"[{tag}] window {size} pid {self.proc.pid}")
        if size != (1280, 800):
            self.stop()
            raise RuntimeError(f"unexpected client size {size}")

    def to(self, x, y):
        origin = winlib.client_origin(self.hwnd)
        return origin[0] + x, origin[1] + y

    def key(self, name, settle=0.35):
        winlib.press(name)
        winlib.wait_frames(settle)

    def chars(self, text, settle=0.35):
        winlib.type_chars(text)
        winlib.wait_frames(settle)

    def click(self, x, y, settle=0.4):
        sx, sy = self.to(x, y)
        winlib.click(sx, sy)
        winlib.wait_frames(settle)

    def shot(self, name):
        path = SHOTS / f"{self.tag}_{name}.png"
        winlib.capture(self.hwnd, str(path))
        note(f"[{self.tag}] shot {path.name}")
        return path

    def stop(self):
        if getattr(self, "hwnd", 0):
            winlib.close_window(self.hwnd)
        if not winlib.wait_gone(self.proc.pid):
            self.proc.kill()
            self.proc.wait()
        note(f"[{self.tag}] exited code {self.proc.returncode}")


def center(rect):
    return rect.x + rect.w / 2.0, rect.y + rect.h / 2.0


def settings_scenario(appdata):
    lay = settings_layout(1280.0, 800.0)
    c0, c1, c2 = lay["controls"]
    ch = lay["control_h"]
    app = App(appdata, "s")
    try:
        app.key("6", 0.8)
        app.shot("01_initial")
        app.click(c0.x + ch / 2.0, c0.y + ch / 2.0)
        app.shot("02_minus")
        app.click(c0.x + c0.w - ch / 2.0, c0.y + ch / 2.0)
        app.shot("03_plus_back")
        app.click(*center(c0))
        app.chars("9999")
        app.shot("04_typing")
        app.key("ENTER")
        app.shot("05_commit_9999")
        app.click(*center(c0))
        app.chars("999999")
        app.key("ENTER")
        app.shot("06_clamped_10000")
        app.click(*center(c2))
        app.chars("100")
        app.key("ENTER")
        app.shot("07_reps_100")
        app.click(c2.x + c2.w - ch / 2.0, c2.y + ch / 2.0)
        app.shot("08_reps_plus_at_max")
        app.key("1", 0.5)
        app.shot("09_mode_key_opens")
        app.key("DOWN", 0.3)
        app.key("DOWN", 0.3)
        app.shot("10_arrows_to_per_orbit")
        app.key("ENTER")
        app.shot("11_enter_commits")
        app.click(*center(c0))
        app.shot("12_field_focused")
        app.key("ESC")
        app.shot("13_esc_unfocuses")
        app.key("ESC", 0.6)
        app.shot("14_esc_menu")
        app.key("6", 0.8)
        app.shot("15_reenter_roundtrip")
        app.key("3", 0.4)
        app.key("ESC", 0.3)
        app.click(*center(c1))
        app.shot("16_select_open")
        through_x = c1.x + c1.w * 0.55
        through_y = c2.y + 14.0
        app.click(through_x, through_y)
        app.shot("17_clickthrough")
        app.chars("9")
        app.shot("18_digits_land_in_hidden_field")
        app.key("ESC")
        app.key("ESC", 0.6)
        app.shot("19_menu_after")
    finally:
        app.stop()
    persisted = read_settings_db(appdata)
    note(f"[s] persisted {json.dumps(persisted, sort_keys=True)}")
    return persisted


def reload_scenario(appdata):
    lay = settings_layout(1280.0, 800.0)
    app = App(appdata, "r")
    try:
        app.key("6", 0.8)
        app.shot("01_settings_after_reload")
    finally:
        app.stop()
    return read_settings_db(appdata)


def main():
    SHOTS.mkdir(parents=True, exist_ok=True)
    temp = Path(tempfile.mkdtemp(prefix="adv79b-"))
    try:
        persisted = settings_scenario(temp)
        reloaded = reload_scenario(temp)
        note(f"[r] reloaded {json.dumps(reloaded, sort_keys=True)}")
        same = persisted == reloaded
        note(f"[r] round trip identical: {same}")
    finally:
        shutil.rmtree(temp, ignore_errors=True)
        note(f"purged temp {temp}")
    (REPO / "playground" / "adv79b" / "drive.log").write_text("\n".join(LOG) + "\n")


if __name__ == "__main__":
    main()
