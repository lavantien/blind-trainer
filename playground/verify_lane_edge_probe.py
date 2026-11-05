import sys

EDGE_TURNS = {
    "U": ["UF", "UL", "UB", "UR"],
    "D": ["DF", "DR", "DB", "DL"],
    "F": ["UF", "RF", "DF", "LF"],
    "B": ["UB", "RB", "DB", "LB"],
    "R": ["UR", "BR", "DR", "FR"],
    "L": ["UL", "BL", "DL", "FL"],
    "M": ["UB", "UF", "DF", "DB"],
}
DIRS = {"": 1, "'": 3, "2": 2}


def apply(pos, ring, quarters):
    at = ring.index(pos) if pos in ring else -1
    if at < 0:
        return pos
    return ring[(at + quarters) % 4]


def track(alg):
    edges = [f"{a}{b}" for a in "UFRLBD" for b in "FRLBUD" if a != b]
    edges = [e for e in edges if e in {p for ring in EDGE_TURNS.values() for p in ring}]
    where = {e: e for e in edges}
    for tok in alg.split():
        face = tok[0]
        quarters = DIRS[tok[1:] if len(tok) > 1 else ""]
        ring = EDGE_TURNS[face]
        where = {piece: apply(slot, ring, quarters) for piece, slot in where.items()}
    return {piece: slot for piece, slot in where.items() if piece != slot} and [
        (piece, slot) for piece, slot in sorted(where.items()) if piece != slot
    ]


for alg in [
    "R U R' U' R' F R2 U' R' U' R U R' F'",
    "F2 U2",
    "F2 U2 U R U' R U R U R U' R' U' R2 U'",
    "F2 U2 R U' R U R U R U' R' U' R2",
    "F2 U2 M2",
]:
    print(alg)
    for item in track(alg):
        print("   ", item)
