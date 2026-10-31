import subprocess
import sys

repo = r"C:\Users\lavantien\dev\github\blind-trainer"
raw = subprocess.run(
    ["git", "-C", repo, "diff", "--raw", "--no-renames", "fa63c3d..fe736ca"],
    capture_output=True,
    text=True,
    check=True,
).stdout

deleted = []
added = []
for line in raw.splitlines():
    parts = line.split("\t")
    if len(parts) < 2:
        continue
    meta, path = parts[0], parts[1]
    fields = meta.split(" ")
    status = fields[4]
    src_sha = fields[2]
    dst_sha = fields[3]
    if status == "D":
        deleted.append(src_sha)
    elif status == "A":
        added.append(dst_sha)

def blob_bytes(shas):
    if not shas:
        return 0, 0
    proc = subprocess.run(
        ["git", "-C", repo, "cat-file", "--batch-check=%(objecttype) %(objectsize)"],
        input="\n".join(shas) + "\n",
        capture_output=True,
        text=True,
        check=True,
    )
    total = 0
    count = 0
    for row in proc.stdout.splitlines():
        kind, _, size = row.partition(" ")
        if kind == "blob":
            total += int(size)
            count += 1
    return count, total

for label, shas in (("deleted", deleted), ("added", added)):
    count, total = blob_bytes(shas)
    print(f"{label}: {count} blobs, {total} bytes, {total / 1024 / 1024:.2f} MiB")
