import base64
import hashlib
import os
import random
import sys

REF = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "ref_ground"))
OUT = os.path.join(os.path.dirname(__file__), "out")
os.makedirs(OUT, exist_ok=True)
sys.path.insert(0, REF)
import pysim

FACE_LIST = list(pysim.FACES)
FACE_INDEX = {f: i for i, f in enumerate(FACE_LIST)}
CORNER_POS = {(0, 0): 0, (0, 2): 1, (2, 2): 2, (2, 0): 3}
EDGE_POS = {(0, 1): 0, (1, 2): 1, (2, 1): 2, (1, 0): 3}
CELL_TO_ID = {}
for f in FACE_LIST:
    fi = FACE_INDEX[f]
    for r in range(3):
        for c in range(3):
            if (r, c) == (1, 1):
                CELL_TO_ID[(f, r, c)] = 48 + fi
            elif r % 2 == 0 and c % 2 == 0:
                CELL_TO_ID[(f, r, c)] = fi * 4 + CORNER_POS[(r, c)]
            else:
                CELL_TO_ID[(f, r, c)] = 24 + fi * 4 + EDGE_POS[(r, c)]
NORMAL_TO_FACE = {v: k for k, v in pysim.FACES.items()}


def occ_from_stickers(stickers):
    occ = [None] * 54
    for pos, n, label in stickers:
        dstface = NORMAL_TO_FACE[n]
        rc = pysim.cell_of(dstface, pos)
        dst_id = CELL_TO_ID[(dstface, rc[0], rc[1])]
        src_id = CELL_TO_ID[(label[0], int(label[1]), int(label[2]))]
        occ[dst_id] = src_id
    if any(v is None for v in occ):
        raise ValueError("non bijective occ")
    return occ


def parse_token(tok):
    base = tok[0]
    suffix = tok[1:]
    if suffix == "":
        quarter = 1
    elif suffix == "2":
        quarter = 2
    elif suffix == "'":
        quarter = 3
    else:
        raise ValueError(f"bad suffix {tok}")
    if base in "UDLRFB":
        ref, layers = base, {1}
    elif base in "udlrfb":
        ref, layers = base.upper(), {0, 1}
    elif base == "M":
        ref, layers = "L", {0}
    elif base == "E":
        ref, layers = "D", {0}
    elif base == "S":
        ref, layers = "F", {0}
    elif base == "x":
        ref, layers = "R", {-1, 0, 1}
    elif base == "y":
        ref, layers = "U", {-1, 0, 1}
    elif base == "z":
        ref, layers = "F", {-1, 0, 1}
    else:
        raise ValueError(f"bad base {tok}")
    return ref, layers, quarter


def simulate_ext(alg):
    stickers = []
    for f, n in pysim.FACES.items():
        for r in range(3):
            for c in range(3):
                stickers.append([pysim.grid(f, r, c), n, f + str(r) + str(c)])
    for tok in alg.split():
        ref, layers, quarter = parse_token(tok)
        n = pysim.FACES[ref]
        axis = pysim.AXIS_OF[ref]
        sign = pysim.SIGN_OF[ref]
        applied = quarter if sign < 0 else (4 - quarter) % 4
        for s in stickers:
            if sum(a * b for a, b in zip(s[0], n)) in layers:
                s[0] = pysim.rot(s[0], axis, applied)
                s[1] = pysim.rot(s[1], axis, applied)
    return stickers


def occ_of(alg):
    return occ_from_stickers(simulate_ext(alg))


def face_turn_only(alg):
    return all(t[0] in "UDLRFB" and t[1:] in ("", "2", "'") for t in alg.split())


def cross_check_face_turns():
    sample = [
        "U", "U'", "U2", "R", "F2", "B'", "L2 D2", "R U R' U'", "F2 D2 L2",
        "U U'", "D D2 D'", "R2 F2 R2 B2",
    ]
    for alg in sample:
        mine = occ_of(alg)
        ref = occ_from_stickers(pysim.simulate(alg))
        assert mine == ref, f"face turn divergence {alg}"


def cross_check_layer_identities():
    pairs = [
        ("x x'", ""), ("x2 x2", ""), ("y y'", ""), ("z2 z2", ""),
        ("M M'", ""), ("E E'", ""), ("S S'", ""), ("M2 M2", ""),
        ("u u'", ""), ("r r'", ""), ("x x x x", ""), ("M M M M", ""),
        ("x", "R M' L'"), ("x'", "R' M L"), ("x2", "R2 M2 L2"),
        ("y", "U E' D'"), ("y'", "U' E D"), ("z", "F S B'"), ("z'", "F' S' B"),
    ]
    for left, right in pairs:
        if right:
            assert occ_of(left) == occ_of(right), f"identity fails {left} vs {right}"
        else:
            assert occ_of(left) == list(range(54)), f"{left} not identity"


def main():
    cross_check_face_turns()
    cross_check_layer_identities()
    rows = {}
    with open(os.path.join(REF, "rows.tsv"), newline="", encoding="utf-8") as f:
        for line in f:
            cols = line.rstrip("\r\n").split("\t")
            rows[(cols[0], cols[1], cols[2])] = cols[4]
    out_lines = []
    misses = 0
    with open(os.path.join(REF, "simcheck.tsv"), newline="", encoding="utf-8") as f:
        f.readline()
        for line in f:
            cols = line.rstrip("\r\n").split("\t")
            solver, sheet, rowid, position, htm, qtm, match, note = cols
            key = (solver, sheet, rowid)
            if key not in rows:
                misses += 1
                continue
            moves = rows[key]
            occ = occ_of(moves)
            out_lines.append("\t".join([
                solver, sheet, rowid, position, moves, qtm, match, note,
                ",".join(str(v) for v in occ),
            ]))
    with open(os.path.join(OUT, "simocc.tsv"), "w", encoding="utf-8", newline="") as f:
        f.write("\n".join(out_lines) + "\n")
    rng = random.Random(0xB)
    lengths = list(range(0, 130)) + [154, 155, 156, 157, 191, 192, 255, 256, 1000, 4096]
    vec_lines = []
    for n in lengths:
        data = bytes(rng.randrange(256) for _ in range(n))
        digest = hashlib.sha256(data).hexdigest()
        vec_lines.append(f"{n}\t{digest}\t{','.join(str(b) for b in data)}")
    with open(os.path.join(OUT, "sha256_vectors.tsv"), "w", encoding="utf-8", newline="") as f:
        f.write("\n".join(vec_lines) + "\n")
    composed = []
    parsed = []
    for line in out_lines:
        cols = line.split("\t")
        parsed.append((cols, [int(v) for v in cols[8].split(",")]))
    mains = [p for p in parsed if p[0][1] in ("edge", "corner")]
    residues = [p for p in parsed if p[0][1] in ("twists", "flips", "parity")]
    for s_cols, s_occ in mains:
        for t_cols, t_occ in residues:
            comp = [s_occ[t_occ[p]] for p in range(54)]
            tag_s = f"{s_cols[0]}/{s_cols[1]}#{s_cols[2]}"
            tag_t = f"{t_cols[0]}/{t_cols[1]}#{t_cols[2]}"
            composed.append("\t".join([
                tag_s, "composed", tag_t, s_cols[3] + " x " + t_cols[3],
                s_cols[4] + " + " + t_cols[4], "0", "COMPOSED", "",
                ",".join(str(v) for v in comp),
            ]))
    with open(os.path.join(OUT, "composed.tsv"), "w", encoding="utf-8", newline="") as f:
        f.write("\n".join(composed) + "\n")
    print(f"rows emitted {len(out_lines)} join misses {misses}")
    print(f"sha vectors {len(vec_lines)}")
    print(f"composed {len(composed)}")


if __name__ == "__main__":
    main()
