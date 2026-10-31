import math

CHAR_WIDTH = 6.6
PADDING = 18
GREEN = "#2ea043"
YELLOW = "#bf8700"
RED = "#cf222e"
GREY = "#57606a"
HEIGHT = 20
FONT = "Verdana,Geneva,DejaVu Sans,sans-serif"


def badge_svg(label, value, percent):
    color = GREEN if percent >= 80 else YELLOW if percent >= 60 else RED
    label_w = math.ceil(len(label) * CHAR_WIDTH + PADDING)
    value_w = math.ceil(len(value) * CHAR_WIDTH + PADDING)
    width = label_w + value_w
    label_mid = round(label_w / 2)
    value_mid = label_w + round(value_w / 2)
    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{HEIGHT}" role="img" aria-label="{label}: {value}">\n'
        f'  <title>{label}: {value}</title>\n'
        f'  <clipPath id="round"><rect width="{width}" height="{HEIGHT}" rx="3"/></clipPath>\n'
        f'  <g clip-path="url(#round)">\n'
        f'    <rect width="{label_w}" height="{HEIGHT}" fill="{GREY}"/>\n'
        f'    <rect x="{label_w}" width="{value_w}" height="{HEIGHT}" fill="{color}"/>\n'
        f'  </g>\n'
        f'  <text x="{label_mid}" y="14" fill="#ffffff" font-family="{FONT}" font-size="11" text-anchor="middle">{label}</text>\n'
        f'  <text x="{value_mid}" y="14" fill="#ffffff" font-family="{FONT}" font-size="11" text-anchor="middle">{value}</text>\n'
        f"</svg>"
    )
