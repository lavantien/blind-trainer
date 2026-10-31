import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from layout import settings_layout

SHOTS = Path(__file__).resolve().parent / "shots"
LAY = settings_layout(1280.0, 800.0)
ACCENT = (89, 168, 242)
HAIRLINE = (66, 71, 84)


def load(name):
    return Image.open(SHOTS / name).convert("RGB")


def lit_rows(image, box, threshold=110):
    x0, y0, x1, y1 = [int(v) for v in box]
    pixels = image.load()
    bands = []
    run = None
    for y in range(y0, y1):
        lit = any(pixels[x, y][0] > threshold and pixels[x, y][1] > threshold and pixels[x, y][2] > threshold
                  for x in range(x0, x1))
        if lit and run is None:
            run = [y, y]
        elif lit:
            run[1] = y
        elif run is not None:
            bands.append(tuple(run))
            run = None
    if run is not None:
        bands.append(tuple(run))
    return bands


def band_width(image, y0, y1, x0, x1, threshold=110):
    pixels = image.load()
    cols = [x for x in range(int(x0), int(x1))
            if any(pixels[x, y][0] > threshold and pixels[x, y][1] > threshold and pixels[x, y][2] > threshold
                   for y in range(int(y0), int(y1)))]
    return (cols[0], cols[-1], cols[-1] - cols[0] + 1) if cols else None


def value_span(name, row):
    image = load(name)
    c = LAY["controls"][row]
    inner_x0 = c.x + LAY["control_h"] + 2
    inner_x1 = c.x + c.w - LAY["control_h"] - 2
    y0, y1 = c.y + 4, c.y + c.h - 4
    return band_width(image, y0, y1, inner_x0, inner_x1)


def border_color(name, row):
    image = load(name)
    c = LAY["controls"][row]
    pixels = image.load()
    x = int(c.x + c.w / 2)
    y_top = int(c.y) - 1
    y_bot = int(c.y + c.h)
    top = pixels[x, y_top]
    bot = pixels[x, y_bot]
    return top, bot


def near(color, target, tol=40):
    return all(abs(color[i] - target[i]) <= tol for i in range(3))


def report(name):
    image = load(name)
    print(f"== {name}")
    label_box = (LAY["rows"][0].x, LAY["rows"][0].y, LAY["controls"][0].x, LAY["rows"][2].y + LAY["rows"][2].h)
    print(f"  left column bands: {lit_rows(image, label_box)}")
    for row in range(3):
        span = value_span(name, row)
        print(f"  row{row} value span {span}")
        print(f"  row{row} border {border_color(name, row)}")


def dropdown_rows(name):
    image = load(name)
    c1 = LAY["controls"][1]
    box = (c1.x, c1.y + c1.h, c1.x + c1.w, c1.y + c1.h * 4)
    return lit_rows(image, box)


def diff_region(name_a, name_b, box):
    a = load(name_a)
    b = load(name_b)
    x0, y0, x1, y1 = [int(v) for v in box]
    pa, pb = a.load(), b.load()
    changed = 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            if pa[x, y] != pb[x, y]:
                changed += 1
    return changed


def main():
    report("s_01_initial.png")
    for name in ["s_02_minus.png", "s_04_typing.png", "s_05_commit_9999.png", "s_06_clamped_10000.png",
                 "s_07_reps_100.png", "s_08_reps_plus_at_max.png", "s_11_enter_commits.png",
                 "s_17_clickthrough.png", "s_18_digits_land_in_hidden_field.png", "r_01_settings_after_reload.png"]:
        image = load(name)
        spans = [value_span(name, row) for row in range(3)]
        borders = [border_color(name, row) for row in range(3)]
        print(f"== {name} spans {spans} borders {borders}")
    print("dropdown bands s_09:", dropdown_rows("s_09_mode_key_opens.png"))
    print("dropdown bands s_16:", dropdown_rows("s_16_select_open.png"))
    c1 = LAY["controls"][1]
    dd_box = (c1.x, c1.y + c1.h, c1.x + c1.w, min(800.0, c1.y + c1.h * 4))
    for pair in [("s_15_reenter_roundtrip.png", "s_16_select_open.png"),
                 ("s_16_select_open.png", "s_17_clickthrough.png"),
                 ("s_17_clickthrough.png", "s_18_digits_land_in_hidden_field.png")]:
        print(f"diff {pair[0]} -> {pair[1]} in dropdown region: {diff_region(pair[0], pair[1], dd_box)} px")


if __name__ == "__main__":
    main()
