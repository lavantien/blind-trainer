import json
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.join(ROOT, "scripts"))
import synth_simcheck as sc

with open(os.path.join(ROOT, "data", "vendor", "cases.json"), encoding="utf-8") as f:
    cases = json.load(f)

letter_by_name, sticker_by_letter, map_rows = sc.load_maps()
mate, succ = sc.build_mate_and_succ(map_rows)
checked = 0
mismatch = 0
skipped = 0
for entry in cases["cases"]:
    for alg in entry["vendor_algs"]:
        if alg["solver"] == "generated":
            continue
        if not alg["ok"] or alg["notes"]:
            skipped += 1
            continue
        try:
            want = sc.expected_occ(entry["orbit"], alg["position"], letter_by_name, sticker_by_letter, mate, succ)
        except KeyError:
            skipped += 1
            continue
        sim = sc.occ_of(alg["alg"])
        checked += 1
        if sim != want:
            mismatch += 1
            if mismatch <= 5:
                print("mismatch", entry["orbit"], alg["position"], alg["alg"])
print(f"selftest: {checked} clean vendor rows checked, {mismatch} mismatch, {skipped} skipped")
sys.exit(1 if mismatch else 0)
