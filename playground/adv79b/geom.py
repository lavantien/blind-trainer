import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from layout import (ROOT_REM, FONT_BODY, FONT_SMALL, MODE_COUNT, frame_metrics, settings_layout,
                    select_slots, overlap_y, text_width, REPO)

FINDINGS = []


def note(line):
    print(line)
    FINDINGS.append(line)


def check_settings(window_w, window_h, tag):
    lay = settings_layout(window_w, window_h)
    m = lay["metrics"]
    note(f"[{tag}] window {window_w}x{window_h} content {m['content_x']:.1f},{m['content_y']:.1f} "
         f"{m['content_w']:.1f}x{m['content_h']:.1f}")
    for i in range(3):
        r, c, h = lay["rows"][i], lay["controls"][i], lay["hints"][i]
        note(f"[{tag}] row{i} {r} control {c} hint {h}")
    slots = select_slots(lay["controls"][1])
    for i, s in enumerate(slots):
        note(f"[{tag}] select slot{i} {s}")
        if s.y + s.h > window_h:
            note(f"[{tag}] FAIL slot{i} bottom {s.y + s.h:.1f} exceeds window {window_h}")
    ov = overlap_y(slots[0], lay["controls"][2])
    note(f"[{tag}] slot0 vs reps-control overlap height {ov:.1f} px")
    if ov > 0:
        band_top = max(slots[0].y, lay["controls"][2].y)
        band_bot = min(slots[0].y + slots[0].h, lay["controls"][2].y + lay["controls"][2].h)
        stepper = lay["control_h"]
        left_stepper = lay["controls"][2].x, lay["controls"][2].x + stepper
        right_stepper = lay["controls"][2].x + lay["controls"][2].w - stepper, lay["controls"][2].x + lay["controls"][2].w
        note(f"[{tag}] click-through band y {band_top:.1f}-{band_bot:.1f} "
             f"left stepper x {left_stepper[0]:.1f}-{left_stepper[1]:.1f} "
             f"right stepper x {right_stepper[0]:.1f}-{right_stepper[1]:.1f}")
    labels = ["memo cadence (ms)", "memo mode", "drill reps"]
    hints = ["digits then enter", "press 1, 2, or 3", "digits then enter"]
    pad = 0.5 * ROOT_REM
    for i in range(3):
        label_box = lay["controls"][i].x - (lay["rows"][i].x + pad)
        lw = text_width(labels[i], FONT_BODY * ROOT_REM)
        hw = text_width(hints[i], FONT_SMALL * ROOT_REM)
        note(f"[{tag}] row{i} label width {lw:.1f} in {label_box:.1f} "
             f"hint width {hw:.1f} in {lay['hints'][i].w:.1f}")
        if lw > label_box:
            note(f"[{tag}] FAIL label overflows into control")
        if hw > lay["hints"][i].w:
            note(f"[{tag}] FAIL hint overflows into control")
    mode_rows = ["1 syllables", "2 images", "3 per-orbit"]
    for i, row in enumerate(mode_rows):
        rw = text_width(row, FONT_BODY * ROOT_REM)
        inner = lay["controls"][1].w - 2 * pad
        note(f"[{tag}] select row '{row}' width {rw:.1f} in {inner:.1f}")
    for value in ["800", "799", "9999", "10000", "6", "7", "9", "100"]:
        vw = text_width(value, FONT_BODY * ROOT_REM)
        inner = lay["controls"][0].w - 2 * lay["control_h"]
        fits = vw <= inner
        note(f"[{tag}] value '{value}' width {vw:.1f} in field inner {inner:.1f} fits={fits}")
        if not fits:
            note(f"[{tag}] FAIL value clipped by steppers")


def check_solve_column(window_w, window_h):
    m = frame_metrics(window_w, window_h)
    gap = 0.5 * ROOT_REM
    net_w = 0.44 * m["content_w"]
    x = m["content_x"] + net_w + gap * 2.0
    y = m["content_y"]
    sizes = {
        "phase": FONT_SMALL * ROOT_REM,
        "focal": 3.4 * ROOT_REM,
        "timer": FONT_SMALL * ROOT_REM,
        "result": 2.2 * ROOT_REM,
        "splits_line": 1.3 * ROOT_REM,
        "broken_line": FONT_SMALL * ROOT_REM,
        "hint": FONT_SMALL * ROOT_REM,
    }
    y += sizes["phase"]
    y += sizes["focal"] + gap
    focal_y = y
    y += sizes["focal"] + gap
    y += sizes["timer"]
    y += sizes["result"] + gap
    y += 1.3 * ROOT_REM
    y += sizes["splits_line"] + gap
    note(f"[solve {window_w}x{window_h}] column x {x:.1f} focal y {focal_y:.1f} "
         f"first splits line y {y:.1f}")
    line = 1.3 * ROOT_REM + 0.0
    lines_that_fit = int((window_h - y) // line)
    note(f"[solve {window_w}x{window_h}] lines remaining below first splits line: {lines_that_fit}")
    avail = window_h - y
    note(f"[solve {window_w}x{window_h}] pixels below first splits line: {avail:.1f}")
    worst = (2.2 * ROOT_REM + gap) + (1.3 * ROOT_REM + gap) * 6 + (FONT_SMALL * ROOT_REM + gap) * 3
    note(f"[solve {window_w}x{window_h}] worst stack below focal needs ~{worst:.1f} px, "
         f"content bottom margin gives {window_h - focal_y:.1f} px")


def main():
    check_settings(1280.0, 800.0, "main")
    check_settings(640.0, 480.0, "probe")
    check_solve_column(1280.0, 800.0)
    body_px = FONT_BODY * ROOT_REM
    note(f"body nominal {body_px:.1f} px, small {FONT_SMALL * ROOT_REM:.1f} px, "
         f"focal {3.4 * ROOT_REM:.1f} px, title {2.2 * ROOT_REM:.1f} px")
    from PIL import ImageFont

    font = ImageFont.truetype(str(REPO / "assets" / "fonts" / "NotoSans-Regular.ttf"), int(body_px))
    ascent, descent = font.getmetrics()
    note(f"body font metrics at {int(body_px)} px: ascent {ascent} descent {descent} "
         f"x-height band approx {font.getlength('x'):.1f} width")
    caps = font.getbbox("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789")
    digits = font.getbbox("0123456789")
    note(f"caps+digits bbox {caps} digits-only bbox {digits}")


if __name__ == "__main__":
    main()
