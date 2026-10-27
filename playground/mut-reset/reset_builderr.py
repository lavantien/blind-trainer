import os
import sys
import tempfile

REPO = r"C:\Users\lavantien\dev\github\blind-trainer"
LEDGER = os.path.join(REPO, r"artifacts\mutants\ledger-core_analysis_sheets_test.tsv")

with open(LEDGER, "r", encoding="utf-8", newline="") as fh:
    lines = fh.read().splitlines()

header = lines[0]
rows = [line.split("\t") for line in lines[1:] if line.strip()]
reset = 0
for row in rows:
    if len(row) >= 11 and row[7] == "builderr":
        row[7] = "pending"
        row[8] = "0"
        row[9] = ""
        reset += 1

fd, tmp = tempfile.mkstemp(dir=os.path.dirname(LEDGER), suffix=".tmp")
with os.fdopen(fd, "w", encoding="utf-8", newline="") as fh:
    fh.write(header + "\n")
    for row in rows:
        fh.write("\t".join(row) + "\n")
os.replace(tmp, LEDGER)
print(f"reset {reset} builderr rows to pending in {os.path.basename(LEDGER)}")
