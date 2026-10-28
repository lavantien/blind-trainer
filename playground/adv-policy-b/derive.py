import json
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
PATTERNS = [("src", ".c"), ("src", ".h"), ("src", ".py"), ("tools", ".c"), ("tools", ".h"),
            ("tools", ".py"), ("tests", ".c"), ("tests", ".h"), ("tests", ".py"),
            ("scripts", ".c"), ("scripts", ".h"), ("scripts", ".py")]
SKIP_DIRS = {"vendor", "build", "crates", "target", "ref_ground", "ref_doc", "playground",
             "artifacts", "__pycache__"}
THRESHOLD = 4
SMALL_ALLOW = {str(n) for n in range(10)}


def collect():
    files = []
    for top, ext in PATTERNS:
        base = os.path.join(ROOT, top)
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
            for name in filenames:
                if name.lower().endswith(ext) and len(name) > len(ext):
                    files.append(os.path.join(dirpath, name))
    return sorted(set(files))


def is_word(b):
    return (97 <= b <= 122) or (65 <= b <= 90) or (48 <= b <= 57) or b == 95


def ident_start(b):
    return (97 <= b <= 122) or (65 <= b <= 90) or b == 95


def hex_digit(b):
    return (48 <= b <= 57) or (97 <= b <= 102) or (65 <= b <= 70)


def utf8_width(b):
    if b < 0x80:
        return 1
    if b >> 5 == 0x6:
        return 2
    if b >> 4 == 0xE:
        return 3
    return 4


def mask_source(text, lang):
    data = bytearray(text.encode("utf-8", "surrogateescape"))
    n = len(data)
    out = bytearray(data)
    i = 0

    def splice(at):
        while at < n and data[at] == 92 and (at + 1 < n and data[at + 1] in (10,)):
            at += 2
        while True:
            if at < n and data[at] == 92 and at + 2 < n and data[at + 1] == 13 and data[at + 2] == 10:
                at += 3
            else:
                break
        return at

    while i < n:
        b = data[i]
        if lang == "c":
            if b == 47:
                nxt = i + 1
                while nxt < n and data[nxt] == 92:
                    nxt = splice(nxt)
                if nxt < n and data[nxt] == 47:
                    end = nxt + 1
                    while end < n:
                        if data[end] == 10 and not (end >= 1 and data[end - 1] == 92) and not (
                                end >= 2 and data[end - 1] == 13 and data[end - 2] == 92):
                            break
                        end += 1
                    for k in range(i, min(end, n)):
                        out[k] = 32
                    i = end
                    continue
                if nxt < n and data[nxt] == 42:
                    j = splice(nxt + 1)
                    while j < n:
                        if data[j] == 42:
                            k = splice(j + 1)
                            if k < n and data[k] == 47:
                                end = k + 1
                                break
                        j += 1
                        j = splice(j)
                    else:
                        end = n
                    for k in range(i, min(end, n)):
                        out[k] = 32
                    i = end
                    continue
            if b == 34:
                j = i + 1
                while j < n:
                    if data[j] == 92:
                        j += 2
                        continue
                    if data[j] == 34:
                        break
                    j += 1
                close = j if j < n else -1
                if close >= 0:
                    for k in range(i + 1, close):
                        out[k] = 32
                    i = close + 1
                else:
                    for k in range(i + 1, n):
                        out[k] = 32
                    i = n
                continue
            if b == 39:
                q = i + 1
                end = -1
                if q < n and data[q] != 39:
                    if data[q] == 92:
                        j = q + 1
                        if j >= n:
                            end = -1
                        else:
                            if data[j] in (117, 85):
                                want = 4 if data[j] == 117 else 8
                                j += 1
                                seen = 0
                                while seen < want and j < n and hex_digit(data[j]):
                                    j += 1
                                    seen += 1
                                end = j if seen == want and j < n and data[j] == 39 else -1
                                if end == j and j < n and data[j] == 39:
                                    end = j + 1
                                else:
                                    end = -1
                            elif data[j] == 120:
                                j += 1
                                while j < n and hex_digit(data[j]):
                                    j += 1
                                end = j + 1 if j < n and data[j] == 39 else -1
                            elif 48 <= data[j] <= 55:
                                digits = 0
                                while digits < 3 and j < n and 48 <= data[j] <= 55:
                                    j += 1
                                    digits += 1
                                end = j + 1 if j < n and data[j] == 39 else -1
                            else:
                                j += 1
                                end = j + 1 if j < n and data[j] == 39 else -1
                    else:
                        w = utf8_width(data[q])
                        end = q + w + 1 if q + w < n and data[q + w] == 39 else -1
                if end > 0:
                    for k in range(i + 1, end - 1):
                        out[k] = 32
                    i = end
                else:
                    i += 1
                continue
            if ident_start(b):
                start = i
                while i < n and is_word(data[i]):
                    i += 1
                seg = bytes(data[start:i])
                prefix = (len(seg) == 1 and seg[0] in (117, 85, 76)) or (
                    len(seg) == 2 and seg[0] == 117 and seg[1] == 56)
                if i < n and data[i] == 34 and prefix:
                    j = i + 1
                    while j < n:
                        if data[j] == 92:
                            j += 2
                            continue
                        if data[j] == 34:
                            break
                        j += 1
                    close = j if j < n else -1
                    if close >= 0:
                        for k in range(i + 1, close):
                            out[k] = 32
                        i = close + 1
                    else:
                        for k in range(i + 1, n):
                            out[k] = 32
                        i = n
                    continue
                if i < n and data[i] == 39 and prefix:
                    save = i
                    q = i + 1
                    end = -1
                    if q < n and data[q] != 39:
                        if data[q] == 92:
                            j = q + 1
                            if j < n:
                                if data[j] in (117, 85):
                                    want = 4 if data[j] == 117 else 8
                                    j += 1
                                    seen = 0
                                    while seen < want and j < n and hex_digit(data[j]):
                                        j += 1
                                        seen += 1
                                    if seen == want and j < n and data[j] == 39:
                                        end = j + 1
                                elif data[j] == 120:
                                    j += 1
                                    while j < n and hex_digit(data[j]):
                                        j += 1
                                    if j < n and data[j] == 39:
                                        end = j + 1
                                elif 48 <= data[j] <= 55:
                                    digits = 0
                                    while digits < 3 and j < n and 48 <= data[j] <= 55:
                                        j += 1
                                        digits += 1
                                    if j < n and data[j] == 39:
                                        end = j + 1
                                else:
                                    j += 1
                                    if j < n and data[j] == 39:
                                        end = j + 1
                        else:
                            w = utf8_width(data[q])
                            if q + w < n and data[q + w] == 39:
                                end = q + w + 1
                    if end > 0:
                        for k in range(save + 1, end - 1):
                            out[k] = 32
                        i = end
                        continue
                    i = save
                continue
        else:
            if b == 35:
                end = n
                for k in range(i + 1, n):
                    if data[k] == 10:
                        end = k
                        break
                for k in range(i, end):
                    out[k] = 32
                i = end
                continue
            if b in (34, 39):
                delim = data[i]
                triple = i + 2 < n and data[i + 1] == delim and data[i + 2] == delim
                start = i + 3 if triple else i + 1
                j = start
                closed = -1
                while j < n:
                    if data[j] == 92:
                        j += 2
                        continue
                    if data[j] == 10 and not triple:
                        closed = j
                        break
                    if triple:
                        if data[j] == delim and j + 2 < n and data[j + 1] == delim and data[j + 2] == delim:
                            closed = j + 3
                            break
                    elif data[j] == delim:
                        closed = j + 1
                        break
                    j += 1
                if closed < 0:
                    closed = n
                for k in range(start, min(closed, n)):
                    out[k] = 32
                i = closed
                continue
            if ident_start(b):
                start = i
                while i < n and is_word(data[i]):
                    i += 1
                seg = bytes(data[start:i])
                if all(c in b"rRbBuUfF" for c in seg) and seg and i < n and data[i] in (34, 39):
                    delim = data[i]
                    triple = i + 2 < n and data[i + 1] == delim and data[i + 2] == delim
                    start2 = i + 3 if triple else i + 1
                    j = start2
                    closed = -1
                    while j < n:
                        if data[j] == 92:
                            j += 2
                            continue
                        if data[j] == 10 and not triple:
                            closed = j
                            break
                        if triple:
                            if data[j] == delim and j + 2 < n and data[j + 1] == delim and data[j + 2] == delim:
                                closed = j + 3
                                break
                        elif data[j] == delim:
                            closed = j + 1
                            break
                        j += 1
                    if closed < 0:
                        closed = n
                    for k in range(start2, min(closed, n)):
                        out[k] = 32
                    i = closed
                    continue
                continue
        i += 1
    return bytes(out).decode("utf-8", "surrogateescape")


def lang_of(path):
    low = path.lower()
    if low.endswith(".py") or low.endswith(".pyw"):
        return "py"
    return "c"


def rd(r, ch):
    if r in (120, 88):
        return hex_digit(ch)
    if r in (111, 79):
        return 48 <= ch <= 55
    return ch in (48, 49)


def sep_c(ch):
    return ch == 39


def sep_py(ch):
    return ch == 95


def line_of(text, at):
    return text.count("\n", 0, at) + 1


def core_int_value(core, lang):
    if not core:
        return None
    radix = 10
    at = 0
    if core[0] == "0" and len(core) > 1:
        c1 = core[1]
        if c1 in "xX":
            radix = 16
            at = 2
        elif c1 in "bB":
            radix = 2
            at = 2
        elif c1 in "oO":
            radix = 8
            at = 2
        elif lang == "c":
            radix = 8
            at = 1
        else:
            at = 1
    value = 0
    seen = 0
    for ch in core[at:]:
        if 48 <= ord(ch) <= 57:
            d = ord(ch) - 48
        elif 97 <= ord(ch) <= 102:
            d = ord(ch) - 87
        elif 65 <= ord(ch) <= 70:
            d = ord(ch) - 55
        else:
            return None
        if d >= radix:
            return None
        value = value * radix + d
        seen += 1
        if value > (1 << 64):
            return None
    if seen == 0:
        return None
    return value


def scan_literals(masked, lang):
    text = masked
    n = len(text)
    hits = []
    i = 0
    sep = sep_c if lang == "c" else sep_py

    def ds(ch):
        if 48 <= ch <= 57:
            return True
        return ch == 39 if lang == "c" else ch == 95

    while i < n:
        if not text[i].isdigit() or not ord(text[i]) < 128:
            i += 1
            continue
        b = ord(text[i])
        if not (48 <= b <= 57):
            i += 1
            continue
        leading_dot = False
        excluded = False
        if i > 0:
            prev = ord(text[i - 1])
            before = ord(text[i - 2]) if i >= 2 else 0
            prev_dots_range = prev == 46 and before == 46
            before_is_name = is_word(before) or (before & 0xC0) == 0x80
            if prev_dots_range:
                excluded = False
            elif prev == 46:
                if before_is_name:
                    excluded = True
                else:
                    leading_dot = True
            elif is_word(prev) or prev == 36 or (prev & 0xC0) == 0x80:
                excluded = True
        if excluded:
            i += 1
            continue
        start = i - 1 if leading_dot else i
        j = i
        radix = 0

        def radix_prefix(at):
            if at + 2 >= n:
                return 0
            r = ord(text[at + 1])
            first = ord(text[at + 2])
            if text[at] != "0":
                return 0
            known = r in (120, 88, 111, 79, 98, 66)
            if known and (rd(r, first) or first == 95):
                return r
            return 0

        radix = radix_prefix(j)
        if radix:
            j += 2
            while j < n and (rd(radix, ord(text[j])) or sep(ord(text[j]))):
                j += 1
            if radix in (120, 88):
                if j < n and text[j] == "." and not (j + 1 < n and text[j + 1] == "."):
                    j += 1
                    while j < n and (hex_digit(ord(text[j])) or sep(ord(text[j]))):
                        j += 1
                if j < n and text[j] in "pP":
                    k = j + 1
                    if k < n and text[k] in "+-":
                        k += 1
                    if k < n and (text[k].isdigit() or sep(ord(text[k]))):
                        j = k
                        while j < n and (text[j].isdigit() or sep(ord(text[j]))):
                            j += 1
        else:
            while j < n and ds(ord(text[j])):
                j += 1
            if j < n and text[j] == "." and not (j + 1 < n and text[j + 1] == "."):
                j += 1
                while j < n and ds(ord(text[j])):
                    j += 1
            if j < n and text[j] in "eE":
                k = j + 1
                if k < n and text[k] in "+-":
                    k += 1
                if k < n and ds(ord(text[k])):
                    j = k
                    while j < n and ds(ord(text[j])):
                        j += 1
        s = j
        while s < n and is_word(ord(text[s])):
            s += 1
        suffix_chars = "uUlLfF" if lang == "c" else "jJ"
        end = s if (s > j and all(c in suffix_chars for c in text[j:s])) else j
        boundary_ok = end >= n
        if not boundary_ok:
            after = ord(text[end])
            if is_word(after) or after == 46:
                boundary_ok = after == 46 and end + 1 < n and text[end + 1] == "."
            else:
                boundary_ok = True
        if boundary_ok:
            core = "".join(ch for ch in text[start:j] if ch not in "_'")
            value = core_int_value(core, lang)
            if value is not None and value < (1 << 64):
                core = str(value)
            hits.append((line_of(text, start), core))
        i = end if end > i else i + 1
    return hits


def load_allowlist():
    rows = []
    with open(os.path.join(ROOT, "config", "constants_allow.txt"), encoding="utf-8") as fh:
        for raw in fh:
            line = raw.rstrip("\r\n")
            if not line or line.startswith("#"):
                continue
            parts = line.split("\t")
            if len(parts) < 3:
                continue
            rows.append((parts[0], parts[1]))
    return set(rows)


def numeric_value(core):
    v = core_int_value(core, "c")
    if v is not None:
        return ("i", v)
    try:
        return ("f", float(core))
    except ValueError:
        return None


def main():
    files = collect()
    allow = load_allowlist()
    seen_rows = set()
    dup_rows = []
    with open(os.path.join(ROOT, "config", "constants_allow.txt"), encoding="utf-8") as fh:
        for raw in fh:
            line = raw.rstrip("\r\n")
            if not line or line.startswith("#"):
                continue
            parts = line.split("\t")
            key = (parts[0], parts[1]) if len(parts) >= 2 else (line, "")
            if key in seen_rows:
                dup_rows.append(key)
            seen_rows.add(key)
    print("ALLOWLIST_ROWS " + str(len(seen_rows) + len(dup_rows)))
    print("ALLOWLIST_DUPES " + (", ".join(f"{p} TAB {c}" for p, c in dup_rows) or "NONE"))
    expected = []
    covered = set()
    per_file_cores = {}
    for path in files:
        rel = os.path.relpath(path, ROOT).replace("\\", "/")
        if "tests" in rel.split("/"):
            continue
        with open(path, "rb") as fh:
            raw = fh.read()
        text = raw.decode("utf-8", "surrogateescape")
        lang = lang_of(path)
        masked = mask_source(text, lang)
        hits = scan_literals(masked, lang)
        counts = {}
        first_line = {}
        for ln, core in hits:
            counts[core] = counts.get(core, 0) + 1
            first_line.setdefault(core, ln)
        per_file_cores[rel] = counts
        for core, cnt in counts.items():
            if core in SMALL_ALLOW:
                continue
            covered.add((rel, core))
            if (rel, core) not in allow and cnt >= THRESHOLD:
                expected.append((rel, first_line[core], core, cnt))
    expected.sort()
    stale = sorted(pair for pair in allow if pair not in covered)
    print("DERIVED_FINDINGS")
    for rel, ln, core, cnt in expected:
        print(f"{rel}:{ln}: literal {core} x{cnt}")
    print("DERIVED_STALE")
    for pair in stale:
        print(pair[0] + " TAB " + pair[1])
    print("VALUE_SPLIT_CANDIDATES")
    for rel, counts in sorted(per_file_cores.items()):
        groups = {}
        for core, cnt in counts.items():
            v = numeric_value(core)
            if v is None:
                continue
            key = float(v[1])
            groups.setdefault(key, []).append((core, cnt))
        for v, members in groups.items():
            total = sum(cnt for _, cnt in members)
            if total >= THRESHOLD and len(members) > 1:
                top = any(cnt >= THRESHOLD for _, cnt in members)
                if not top and not (float(v).is_integer() and 0 <= v < 10):
                    spellings = ", ".join(f"{c}x{n}" for c, n in sorted(members))
                    print(f"{rel}: value {v} total {total} across {spellings}")
    print(f"FILES_SCANNED_CONSTANTS {len(per_file_cores)}")
    print(f"FILES_WALKED {len(files)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
