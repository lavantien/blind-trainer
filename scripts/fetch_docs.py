import argparse
import hashlib
import sys
import time
import urllib.request
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import slog

TOOL = slog.Tool("fetch-docs")
REPO = Path(__file__).resolve().parent.parent
REF_DOC = REPO / "ref_doc"
UA = "blind-trainer-docs-fetch/1.0 (offline grounding snapshot)"
ENTRIES = [
    {
        "path": "raylib/cheatsheet.html",
        "url": "https://www.raylib.com/cheatsheet/cheatsheet.html",
        "license": "zlib, raylib.com cheatsheet",
    },
    {
        "path": "raylib/examples.html",
        "url": "https://www.raylib.com/examples.html",
        "license": "zlib, raylib.com examples index",
    },
    {
        "path": "raylib/release-notes-6.2.html",
        "url": "https://github.com/raysan5/raylib/releases/tag/6.2",
        "license": "zlib, raysan5/raylib release notes",
    },
    {
        "path": "raylib/changelog.md",
        "url": "https://raw.githubusercontent.com/raysan5/raylib/master/CHANGELOG",
        "license": "zlib, raysan5/raylib changelog",
    },
    {
        "path": "c23/clang-c-status.html",
        "url": "https://clang.llvm.org/c_status.html",
        "license": "Apache-2.0 WITH LLVM-exception, LLVM clang C status page",
    },
    {
        "path": "win32/job-objects.html",
        "url": "https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects",
        "license": "MIT per Learn terms of use, Microsoft Learn job objects page",
    },
    {
        "path": "vietnamese/syllables-7184-abc.txt",
        "url": "https://raw.githubusercontent.com/vietnameselanguage/syllable/master/vietnamesesyllable_7184_sorted_abc.txt",
        "license": "no license file in repo, vietnameselanguage/syllable",
        "body_sha256": "bd55ccfef5ae0c9e85996561aa0a03556b686eaa362a8829e9e2d57c555b3dca",
    },
]


def select_entries(entries, only):
    if only is None:
        return list(entries)
    wanted = [part.strip() for part in only.split(",") if part.strip()]
    picked = [entry for entry in entries if entry["path"] in wanted]
    missing = [
        path for path in wanted if path not in {entry["path"] for entry in picked}
    ]
    if missing:
        raise ValueError(f"--only names no entry: {', '.join(missing)}")
    return picked


def verify_body(entry, body):
    pin = entry.get("body_sha256")
    if not pin:
        return True, ""
    digest = hashlib.sha256(body).hexdigest()
    if digest != pin:
        return (
            False,
            f"body sha mismatch for {entry['path']}: expected {pin} got {digest}",
        )
    return True, ""


def fail(message):
    return TOOL.line("fail", message)


def header_text(entry, retrieved):
    return f"# source: {entry['url']}\n# retrieved: {retrieved}\n# license: {entry['license']}\n"


def manifest_line(sha256, length, path, url):
    return f"{sha256}  {length}  {path}  {url}"


def preamble_lines(retrieved):
    return [
        "# ref_doc manifest",
        "",
        f"generated: {retrieved} via python scripts/fetch_docs.py",
        "raylib line: 6.2 is current as of 2026-10-27, released 2026-10-15, the win32 native backend at this tag self-documents as early stage so the glfw desktop backend stays the port target",
        "clang line: 23.1.x is current as of 2026-10-27, c23 complete since clang 20 per the c status page",
        "win32 line: job objects cover the jobwrap memory anchor port, KILL_ON_JOB_CLOSE with PROCESS_MEMORY limits",
        'file format: text files carry a 3-line header prefixed with "# " (source, retrieved, license), consumers skip lines starting with "# "',
        "format: sha256  bytes  path  url",
        "",
    ]


def sha256_of(path):
    digest = hashlib.sha256()
    digest.update(Path(path).read_bytes())
    return digest.hexdigest()


def fetch_one(entry, retrieved):
    target = REF_DOC / entry["path"]
    target.parent.mkdir(parents=True, exist_ok=True)
    last_error = ""
    for attempt in range(3):
        try:
            request = urllib.request.Request(entry["url"], headers={"User-Agent": UA})
            with urllib.request.urlopen(request, timeout=300) as response:
                body = response.read()
            if not body:
                return {"ok": False, "error": "empty body"}
            pinned, pin_error = verify_body(entry, body)
            if not pinned:
                return {"ok": False, "error": pin_error}
            target.write_bytes(header_text(entry, retrieved).encode("utf-8") + body)
            return {"ok": True, "error": ""}
        except OSError as exc:
            last_error = str(exc)
            time.sleep(2)
    return {"ok": False, "error": f"fetch failed after 3 attempts: {last_error}"}


def main(argv=None):
    parser = argparse.ArgumentParser(
        prog="fetch_docs.py",
        description="snapshot raylib 6.2, clang c23, win32, and the pinned vietnamese wordlist reference docs into ref_doc with license headers and a manifest",
        allow_abbrev=False,
    )
    parser.add_argument(
        "--only",
        help="comma-separated entry paths to fetch instead of the full set",
    )
    args = parser.parse_args(argv)
    entries = select_entries(ENTRIES, args.only)
    TOOL.entry("fetch-docs", f"entries={len(entries)} only={args.only or 'all'}")
    import datetime

    retrieved = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d")
    with ThreadPoolExecutor(max_workers=6) as pool:
        results = list(
            pool.map(lambda entry: {**entry, **fetch_one(entry, retrieved)}, entries)
        )
    broken = [result for result in results if not result["ok"]]
    if broken:
        for result in broken:
            fail(f"FAILED {result['path']} {result['url']} {result['error']}")
        return 1
    lines = preamble_lines(retrieved)
    for entry in entries:
        target = REF_DOC / entry["path"]
        lines.append(
            manifest_line(
                sha256_of(target), target.stat().st_size, entry["path"], entry["url"]
            )
        )
    manifest = REF_DOC / "MANIFEST.md"
    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    print(f"fetched {len(entries)} files into ref_doc, manifest at {manifest}")
    TOOL.exit("fetch-docs", 0)
    return 0


if __name__ == "__main__":
    sys.exit(main())
