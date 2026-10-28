import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
PARENT = "2b50aff^"


def git_show(path):
    proc = subprocess.run(["git", "-C", str(REPO), "show", PARENT + ":" + path], capture_output=True)
    if proc.returncode != 0:
        sys.stderr.write(proc.stderr.decode("utf-8", "replace"))
        raise SystemExit("extract_old: git show failed for " + PARENT + ":" + path)
    return proc.stdout


def extract_function(raw, name):
    source = raw.decode("utf-8")
    marker = "static size_t " + name + "("
    start = source.find(marker)
    if start < 0:
        raise SystemExit("extract_old: marker not found: " + marker)
    brace = source.find("{", start)
    depth = 0
    for at in range(brace, len(source)):
        if source[at] == "{":
            depth += 1
        elif source[at] == "}":
            depth -= 1
            if depth == 0:
                return source[start : at + 1]
    raise SystemExit("extract_old: unterminated function body")


def main():
    lane = Path(__file__).resolve().parent
    alg = git_show("src/core/alg.c")
    (lane / "old_alg.c").write_bytes(alg)
    body = extract_function(alg, "whitespace_len").replace("whitespace_len", "old_whitespace_len")
    body = body.replace("static size_t old_whitespace_len", "size_t old_whitespace_len", 1)
    (lane / "old_ws.c").write_text("#include <stddef.h>\n\n" + body + "\n", encoding="ascii")
    print("extract_old: old_alg.c " + str(len(alg)) + " bytes, old_ws.c extracted")


if __name__ == "__main__":
    main()
