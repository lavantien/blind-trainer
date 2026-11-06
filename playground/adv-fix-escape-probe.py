import re


def esc(arg):
    body = re.sub(r"(\\+)(?=\")", r"\1\1", arg)
    body = body.replace('"', '\\"')
    return re.sub(r"(\\+)$", r"\1\1", body)


cases = [
    ('--name="x y"', '--name=\\"x y\\"'),
    ("C:\\dir\\", "C:\\dir\\\\"),
    ("a\\\"b", "a\\\\\\\"b"),
    ("plain", "plain"),
    ("", ""),
    ("trailing\\\\\\", "trailing\\\\\\\\\\\\"),
]
for arg, want in cases:
    got = esc(arg)
    print(repr(arg), "->", repr(got), "want", repr(want), "OK" if got == want else "MISMATCH")
