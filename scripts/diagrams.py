import argparse
import hashlib
import shutil
import subprocess
import sys
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import slog

VERSION = "1.2026.8"
SHA256 = "5e1ecfa8ecd32c90b03bbf3b1eb6f020943f98ab0fcf4032be31a0002ee2c462"
URL = f"https://github.com/plantuml/plantuml/releases/download/v{VERSION}/plantuml-{VERSION}.jar"
TOOL = slog.Tool("diagrams")
REPO = Path(__file__).resolve().parent.parent


def fail(message):
    return TOOL.line("fail", message)


def sha256_of(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def download(url, target, attempts=3, delay=2, timeout=300):
    for attempt in range(attempts):
        try:
            with urllib.request.urlopen(urllib.request.Request(url, headers={"User-Agent": "blind-trainer-diagrams/1.0"}), timeout=timeout) as response, open(target, "wb") as handle:
                shutil.copyfileobj(response, handle)
            return True
        except OSError as exc:
            fail(f"download attempt {attempt + 1} failed: {exc}")
            if attempt + 1 < attempts:
                import time

                time.sleep(delay)
    return False


def ensure_jar(jar):
    if jar.is_file() and sha256_of(jar) == SHA256:
        return True
    print(f"fetching plantuml {VERSION} jar")
    jar.parent.mkdir(parents=True, exist_ok=True)
    if not download(URL, jar):
        return False
    return sha256_of(jar) == SHA256


def main(argv=None):
    parser = argparse.ArgumentParser(
        prog="diagrams.py",
        description="render every docs/diagrams puml to png with the sha-pinned plantuml jar",
        allow_abbrev=False,
    )
    args = parser.parse_args(argv)
    TOOL.entry("diagrams")
    out = REPO / "docs" / "diagrams"
    tools = REPO / "artifacts" / "tools"
    jar = tools / f"plantuml-{VERSION}.jar"
    out.mkdir(parents=True, exist_ok=True)
    tools.mkdir(parents=True, exist_ok=True)
    if not ensure_jar(jar):
        actual = sha256_of(jar) if jar.exists() else "missing"
        fail(f"plantuml jar hash mismatch: expected {SHA256} got {actual}")
        return 1
    if shutil.which("java") is None:
        root_slash = str(REPO).replace("\\", "/")
        print(
            "java not found; docker fallback:"
            f" docker run --rm -v {root_slash}:/w -w /w/docs/diagrams plantuml/plantuml:{VERSION} -tpng '*.puml'"
        )
        return 1
    sources = sorted(out.glob("*.puml"))
    if not sources:
        fail(f"no puml sources found in {out}")
        return 1
    done = subprocess.run(
        ["java", "-jar", str(jar), "-failfast2", "-overwrite", "-charset", "UTF-8", "-tpng"]
        + [str(source) for source in sources]
    )
    if done.returncode != 0:
        fail(f"plantuml render failed with exit {done.returncode}")
        return 1
    for source in sources:
        png = source.with_suffix(".png")
        if not png.is_file():
            fail(f"missing rendered png for {source.name}")
            return 1
        print(f"rendered {source.stem}.png")
    TOOL.exit("diagrams", 0)
    return 0


if __name__ == "__main__":
    sys.exit(main())
