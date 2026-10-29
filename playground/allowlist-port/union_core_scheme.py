import difflib
import json
import os
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
MUT_DIR = os.path.join(REPO, "artifacts", "mutants")
PLANS_PATH = os.path.join(os.environ.get("TEMP", ""), "bt-mut-plans.json")
ALG_HOIST = "2b50aff"
ALG_PATH = "src/core/alg.c"
HOIST_TIME = "2026-10-28T07:38"

KEYWORDS = {
    "alignas", "alignof", "auto", "bool", "break", "case", "char", "constexpr",
    "const", "continue", "default", "do", "double", "else", "enum", "extern",
    "float", "for", "goto", "if", "inline", "int", "long", "nullptr", "register",
    "restrict", "return", "short", "signed", "sizeof", "static_assert", "static",
    "struct", "switch", "thread_local", "typedef", "typeof_unqual", "typeof",
    "union", "unsigned", "void", "volatile", "while", "_Alignas", "_Alignof",
    "_Atomic", "_BitInt", "_Bool", "_Complex", "_Decimal128", "_Decimal32",
    "_Decimal64", "_Generic", "_Imaginary", "_Noreturn", "_Static_assert",
    "_Thread_local",
}

OPERATORS = [
    "<<=", ">>=", "...", "%:%:", "->", "++", "--", "<<", ">>", "<=", ">=", "==",
    "!=", "&&", "||", "+=", "-=", "*=", "/=", "%=", "&=", "^=", "|=", "##", "<:",
    ":>", "<%", "%:", "+", "-", "*", "/", "%", "&", "|", "^", "~", "!", "<", ">",
    "=", "?", ":", ";", ",", ".", "(", ")", "[", "]", "{", "}", "#",
]


class Token:
    __slots__ = ("kind", "offset", "len", "line", "col", "text")

    def __init__(self, kind, offset, length, line, col, text):
        self.kind = kind
        self.offset = offset
        self.len = length
        self.line = line
        self.col = col
        self.text = text


def tokenize(text):
    tokens = []
    pos = 0
    line = 1
    col = 1
    n = len(text)

    def advance(count=1):
        nonlocal pos, line, col
        for _ in range(count):
            if pos < n and text[pos] == "\n":
                line += 1
                col = 1
            else:
                col += 1
            pos += 1

    while pos < n:
        c = text[pos]
        if c in " \t\r\n":
            advance()
            continue
        if c == "/" and pos + 1 < n and text[pos + 1] == "/":
            while pos < n and text[pos] != "\n":
                advance()
            continue
        if c == "/" and pos + 1 < n and text[pos + 1] == "*":
            start = pos
            advance(2)
            while pos + 1 < n and not (text[pos] == "*" and text[pos + 1] == "/"):
                advance()
            advance(2)
            tokens.append(Token("comment", start, pos - start, line, col, text[start:pos]))
            continue
        if c == "#":
            start = pos
            while pos < n and text[pos] != "\n" and not (text[pos] == "\\" and pos + 1 < n):
                if text[pos] == "\\" and pos + 1 < n and text[pos + 1] == "\n":
                    advance(2)
                    continue
                advance()
            tokens.append(Token("pp", start, pos - start, line, col, text[start:pos]))
            continue
        if c == '"' or c == "'":
            start = pos
            quote = c
            advance()
            while pos < n:
                if text[pos] == "\\":
                    advance(2)
                    continue
                if text[pos] == quote:
                    advance()
                    break
                if text[pos] == "\n":
                    break
                advance()
            kind = "string" if quote == '"' else "char"
            tokens.append(Token(kind, start, pos - start, line, col, text[start:pos]))
            continue
        if c.isdigit():
            start = pos
            while pos < n and (text[pos].isalnum() or text[pos] in "._"):
                advance()
            tokens.append(Token("number", start, pos - start, line, col, text[start:pos]))
            continue
        if c.isalpha() or c == "_":
            start = pos
            while pos < n and (text[pos].isalnum() or text[pos] == "_"):
                advance()
            word = text[start:pos]
            kind = "keyword" if word in KEYWORDS else "ident"
            tokens.append(Token(kind, start, pos - start, line, col, word))
            continue
        matched = None
        for op in OPERATORS:
            if text.startswith(op, pos):
                matched = op
                break
        if matched is not None:
            start = pos
            advance(len(matched))
            tokens.append(Token("op", start, pos - start, line, col, matched))
            continue
        advance()
    return tokens


def function_name_for_open(tokens, open_index):
    if open_index < 3:
        return ""
    close = tokens[open_index - 1]
    if close.kind != "op" or close.text != ")":
        return ""
    paren_depth = 1
    j = open_index - 1
    while j > 0 and paren_depth > 0:
        j -= 1
        scan = tokens[j]
        if scan.kind != "op" or scan.len != 1:
            continue
        if scan.text == ")":
            paren_depth += 1
        elif scan.text == "(":
            paren_depth -= 1
    if paren_depth != 0 or tokens[j].kind != "op":
        return ""
    k = j
    while k > 0 and tokens[k].kind != "ident":
        k -= 1
    if tokens[k].kind != "ident":
        return ""
    return tokens[k].text


def function_at(tokens, offset):
    depth = 0
    inside = False
    current = ""
    for i, tok in enumerate(tokens):
        if tok.kind != "op" or tok.len != 1:
            continue
        if tok.text == "{":
            if depth == 0:
                current = function_name_for_open(tokens, i)
                inside = offset >= tok.offset
            depth += 1
            continue
        if tok.text == "}":
            if inside and depth == 1 and tok.offset >= offset:
                return current
            depth = max(0, depth - 1)
            if depth == 0:
                inside = False
                current = ""
            continue
        if inside and depth >= 1 and tok.offset > offset:
            return current
    if depth >= 1 and inside:
        return current
    return ""


def offset_at(text, line, col):
    cur_line = 1
    cur_col = 1
    for i, ch in enumerate(text):
        if cur_line == line and cur_col == col:
            return i
        if ch == "\n":
            cur_line += 1
            cur_col = 1
        else:
            cur_col += 1
    return None


def git_show(revision, path):
    out = subprocess.run(
        ["git", "show", "%s:%s" % (revision, path)],
        cwd=REPO,
        capture_output=True,
        text=True,
        encoding="utf-8",
        check=True,
    )
    return out.stdout


def line_starts(text):
    starts = [0]
    for i, ch in enumerate(text):
        if ch == "\n":
            starts.append(i + 1)
    return starts


def token_matches(text, starts, line, col, from_text):
    if line - 1 >= len(starts):
        return False
    pos = starts[line - 1] + col - 1
    end = starts[line] - 1 if line < len(starts) else len(text)
    if pos + len(from_text) > end:
        return False
    return text[pos : pos + len(from_text)] == from_text


class AlgRemap:
    def __init__(self):
        self.old_text = git_show(ALG_HOIST + "^", ALG_PATH)
        with open(os.path.join(REPO, ALG_PATH.replace("/", os.sep)), "r", encoding="utf-8") as h:
            self.new_text = h.read()
        self.old_starts = line_starts(self.old_text)
        self.new_starts = line_starts(self.new_text)
        matcher = difflib.SequenceMatcher(
            a=self.old_text.splitlines(), b=self.new_text.splitlines(), autojunk=False
        )
        self.old_to_new = {}
        for tag, i1, i2, j1, j2 in matcher.get_opcodes():
            if tag == "equal":
                for k in range(i2 - i1):
                    self.old_to_new[i1 + k + 1] = j1 + k + 1

    def remap(self, line, col, from_text):
        if token_matches(self.new_text, self.new_starts, line, col, from_text):
            return ("fresh", line, col)
        if not token_matches(self.old_text, self.old_starts, line, col, from_text):
            return ("ambiguous", line, col)
        new_line = self.old_to_new.get(line)
        if new_line is None:
            return ("obsolete", line, col)
        if token_matches(self.new_text, self.new_starts, new_line, col, from_text):
            return ("remapped", new_line, col)
        return ("obsolete", line, col)


def load_plans():
    with open(PLANS_PATH, "r", encoding="utf-8") as handle:
        return json.load(handle)["plans"]


def read_ledger(suite):
    path = os.path.join(MUT_DIR, "ledger-%s.tsv" % suite)
    if not os.path.exists(path):
        return None
    rows = []
    with open(path, "r", encoding="utf-8") as handle:
        next(handle)
        for raw in handle:
            fields = raw.rstrip("\r\n").split("\t")
            if len(fields) < 8:
                continue
            rows.append(fields)
    return rows


def main():
    areas = sys.argv[1].split(",") if len(sys.argv) > 1 else ["core", "scheme"]
    plans = load_plans()
    selected = [p for p in plans if p["area"] in areas]

    suite_pending = {}
    for plan in selected:
        rows = read_ledger(plan["suite"])
        if rows is None:
            suite_pending[plan["suite"]] = -1
            continue
        suite_pending[plan["suite"]] = sum(1 for r in rows if r[7] == "pending")

    file_suites = {}
    for plan in selected:
        for source in plan["sources"]:
            file_suites.setdefault(source, []).append(plan["suite"])

    sites = {}
    complete_files = []
    incomplete_files = []
    remap = AlgRemap()
    remap_stats = {"fresh": 0, "remapped": 0, "obsolete": 0, "ambiguous": 0}
    obsolete_rows = []
    stale_post_hoist = []
    for source, suites in sorted(file_suites.items()):
        if any(suite_pending[s] != 0 for s in suites):
            incomplete_files.append(source)
            continue
        complete_files.append(source)
        for suite in suites:
            for r in read_ledger(suite):
                if r[1] != source:
                    continue
                line = int(r[2])
                col = int(r[3])
                if source == ALG_PATH:
                    state, new_line, new_col = remap.remap(line, col, r[5])
                    remap_stats[state] += 1
                    if state == "obsolete":
                        obsolete_rows.append((suite, line, col, r[4], r[5], r[6], r[7]))
                        continue
                    if state == "ambiguous":
                        obsolete_rows.append((suite, line, col, r[4], r[5], r[6], r[7]))
                        continue
                    if state == "remapped" and r[9] >= HOIST_TIME:
                        stale_post_hoist.append((suite, line, col, new_line, r[4], r[5], r[7], r[9]))
                    line = new_line
                    col = new_col
                key = (r[1], line, col, r[4], r[5], r[6])
                sites.setdefault(key, set()).add(r[7])

    union = {"killed": 0, "timeout": 0, "survived": 0, "unviable": 0, "pending": 0}
    survivors = []
    timeouts = []
    for key, statuses in sites.items():
        if "pending" in statuses:
            union["pending"] += 1
        elif "killed" in statuses:
            union["killed"] += 1
        elif "timeout" in statuses:
            union["timeout"] += 1
            timeouts.append(key)
        elif "survived" in statuses:
            union["survived"] += 1
            survivors.append(key)
        else:
            union["unviable"] += 1

    cache = {}

    def function_of(path, line, col):
        if path not in cache:
            full = os.path.join(REPO, path.replace("/", os.sep))
            try:
                with open(full, "r", encoding="utf-8") as handle:
                    text = handle.read()
            except OSError:
                cache[path] = (None, None)
                return ""
            cache[path] = (text, tokenize(text))
        text, tokens = cache[path]
        if text is None:
            return ""
        offset = offset_at(text, line, col)
        if offset is None:
            return ""
        return function_at(tokens, offset)

    grouped = {}
    for key in survivors:
        path, line, col, op, frm, to = key
        fn = function_of(path, line, col)
        gkey = (path, fn, op, "%s->%s" % (frm, to) if frm else "(empty)->%s" % to)
        grouped.setdefault(gkey, []).append((line, col))

    print("areas=%s" % ",".join(areas))
    for suite in sorted(suite_pending):
        if suite_pending[suite] != 0:
            print("incomplete suite %s pending=%d" % (suite, suite_pending[suite]))
    if all(v == 0 for v in suite_pending.values()):
        print("all %d suites complete (0 pending)" % len(suite_pending))
    print("complete_files=%d incomplete_files=%s" % (len(complete_files), incomplete_files))
    print("distinct_sites=%d" % len(sites))
    print(
        "union killed=%d timeout=%d survived=%d unviable=%d pending_sites=%d"
        % (union["killed"], union["timeout"], union["survived"], union["unviable"], union["pending"])
    )
    print("alg.c remap: %s" % remap_stats)
    print("obsolete or ambiguous alg.c rows excluded: %d" % len(obsolete_rows))
    for row in obsolete_rows[:10]:
        print("  obsolete %s" % (row,))
    print("remapped alg.c rows ruled after the hoist (suspect verdicts): %d" % len(stale_post_hoist))
    for row in stale_post_hoist[:10]:
        print("  suspect %s" % (row,))

    out = {
        "areas": areas,
        "suites": suite_pending,
        "complete_files": complete_files,
        "totals": union,
        "survivor_classes": [],
        "timeout_classes": [],
    }
    for gkey in sorted(grouped, key=lambda k: (k[0], k[1], k[2], k[3])):
        lines = sorted(grouped[gkey])
        entry = {
            "file": gkey[0],
            "function": gkey[1],
            "op": gkey[2],
            "replacement": gkey[3],
            "count": len(lines),
            "sites": ["%d:%d" % (ln, cl) for ln, cl in lines],
        }
        out["survivor_classes"].append(entry)
        print(
            "SURV %s\t%s\t%s\t%s\tcount=%d\tsites=%s"
            % (gkey[0], gkey[1], gkey[2], gkey[3], len(lines), ",".join(entry["sites"]))
        )

    timeout_groups = {}
    for key in timeouts:
        path, line, col, op, frm, to = key
        fn = function_of(path, line, col)
        gkey = (path, fn, op, "%s->%s" % (frm, to) if frm else "(empty)->%s" % to)
        timeout_groups.setdefault(gkey, []).append((line, col))
    for gkey in sorted(timeout_groups, key=lambda k: (k[0], k[1], k[2], k[3])):
        lines = sorted(timeout_groups[gkey])
        entry = {
            "file": gkey[0],
            "function": gkey[1],
            "op": gkey[2],
            "replacement": gkey[3],
            "count": len(lines),
            "sites": ["%d:%d" % (ln, cl) for ln, cl in lines],
        }
        out["timeout_classes"].append(entry)
        print(
            "TIMEOUT %s\t%s\t%s\t%s\tcount=%d\tsites=%s"
            % (gkey[0], gkey[1], gkey[2], gkey[3], len(lines), ",".join(entry["sites"]))
        )

    dump = os.path.join(os.path.dirname(__file__), "union_%s.json" % "_".join(areas))
    with open(dump, "w", encoding="utf-8") as handle:
        json.dump(out, handle, indent=1)
    print("wrote %s" % dump)


if __name__ == "__main__":
    main()
