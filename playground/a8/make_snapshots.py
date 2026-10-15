import hashlib
import shutil
from pathlib import Path

REF = Path("../../ref_doc/jperm")
REF.mkdir(parents=True, exist_ok=True)
TODAY = "2026-10-16"

HEADERS = {
    "pll-algs.js": "https://jperm.net/lib/pll.js?v=20190924-1156 (case data for https://jperm.net/algs/pll)",
    "oll-algs.js": "https://jperm.net/lib/oll.js?v=20190924-1156 (case data for https://jperm.net/algs/oll)",
}
LICENSE = "no explicit license on jperm.net algorithm pages, content by James Straughan (J Perm), local reference only"

for name, url in HEADERS.items():
    src = Path(f"{name.split('-')[0]}_raw.js")
    body = src.read_bytes().decode("utf-8")
    text = f"# source: {url}\n# retrieved: {TODAY}\n# license: {LICENSE}\n{body}"
    if not text.endswith("\n"):
        text += "\n"
    (REF / name).write_text(text, encoding="utf-8", newline="\n")

shutil.copyfile("bestf2l.pdf", REF / "f2l-bestf2l.pdf")
meta = (
    f"# source: https://bit.ly/bestf2l -> https://drive.google.com/open?id=1nzAXYUWZJ6H2wIOXaHdWXep3W57tArbR "
    f"(J Perm, Best Algorithms for ALL Advanced F2L Cases, updated 2021 August 8, linked from https://jperm.net/3x3/cfop/f2l)\n"
    f"# retrieved: {TODAY}\n"
    f"# license: free reference sheet by James Straughan (J Perm), no explicit license, local reference only\n"
)
(REF / "f2l-bestf2l.pdf.meta.txt").write_text(meta, encoding="utf-8", newline="\n")

for p in sorted(REF.iterdir()):
    data = p.read_bytes()
    print(hashlib.sha256(data).hexdigest(), len(data), "jperm/" + p.name)
