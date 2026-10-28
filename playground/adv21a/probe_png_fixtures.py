import re
import struct
import sys
import zlib

REPO = r"C:\Users\lavantien\dev\github\blind-trainer"
SRC = REPO + r"\tests\unit\e2e\driver_test.c"
SIG = bytes([137, 80, 78, 71, 13, 10, 26, 10])


def extract(name):
    text = open(SRC, "r", encoding="utf-8").read()
    match = re.search(name + r"\[\]\s*=\s*\{(.*?)\};", text, re.S)
    if match is None:
        sys.exit("array not found: " + name)
    nums = [int(tok) for tok in re.findall(r"\d+", match.group(1))]
    return bytes(nums)


def walk(data, label):
    print(label, "len", len(data))
    if data[:8] != SIG:
        print(label, "BAD signature")
        return
    at = 8
    while at < len(data):
        if at + 8 > len(data):
            print(label, "truncated chunk header at", at)
            return
        length = struct.unpack(">I", data[at : at + 4])[0]
        kind = data[at + 4 : at + 8]
        body = data[at + 8 : at + 8 + length]
        crc_stored = struct.unpack(">I", data[at + 8 + length : at + 12 + length])[0]
        crc_calc = zlib.crc32(kind + body) & 0xFFFFFFFF
        ok_crc = "ok" if crc_stored == crc_calc else "MISMATCH"
        line = f"  chunk {kind.decode()} len {length} crc {ok_crc}"
        if kind == b"IHDR":
            w, h, depth, color = struct.unpack(">IIBB", body[:10])
            line += f" dims {w}x{h} depth {depth} color {color}"
        if kind == b"IDAT":
            try:
                raw = zlib.decompress(body)
                line += f" zlib ok raw {len(raw)} bytes"
                expected = h * (1 + w * 4) if kind == b"IDAT" and "h" in dir() else None
                if expected is not None:
                    line += f" expected {expected}"
            except zlib.error as err:
                line += f" zlib BROKEN: {err}"
        print(line)
        at += 12 + length
        if kind == b"IEND":
            break
    if at != len(data):
        print(label, "trailing bytes after IEND:", len(data) - at)


def main():
    good = extract("TD_PNG_3X2")
    wrong = extract("TD_PNG_2X2")
    walk(good, "TD_PNG_3X2")
    walk(wrong, "TD_PNG_2X2")


if __name__ == "__main__":
    main()
