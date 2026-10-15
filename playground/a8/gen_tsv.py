import hashlib
import json
import re

PACKS = "../../ref_ground/packs"
TODAY = "2026-10-16"
PLL_URL = "https://jperm.net/algs/pll"
OLL_URL = "https://jperm.net/algs/oll"
F2L_URL = "https://jperm.net/3x3/cfop/f2l"

def wide_map(m):
    out = dict(m)
    for k, v in m.items():
        out[k.lower()] = v.lower()
    out["u"] = "u"
    out["d"] = "d"
    return out


FACE_MAPS = {
    "fr": {},
    "fl": wide_map({"F": "L", "R": "F", "B": "R", "L": "B", "U": "U", "D": "D"}),
    "bl": wide_map({"F": "B", "R": "L", "B": "F", "L": "R", "U": "U", "D": "D"}),
    "br": wide_map({"F": "R", "R": "B", "B": "L", "L": "F", "U": "U", "D": "D"}),
}
SLOT_ORDER = ["fr", "fl", "br", "bl"]
FOLLOWS = {"M": "L", "E": "D", "S": "F"}
SLICE_FOR = {"L": "M", "D": "E", "F": "S"}
SLICE_INV = {"R": "M", "U": "E", "B": "S"}
QUARTER = {"": 1, "2": 2, "2'": 2, "'": 3}
FMT_Q = {1: "", 2: "2", 3: "'"}


def parse_js(path):
    src = open(path, encoding="utf-8", errors="replace").read()
    body = src[src.find("algsetAlgs") :]
    out = []
    for m in re.finditer(r"\{name:(\"?)([^,]+?)\1,alg:\[(.*?)\]", body):
        name = m.group(2)
        algs = [a.strip().strip('"') for a in m.group(3).split('","')]
        algs = [a.strip('"') for a in algs]
        out.append((name, algs))
    return out


def map_token(tok, face_map):
    lead = tok[: len(tok) - len(tok.lstrip("("))]
    rest = tok[len(lead) :]
    tail = ""
    while rest and (rest[-1] == ")" or rest[-1].isdigit()):
        tail = rest[-1] + tail
        rest = rest[:-1]
    core = rest
    m = re.fullmatch(r"(2?)([UDLRFBudlrfbMESxyz])(2?'?)", core)
    assert m, tok
    inner, base, suffix = m.group(1), m.group(2), m.group(3)
    quarter = QUARTER[suffix]
    if base in "xyz":
        assert base == "y", tok
        newbase, nq = base, quarter
    elif base in FOLLOWS:
        assert not inner, tok
        g = face_map[FOLLOWS[base]]
        if g in SLICE_FOR:
            newbase, nq = SLICE_FOR[g], quarter
        else:
            newbase, nq = SLICE_INV[g], {1: 3, 2: 2, 3: 1}[quarter]
    else:
        newbase, nq = face_map[base], quarter
    return lead + inner + newbase + FMT_Q[nq] + tail


def map_alg(alg, face_map):
    return " ".join(map_token(t, face_map) for t in alg.split())


def has_rotation(alg):
    return any(re.fullmatch(r"[xyz]'?2?", t) for t in alg.split())


def pick_f2l(case):
    algs, multis = case["algs"], case["multi"]
    for idx, (alg, multi) in enumerate(zip(algs, multis)):
        if not has_rotation(alg) and not multi:
            return idx, alg, "primary"
    for idx, (alg, multi) in enumerate(zip(algs, multis)):
        if not has_rotation(alg):
            return idx, alg, "multi-slot" if multi else "primary"
    return 0, algs[0], "rotated"


def f2l_group(case):
    page, y = case["page"], case["y"]
    if page == 2:
        return "1a both on top, white faces up"
    if page == 3:
        return "1a both on top, white faces side, stickers different" if y < 400 else "1a both on top, white faces side, stickers same"
    if page == 4:
        return "1b edge in slot" if y < 470 else "1b corner in slot"
    return "1c both in slot"


def tsv(rows):
    lines = ["\t".join(["case_key", "alg", "front_json", "source_url", "source_at"])]
    lines += ["\t".join(r) for r in rows]
    return "\n".join(lines) + "\n"


def write(path, text):
    with open(path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(text)
    print(path, hashlib.sha256(text.encode()).hexdigest(), len(text.splitlines()) - 1, "rows")


def main():
    pll = parse_js("pll_raw.js")
    assert len(pll) == 21, len(pll)
    rows = []
    for name, algs in pll:
        rows.append([f"pll.{name.lower()}", algs[0], json.dumps({"case": name}, separators=(",", ":")), PLL_URL, TODAY])
    write(f"{PACKS}/cfop_pll.tsv", tsv(rows))

    oll = parse_js("oll_raw.js")
    assert len(oll) == 57, len(oll)
    src = open("oll_raw.js", encoding="utf-8", errors="replace").read()
    groups = dict(re.findall(r'\{name:(\d+),alg:\[.*?\],group:"([^"]+)"', src))
    assert len(groups) == 57
    rows = []
    for name, algs in oll:
        rows.append(
            [
                f"oll.{int(name):02d}",
                algs[0],
                json.dumps({"case": name, "group": groups[name]}, separators=(",", ":")),
                OLL_URL,
                TODAY,
            ]
        )
    write(f"{PACKS}/cfop_oll.tsv", tsv(rows))

    cases = json.load(open("f2l_cases.json", encoding="utf-8"))[:41]
    assert len(cases) == 41
    picks = []
    rows = []
    for i, case in enumerate(cases, 1):
        idx, alg, kind = pick_f2l(case)
        picks.append((i, idx, kind, alg))
        group = f2l_group(case)
        for slot in SLOT_ORDER:
            out = alg if slot == "fr" else map_alg(alg, FACE_MAPS[slot])
            front = {"case": f"{i:02d}", "slot": slot, "group": group}
            if idx:
                front["alt"] = idx
            rows.append([f"f2l.{slot}.{i:02d}", out, json.dumps(front, separators=(",", ":")), F2L_URL, TODAY])
    write(f"{PACKS}/cfop_f2l.tsv", tsv(rows))

    rotated = [p for p in picks if p[2] != "primary"]
    print("f2l picks not primary:", len(rotated))
    for i, idx, kind, alg in rotated:
        print(" ", i, kind, "alt", idx, alg)
    json.dump(picks, open("f2l_picks.json", "w", encoding="utf-8"), indent=1)


if __name__ == "__main__":
    main()
