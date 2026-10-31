import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "playground" / "adv79b"))

ROOT_REM = 28.0
FONT_FOCAL = 3.4
FONT_TITLE = 2.2
FONT_ALG = 1.7
FONT_HEADING = 1.3
FONT_BODY = 1.0
FONT_SMALL = 0.8
FONT_MICRO = 0.75
SPACING_REM = 0.5
PANEL_PADDING_REM = 1.0
HEADER_HEIGHT_REM = 3.5

COLUMN_REM = 26.0
ROW_REM = 3.2
CONTROL_REM = 11.0
CONTROL_INSET_REM = 0.3
ROW_COUNT = 3
MODE_COUNT = 3


class Rect:
    def __init__(self, x, y, w, h):
        self.x = x
        self.y = y
        self.w = w
        self.h = h

    def __repr__(self):
        return f"Rect({self.x:.1f},{self.y:.1f},{self.w:.1f},{self.h:.1f})"


def frame_metrics(window_w, window_h):
    pad = PANEL_PADDING_REM * ROOT_REM
    header = HEADER_HEIGHT_REM * ROOT_REM
    return {
        "pad": pad,
        "header": header,
        "content_x": pad,
        "content_y": pad + header,
        "content_w": window_w - pad * 2.0,
        "content_h": window_h - pad * 2.0 - header,
    }


def settings_layout(window_w, window_h):
    m = frame_metrics(window_w, window_h)
    column_w = min(COLUMN_REM * ROOT_REM, m["content_w"])
    column_half = column_w * 0.5
    row_h = ROW_REM * ROOT_REM
    gap = SPACING_REM * 2.0 * ROOT_REM
    block_h = row_h * ROW_COUNT + gap * (ROW_COUNT - 1)
    column_x = m["content_x"] + (m["content_w"] - column_w) / 2.0
    top_y = m["content_y"] + (m["content_h"] - block_h) / 2.0
    control_w = min(CONTROL_REM * ROOT_REM, column_half)
    inset = CONTROL_INSET_REM * ROOT_REM
    if row_h - inset - inset < ROOT_REM:
        inset = max((row_h - ROOT_REM) * 0.5, 0.0)
    control_h = row_h - inset - inset
    pad = SPACING_REM * ROOT_REM
    body = FONT_BODY * ROOT_REM
    hint_gap = SPACING_REM * 0.5 * ROOT_REM
    small = FONT_SMALL * ROOT_REM
    rows, controls, hints = [], [], []
    for i in range(ROW_COUNT):
        row_y = top_y + (row_h + gap) * i
        rows.append(Rect(column_x, row_y, column_w, row_h))
        controls.append(Rect(column_x + column_w - control_w, row_y + inset, control_w, control_h))
        hints.append(Rect(column_x + pad, row_y + pad + body + hint_gap, column_w - control_w - pad, small))
    return {
        "metrics": m,
        "rows": rows,
        "controls": controls,
        "hints": hints,
        "row_h": row_h,
        "gap": gap,
        "inset": inset,
        "control_h": control_h,
    }


def select_slots(control):
    return [Rect(control.x, control.y + control.h * (i + 1), control.w, control.h) for i in range(MODE_COUNT)]


def overlap_y(a, b):
    top = max(a.y, b.y)
    bottom = min(a.y + a.h, b.y + b.h)
    return max(0.0, bottom - top)


def text_width(text, px, font_path=None):
    from PIL import ImageFont

    if font_path is None:
        font_path = str(REPO / "assets" / "fonts" / "NotoSans-Regular.ttf")
    font = ImageFont.truetype(font_path, int(round(px)))
    return font.getlength(text)
