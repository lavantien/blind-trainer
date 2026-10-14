import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.join(ROOT, "scripts"))
import synth_simcheck as sc

letter_by_name, sticker_by_letter = sc.load_maps()
alg = "R2 U' S R2 S' R2 U R2"
sim = sc.occ_of(alg)
a, b, c = [sticker_by_letter["edge"][letter_by_name[n]] for n in "UF UB UR".split()]
print("cycle stickers", a, b, c)
fwd = list(range(54))
fwd[b] = a
fwd[c] = b
fwd[a] = c
inv = list(range(54))
inv[a] = b
inv[b] = c
inv[c] = a
print("forward matches:", sim == fwd)
print("inverse matches:", sim == inv)
diff = [(i, sim[i], fwd[i]) for i in range(54) if sim[i] != fwd[i]]
print("diff vs forward:", diff)
