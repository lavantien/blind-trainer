SIGNS = [
    (1, 1, 1),
    (1, 1, -1),
    (-1, 1, -1),
    (-1, 1, 1),
    (-1, -1, 1),
    (1, -1, 1),
    (1, -1, -1),
    (-1, -1, -1),
]


def vertex(face, local):
    p = [0, 0, 0]
    p[local] = SIGNS[face][local]
    return tuple(p)


def rot(signs, point):
    framed = [signs[0] * point[0], signs[1] * point[1], signs[2] * point[2]]
    cycled = (framed[1], framed[2], framed[0])
    return (signs[0] * cycled[0], signs[1] * cycled[1], signs[2] * cycled[2])


def dot_clean(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross_clean(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def trunc_div(x, y):
    q = abs(x) // abs(y)
    return q if (x >= 0) == (y >= 0) else -q


def quarter(val):
    return 1 if val < 0 else 2


cases = []
for f in range(8):
    s = SIGNS[f]
    anchor = vertex(f, 0)
    rotated = rot(s, anchor)
    cases.append((s, anchor, rotated))

clean_q = []
for s, anchor, rotated in cases:
    clean_q.append(quarter(dot_clean(cross_clean(anchor, rotated), s)))
print("clean quarters:", clean_q)

DOT_MUTANTS = {
    "17 *->+": lambda a, b: a[0] + b[0] + a[1] * b[1] + a[2] * b[2],
    "17 *->/": lambda a, b: trunc_div(a[0], b[0]) + a[1] * b[1] + a[2] * b[2],
    "31 +->-": lambda a, b: a[0] * b[0] - a[1] * b[1] + a[2] * b[2],
    "31 +->*": lambda a, b: a[0] * b[0] * a[1] * b[1] + a[2] * b[2],
    "45 *->+": lambda a, b: a[0] * b[0] + a[1] + b[1] + a[2] * b[2],
    "45 *->/": lambda a, b: a[0] * b[0] + trunc_div(a[1], b[1]) + a[2] * b[2],
    "59 +->-": lambda a, b: a[0] * b[0] + a[1] * b[1] - a[2] * b[2],
    "59 +->*": lambda a, b: a[0] * b[0] + a[1] * b[1] * a[2] * b[2],
    "73 *->+": lambda a, b: a[0] * b[0] + a[1] * b[1] + a[2] + b[2],
    "73 *->/": lambda a, b: a[0] * b[0] + a[1] * b[1] + trunc_div(a[2], b[2]),
}

CROSS_MUTANTS = {
    "79:14 *->+": lambda a, b: (a[1] + b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]),
    "79:14 *->/": lambda a, b: (trunc_div(a[1], b[2]) - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]),
    "79:21 -->+": lambda a, b: (a[1] * b[2] + a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]),
    "79:28 *->+": lambda a, b: (a[1] * b[2] - a[2] + b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]),
    "80:14 *->+": lambda a, b: (a[1] * b[2] - a[2] * b[1], a[2] + b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]),
    "80:21 -->+": lambda a, b: (a[1] * b[2] - a[2] * b[1], a[2] * b[0] + a[0] * b[2], a[0] * b[1] - a[1] * b[0]),
    "80:21 -->/": lambda a, b: (a[1] * b[2] - a[2] * b[1], trunc_div(a[2] * b[0], a[0] * b[2]), a[0] * b[1] - a[1] * b[0]),
    "80:28 *->+": lambda a, b: (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] + b[2], a[0] * b[1] - a[1] * b[0]),
    "80:28 *->/": lambda a, b: (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - trunc_div(a[0], b[2]), a[0] * b[1] - a[1] * b[0]),
    "81:14 *->+": lambda a, b: (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] + b[1] - a[1] * b[0]),
    "81:21 -->+": lambda a, b: (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] + a[1] * b[0]),
    "81:28 *->+": lambda a, b: (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] + b[0]),
}


def run(cross_fn, dot_fn, label):
    qs = []
    for s, anchor, rotated in cases:
        try:
            val = dot_fn(cross_fn(anchor, rotated), s)
        except ZeroDivisionError:
            qs.append("div0")
            continue
        qs.append(quarter(val))
    same = qs == clean_q
    print(f"{label}: {'EQUIVALENT' if same else 'DIFFERS'} {qs}")


for label, fn in DOT_MUTANTS.items():
    run(cross_clean, fn, f"dot {label}")
for label, fn in CROSS_MUTANTS.items():
    run(fn, dot_clean, f"cross {label}")
