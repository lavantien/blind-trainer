import argparse
import hashlib
import sys
import time
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import slog

TOOL = slog.Tool("fetch-fonts")
REPO = Path(__file__).resolve().parent.parent
UA = "blind-trainer-fonts-fetch/1.0 (pinned asset download)"
FONTS = [
    {
        "name": "NotoSans-Regular.ttf",
        "sha256": "478c558ea716033cd60c03438f628dfa75694dcf6b5f6d505a2f05fd2b4f3823",
        "url": "https://raw.githubusercontent.com/notofonts/notofonts.github.io/main/fonts/NotoSans/hinted/ttf/NotoSans-Regular.ttf",
    },
    {
        "name": "NotoSans-Bold.ttf",
        "sha256": "1df075a380fc7cb898acf64c1f7b3b4dd780de3caa860178bf929de35817a913",
        "url": "https://raw.githubusercontent.com/notofonts/notofonts.github.io/main/fonts/NotoSans/hinted/ttf/NotoSans-Bold.ttf",
    },
    {
        "name": "OFL.txt",
        "sha256": "f2095b08bed08b23a6fe26112fcd679a2bee3f002eef077eb05d215ed1051bd8",
        "url": "https://raw.githubusercontent.com/notofonts/notofonts.github.io/main/fonts/LICENSE",
    },
]


def fail(message):
    return TOOL.line("fail", message)


def sha256_of(path):
    digest = hashlib.sha256()
    digest.update(Path(path).read_bytes())
    return digest.hexdigest()


def needs_download(existing_sha, expected):
    if existing_sha is None:
        return True
    return existing_sha != expected


def download(url, target, attempts=3, delay=2):
    for attempt in range(attempts):
        try:
            request = urllib.request.Request(url, headers={"User-Agent": UA})
            with urllib.request.urlopen(request, timeout=300) as response:
                body = response.read()
            Path(target).write_bytes(body)
            return True
        except OSError as exc:
            fail(f"download attempt {attempt + 1} failed: {exc}")
            if attempt + 1 < attempts:
                time.sleep(delay)
    return False


def main(argv=None):
    parser = argparse.ArgumentParser(
        prog="fetch_fonts.py",
        description="fetch the sha256-pinned noto sans fonts into assets/fonts",
        allow_abbrev=False,
    )
    args = parser.parse_args(argv)
    TOOL.entry("fetch-fonts", f"fonts={len(FONTS)}")
    fonts_dir = REPO / "assets" / "fonts"
    fonts_dir.mkdir(parents=True, exist_ok=True)
    broken = False
    for font in FONTS:
        target = fonts_dir / font["name"]
        existing = sha256_of(target) if target.is_file() else None
        if needs_download(existing, font["sha256"]):
            if existing is not None:
                print(f"hash mismatch for existing {font['name']}: {existing}, refetching")
            tmp = Path(str(target) + ".download")
            if not download(font["url"], tmp):
                print(f"download failed for {font['name']} from {font['url']}")
                broken = True
                continue
            tmp.replace(target)
        actual = sha256_of(target)
        if actual != font["sha256"]:
            print(f"hash mismatch for {font['name']}: expected {font['sha256']}, got {actual}")
            broken = True
        else:
            print(f"ok {font['name']} {actual}")
    if broken:
        fail("font fetch failed: hash mismatch or download error")
        return 1
    TOOL.exit("fetch-fonts", 0)
    return 0


if __name__ == "__main__":
    sys.exit(main())
