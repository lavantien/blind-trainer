import os
import random
import sys

ROOT = os.path.abspath(sys.argv[1])
REF = os.path.join(ROOT, "ref_ground")
OUT = os.path.join(ROOT, "playground", "attacker-a", "gen")
sys.path.insert(0, REF)
import pysim

FACES = pysim.FACES
AXIS_OF = pysim.AXIS_OF
SIGN_OF = pysim.SIGN_OF
grid = pysim.grid
rot = pysim.rot
FACE_ORDER = ["U", "L", "F", "R", "B", "D"]

HOME = {}
CELL_TO_ID = {}
NAME_TO_ID = {}
PIECE_OF = {}
for line in open(os.path.join(REF, "position_map.tsv"), encoding="utf-8").read().splitlines()[1:]:
    orbit, sticker, letter, name, face, row, col, piece = line.split("\t")
    sid = int(sticker)
    frc = (int(face), int(row), int(col))
    HOME[sid] = frc
    CELL_TO_ID[frc] = sid
    NAME_TO_ID[name] = sid
    PIECE_OF[sid] = (orbit, int(piece))
for f in range(6):
    CELL_TO_ID[(f, 1, 1)] = 48 + f

FACE_LETTER = {ch: i for i, ch in enumerate(FACE_ORDER)}
SLICE_FOLLOWS = {"M": "L", "E": "D", "S": "F"}
ROT_FOLLOWS = {"x": "R", "y": "U", "z": "F"}

FACE_CHARS = "UDLRFB"


def parse_token(token):
    inner = token.startswith("2")
    body = token[1:] if inner else token
    if body.endswith("2'") or body.endswith("2"):
        base, quarter = body[:-2] if body.endswith("2'") else body[:-1], 2
    elif body.endswith("'"):
        base, quarter = body[:-1], 3
    else:
        base, quarter = body, 1
    return base, quarter, inner


def token_spec(token):
    base, quarter, inner = parse_token(token)
    if base in SLICE_FOLLOWS:
        return SLICE_FOLLOWS[base], "inner", quarter, token
    if base in ROT_FOLLOWS:
        return ROT_FOLLOWS[base], "whole", quarter, token
    if base in FACE_CHARS:
        if inner:
            return base, "inner", quarter, token
        return base, "face", quarter, token
    if len(base) == 1 and base.upper() in FACE_CHARS:
        if inner:
            raise ValueError(token)
        return base.upper(), "wide", quarter, token
    raise ValueError(token)


def layer_match(kind, d):
    if kind == "face":
        return d == 1
    if kind == "inner":
        return d == 0
    if kind == "wide":
        return d in (0, 1)
    return True


def simulate(tokens):
    stickers = []
    for sid in range(54):
        f, r, c = HOME[sid] if sid < 48 else (sid - 48, 1, 1)
        stickers.append([list(grid(FACE_ORDER[f], r, c)), list(FACES[FACE_ORDER[f]]), sid])
    for token in tokens:
        face, kind, quarter, _ = token_spec(token)
        n = FACES[face]
        axis = AXIS_OF[face]
        applied = quarter if SIGN_OF[face] < 0 else (4 - quarter) % 4
        for s in stickers:
            d = sum(a * b for a, b in zip(s[0], n))
            if layer_match(kind, d):
                s[0] = list(rot(tuple(s[0]), axis, applied))
                s[1] = list(rot(tuple(s[1]), axis, applied))
    return stickers


def occ_of(tokens):
    stickers = simulate(tokens)
    occ = [0] * 54
    face_of = {v: k for k, v in FACES.items()}
    for pos, normal, sid in stickers:
        f = FACE_LETTER[face_of[tuple(normal)]]
        rc = pysim.cell_of(FACE_ORDER[f], tuple(pos))
        occ[CELL_TO_ID[(f, rc[0], rc[1])]] = sid
    return occ


def table_of(tokens):
    occ = occ_of(tokens)
    table = [0] * 54
    for pos, sid in enumerate(occ):
        table[sid] = pos
    return table


def pysim_occ(alg):
    stickers = pysim.simulate(alg)
    occ = [0] * 54
    face_of = {v: k for k, v in FACES.items()}
    for pos, normal, label in stickers:
        f = FACE_ORDER.index(label[0])
        sid = CELL_TO_ID[(f, int(label[1]), int(label[2]))]
        df = FACE_LETTER[face_of[normal]]
        rc = pysim.cell_of(FACE_ORDER[df], tuple(pos))
        occ[CELL_TO_ID[(df, rc[0], rc[1])]] = sid
    return occ


def pieces_of(names):
    return {PIECE_OF[NAME_TO_ID[n]][1] for n in names}


def piece_members(pid, orbit_letter):
    kind = 0 if orbit_letter == "corner" else 24
    lo, hi = (0, 24) if orbit_letter == "corner" else (24, 48)
    return [s for s in range(lo, hi) if PIECE_OF[s][1] == pid]


def others_home(occ, exclude_pieces, orbit_letter):
    lo, hi = (0, 24) if orbit_letter == "corner" else (24, 48)
    for s in range(lo, hi):
        if PIECE_OF[s][1] not in exclude_pieces and occ[s] != s:
            return False
    return True


def centers_fixed(occ):
    return all(occ[48 + f] == 48 + f for f in range(6))


def check_structure(sheet, position, note, occ):
    if sheet in ("edge", "corner"):
        a, b, c = position.split("-")
        ia, ib, ic = NAME_TO_ID[a], NAME_TO_ID[b], NAME_TO_ID[c]
        orbit = PIECE_OF[ia][0]
        if not (occ[ib] == ia and occ[ic] == ib and occ[ia] == ic):
            return "cycle direction"
        if not others_home(occ, pieces_of([a, b, c]), orbit):
            return "foreign piece moved"
        if not centers_fixed(occ):
            return "centers moved"
        return None
    if sheet == "flips":
        a, b = [x.strip() for x in position.split("&")]
        for n in (a, b):
            ia = NAME_TO_ID[n]
            members = piece_members(PIECE_OF[ia][1], PIECE_OF[ia][0])
            other = [m for m in members if m != ia][0]
            if occ[ia] != other or occ[other] != ia:
                return f"flip {n}"
        if not others_home(occ, pieces_of([a, b]), PIECE_OF[NAME_TO_ID[a]][0]):
            return "foreign piece moved"
        if not centers_fixed(occ):
            return "centers moved"
        return None
    if sheet == "twists":
        a, b = [x.strip() for x in position.split("&")]
        for n in (a, b):
            ia = NAME_TO_ID[n]
            members = piece_members(PIECE_OF[ia][1], PIECE_OF[ia][0])
            hit = {occ[m] for m in members}
            if hit != set(members) or any(occ[m] == m for m in members):
                return f"twist {n} not in place"
        if not others_home(occ, pieces_of([a, b]), PIECE_OF[NAME_TO_ID[a]][0]):
            return "foreign piece moved"
        if not centers_fixed(occ):
            return "centers moved"
        return None
    if sheet == "parity":
        epair, cpair = [x.strip() for x in position.split(",")]
        e1, e2 = epair.split("-")
        c1, c2 = cpair.split("-")
        for x, y in ((e1, e2), (c1, c2)):
            ix, iy = NAME_TO_ID[x], NAME_TO_ID[y]
            px, py = PIECE_OF[ix][1], PIECE_OF[iy][1]
            orbit = PIECE_OF[ix][0]
            mx, my = piece_members(px, orbit), piece_members(py, orbit)
            if {occ[m] for m in mx} != set(my) or {occ[m] for m in my} != set(mx):
                return f"swap {x}-{y}"
        if not others_home(occ, pieces_of([e1, e2]), PIECE_OF[NAME_TO_ID[e1]][0]):
            return "foreign edge moved"
        if not others_home(occ, pieces_of([c1, c2]), PIECE_OF[NAME_TO_ID[c1]][0]):
            return "foreign corner moved"
        if not centers_fixed(occ):
            return "centers moved"
        return None
    if sheet == "ltct":
        epair, cpart = [x.strip() for x in position.split(",")]
        e1, e2 = epair.split("-")
        c1, c2b = cpart.split("-")
        c2, c3 = c2b[:-1].split("[") if c2b.endswith("]") and "[" in c2b else (c2b, None)
        named = [e1, e2, c1, c2]
        if c3:
            named.append(c3)
        ix, iy = NAME_TO_ID[e1], NAME_TO_ID[e2]
        mx = piece_members(PIECE_OF[ix][1], "edge")
        my = piece_members(PIECE_OF[iy][1], "edge")
        if {occ[m] for m in mx} != set(my):
            return "edge swap"
        cx, cy = NAME_TO_ID[c1], NAME_TO_ID[c2]
        mcx = piece_members(PIECE_OF[cx][1], "corner")
        mcy = piece_members(PIECE_OF[cy][1], "corner")
        if {occ[m] for m in mcx} != set(mcy):
            return "corner swap"
        if c3:
            ic3 = NAME_TO_ID[c3]
            m3 = piece_members(PIECE_OF[ic3][1], "corner")
            if {occ[m] for m in m3} != set(m3) or all(occ[m] == m for m in m3):
                return "bracket twist"
        excl_e = pieces_of([e1, e2])
        excl_c = pieces_of([c1, c2] + ([c3] if c3 else []))
        for s in range(24):
            if PIECE_OF[s][1] not in excl_c and occ[s] != s:
                return "foreign corner moved"
        for s in range(24, 48):
            if PIECE_OF[s][1] not in excl_e and occ[s] != s:
                return "foreign edge moved"
        if not centers_fixed(occ):
            return "centers moved"
        return None
    return "unknown sheet"


def main():
    os.makedirs(OUT, exist_ok=True)
    rng = random.Random(0xC0FFEE)

    face_only_algs = []
    rows = [l.split("\t") for l in open(os.path.join(REF, "rows.tsv"), encoding="utf-8").read().splitlines()]
    for r in rows:
        toks = r[4].split()
        if all(t[0] in FACE_CHARS for t in toks):
            face_only_algs.append(" ".join(toks))
    sample = rng.sample(face_only_algs, min(120, len(face_only_algs)))
    for _ in range(300):
        n = rng.randint(1, 14)
        sample.append(" ".join(rng.choice(FACE_CHARS) + rng.choice(["", "'", "2"]) for _ in range(n)))
    mism = 0
    for alg in sample:
        if occ_of(alg.split()) != pysim_occ(alg):
            mism += 1
    print("face-path agreement with pysim.simulate:", len(sample) - mism, "/", len(sample))

    simcheck = [l.split("\t") for l in open(os.path.join(REF, "simcheck.tsv"), encoding="utf-8", newline="").read().replace("\r\n", "\n").splitlines()]
    sc = {}
    for solver, sheet, rowid, position, moves, qtm, match, note in simcheck[1:]:
        sc[(solver, sheet, rowid)] = (position, moves, qtm, match, note)

    qtm_plain = 0
    qtm_rot0 = 0
    moves_plain = 0
    moves_rot0 = 0
    struct_fail = []
    with open(os.path.join(OUT, "expected_algs.tsv"), "w", encoding="utf-8", newline="\n") as out:
        out.write("solver\tsheet\trowid\tposition\talg\tmoves\tqtm\tocc\n")
        for solver, sheet, rowid, position, alg in rows:
            toks = alg.split()
            occ = occ_of(toks)
            key = (solver, sheet, rowid)
            sc_position, moves, qtm, match, note = sc[key]
            q_calc = sum(2 if t.endswith("2") else 1 for t in toks)
            q_rot0 = sum(0 if t[0] in "xyz" else (2 if t.endswith("2") else 1) for t in toks)
            if q_calc == int(qtm):
                qtm_plain += 1
            if q_rot0 == int(qtm):
                qtm_rot0 += 1
            if len(toks) == int(moves):
                moves_plain += 1
            if sum(1 for t in toks if t[0] not in "xyz") == int(moves):
                moves_rot0 += 1
            problem = check_structure(sheet, sc_position, note, occ)
            if problem:
                struct_fail.append((solver, sheet, rowid, sc_position, alg, problem))
            out.write("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n" % (
                solver, sheet, rowid, sc_position, alg, moves, qtm,
                ",".join(str(x) for x in occ)))
    print("qtm rule: plain", qtm_plain, "rot0", qtm_rot0, "of", len(rows))
    print("moves rule: plain", moves_plain, "rot0", moves_rot0, "of", len(rows))
    print("structure failures:", len(struct_fail))
    for f in struct_fail[:20]:
        print("  ", f)

    tokens = []
    for base in FACE_CHARS + FACE_CHARS.lower() + "MES" + "xyz":
        for suffix in ("", "'", "2"):
            tokens.append(base + suffix)
    for base in FACE_CHARS:
        for suffix in ("", "'", "2"):
            tokens.append("2" + base + suffix)
    with open(os.path.join(OUT, "expected_generators.tsv"), "w", encoding="utf-8", newline="\n") as out:
        out.write("token\ttable\n")
        for token in tokens:
            out.write("%s\t%s\n" % (token, ",".join(str(x) for x in table_of([token]))))
    print("generator tokens:", len(tokens))

    pins = [
        ("z=F S B'", ["F", "S", "B'"], ["z"]),
        ("z2=F2 S2 B2", ["F2", "S2", "B2"], ["z2"]),
        ("z'=F' S' B", ["F'", "S'", "B"], ["z'"]),
        ("x=R M' L'", ["R", "M'", "L'"], ["x"]),
        ("y=U E' D'", ["U", "E'", "D'"], ["y"]),
        ("r=R 2R", ["R", "2R"], ["r"]),
        ("u=U 2U", ["U", "2U"], ["u"]),
        ("M=2L", ["2L"], ["M"]),
        ("E=2D", ["2D"], ["E"]),
        ("S=2F", ["2F"], ["S"]),
    ]
    for name, lhs, rhs in pins:
        same = table_of(lhs) == table_of(rhs)
        print("pin", name, same)

    with open(os.path.join(OUT, "expected_random.tsv"), "w", encoding="utf-8", newline="\n") as out:
        out.write("idx\talg\tocc\n")
        for i in range(2000):
            n = rng.randint(1, 12)
            alg = " ".join(rng.choice(tokens) for _ in range(n))
            out.write("%d\t%s\t%s\n" % (i, alg, ",".join(str(x) for x in occ_of(alg.split()))))
    print("random algs: 2000")

    with open(os.path.join(OUT, "expected_long.tsv"), "w", encoding="utf-8", newline="\n") as out:
        out.write("idx\talg\tocc\n")
        for i in range(200):
            n = rng.randint(50, 200)
            alg = " ".join(rng.choice(tokens) for _ in range(n))
            out.write("%d\t%s\t%s\n" % (i, alg, ",".join(str(x) for x in occ_of(alg.split()))))
    print("long algs: 200")


if __name__ == "__main__":
    main()
