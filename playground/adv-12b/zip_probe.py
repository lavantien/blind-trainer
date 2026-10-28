import binascii
import json
import struct
import sys
import zlib
from pathlib import Path

repo = Path(__file__).resolve().parents[2]
zip_path = Path(sys.argv[1])
staging = Path(sys.argv[2])
release = json.loads((repo / "config" / "config.json").read_text(encoding="utf-8"))["release"]

blob = zip_path.read_bytes()
eocd = blob.rfind(b"PK\x05\x06")
if eocd < 0:
    sys.exit("no eocd")
count, cd_size, cd_offset = struct.unpack_from("<HII", blob, eocd + 10)
if eocd + 22 != len(blob):
    print(f"note: {len(blob) - eocd - 22} trailing bytes after eocd")
if cd_offset + cd_size != eocd:
    sys.exit(f"central dir gap: cd ends {cd_offset + cd_size}, eocd {eocd}")

expected = {}
expected[release["exe"]] = staging / release["exe"]
expected[release["config_file"]] = staging / release["config_file"]
fonts = repo / release["fonts_dir"]
for font in sorted(fonts.iterdir()):
    if font.is_file():
        expected[str(font.relative_to(repo)).replace("\\", "/")] = staging / release["fonts_dir"] / font.name

at = cd_offset
seen = {}
names = []
for _ in range(count):
    if struct.unpack_from("<I", blob, at)[0] != 0x02014B50:
        sys.exit(f"bad central sig at {at}")
    method = struct.unpack_from("<H", blob, at + 10)[0]
    crc, csize, usize = struct.unpack_from("<III", blob, at + 16)
    nlen, elen, comment_len = struct.unpack_from("<HHH", blob, at + 28)
    lho = struct.unpack_from("<I", blob, at + 42)[0]
    name = blob[at + 46 : at + 46 + nlen].decode("utf-8")
    names.append(name)
    if blob[lho : lho + 4] != b"PK\x03\x04":
        sys.exit(f"bad local sig for {name}")
    lnlen, lelen = struct.unpack_from("<HH", blob, lho + 26)
    lname = blob[lho + 30 : lho + 30 + lnlen].decode("utf-8")
    if lname != name:
        sys.exit(f"local name {lname} != central {name}")
    if struct.unpack_from("<H", blob, lho + 8)[0] != method:
        sys.exit(f"method mismatch in {name}")
    data_at = lho + 30 + lnlen + lelen
    raw = blob[data_at : data_at + csize]
    if method == 8:
        out = zlib.decompress(raw, -15)
    elif method == 0:
        out = raw
    else:
        sys.exit(f"method {method} in {name}")
    if len(out) != usize or binascii.crc32(out) & 0xFFFFFFFF != crc:
        sys.exit(f"crc or size mismatch in {name}")
    seen[name] = out
    at += 46 + nlen + elen + comment_len

root = staging.name + "/"
norm = {}
for name, out in seen.items():
    if name.endswith("/"):
        continue
    key = name[len(root):] if name.startswith(root) else name
    norm[key] = out
if sorted(norm) != sorted(expected):
    sys.exit(f"entry set mismatch: {sorted(norm)} vs {sorted(expected)}")
for key, src in sorted(expected.items()):
    want = src.read_bytes()
    if norm[key] != want:
        sys.exit(f"byte mismatch: {key}")
    print(f"ok {key} {len(want)} bytes method stored-in-zip")

print(f"zip ok: {count} central entries, {len(norm)} files, all bytes match staging")
print("names:", names)
