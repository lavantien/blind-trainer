import json
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
CONFIG = os.path.join(ROOT, "config", "config.json")
with open(CONFIG, encoding="utf-8") as f:
    config = json.load(f)
REF = os.path.join(ROOT, config["paths"]["ref_ground"])
VENDOR = os.path.join(ROOT, config["paths"]["data_vendor"])
ARTIFACTS = os.path.join(ROOT, config["paths"]["artifacts"])
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


def load_maps():
    letter_by_name = {}
    sticker_by_letter = {"corner": {}, "edge": {}}
    rows = {"corner": [], "edge": []}
    with open(os.path.join(REF, "position_map.tsv"), newline="", encoding="utf-8") as f:
        f.readline()
        for line in f:
            cols = line.rstrip("\r\n").split("\t")
            orbit, sticker, letter, name = cols[:4]
            face, row, col = FACE_LIST[int(cols[4])], int(cols[5]), int(cols[6])
            sticker = int(sticker)
            sticker_by_letter[orbit][letter] = sticker
            letter_by_name[name] = letter
            rows[orbit].append((sticker, face, row, col))
    return letter_by_name, sticker_by_letter, rows


def cross(a, b):
    return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]


def build_mate_and_succ(rows):
    mate = {}
    succ = {}
    for sticker, face, row, col in rows["edge"]:
        point = pysim.grid(face, row, col)
        for other in FACE_LIST:
            if other == face:
                continue
            rc = pysim.cell_of(other, point)
            if rc is not None:
                mate[sticker] = CELL_TO_ID[(other, rc[0], rc[1])]
                break
    for sticker, face, row, col in rows["corner"]:
        point = pysim.grid(face, row, col)
        normal = pysim.FACES[face]
        radial_dot = sum(p * n for p, n in zip(point, normal))
        spun = [p * radial_dot / 3 + c for p, c in zip(point, cross(point, normal))]
        best = None
        for other in FACE_LIST:
            rc = pysim.cell_of(other, point)
            if rc is None:
                continue
            score = sum(a * b for a, b in zip(spun, pysim.FACES[other]))
            if best is None or score > best[0]:
                best = (score, CELL_TO_ID[(other, rc[0], rc[1])])
        succ[sticker] = best[1]
    return mate, succ


def expected_occ(orbit, position, letter_by_name, sticker_by_letter, mate, succ):
    names = position.split("-")
    if len(names) != 3:
        raise ValueError(f"bad position {position}")
    a, b, c = [sticker_by_letter[orbit][letter_by_name[n]] for n in names]
    occ = list(range(54))
    cycles = [[a, b, c]]
    if orbit == "edge":
        cycles.append([mate[a], mate[b], mate[c]])
    else:
        cycles.append([succ[a], succ[b], succ[c]])
        cycles.append([succ[succ[a]], succ[succ[b]], succ[succ[c]]])
    for x, y, z in cycles:
        occ[y] = x
        occ[z] = y
        occ[x] = z
    return occ


def main():
    letter_by_name, sticker_by_letter, map_rows = load_maps()
    mate, succ = build_mate_and_succ(map_rows)
    with open(os.path.join(VENDOR, "cases.json"), encoding="utf-8") as f:
        cases = json.load(f)
    rows = []
    bad = 0
    for entry in cases["cases"]:
        for alg in entry["vendor_algs"]:
            if alg["solver"] != "generated":
                continue
            sim = occ_of(alg["alg"])
            want = expected_occ(entry["orbit"], alg["position"], letter_by_name, sticker_by_letter, mate, succ)
            ok = sim == want
            bad += 0 if ok else 1
            rows.append("\t".join([
                entry["orbit"], alg["position"], alg["alg"], str(alg["qtm"]),
                "OK" if ok else "MISMATCH", ",".join(str(v) for v in sim),
            ]))
    out = os.path.join(ARTIFACTS, "synth-simcheck.tsv")
    os.makedirs(ARTIFACTS, exist_ok=True)
    with open(out, "w", encoding="utf-8", newline="") as f:
        f.write("orbit\tposition\talg\tqtm\tsim\tsim_occ\n")
        f.write("\n".join(rows) + "\n")
    print(f"synth simcheck: {len(rows)} generated algs, {len(rows) - bad} ok, {bad} mismatch, artifact {out}")
    if bad:
        sys.exit(1)


if __name__ == "__main__":
    main()
