from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CORPUS = ROOT / "tests" / "fuzz" / "corpus" / "ga"
VENDORED = ROOT / "fuzz" / "fuzz" / "corpus" / "ga"

FILES = {
    "truncated-2.bin": b"ma\xc3",
    "truncated-3.bin": b"m\xe1\xba",
    "truncated-4.bin": b"\xf0\x9f\x98",
    "lone-continuation.bin": b"ma\x80",
    "overlong-2.bin": b"\xc0\xaf",
    "overlong-3.bin": b"\xe0\x80\xaf",
    "surrogate.bin": b"\xed\xa0\x80",
    "out-of-range.bin": b"\xf4\x90\x80\x80",
    "wordlist-ok.bin": b"# note\nb\xc3\xa0\n  la  \n",
    "wordlist-nbsp.bin": b"\xc2\xa0m\xc3\xa1\xc2\xa0\n",
    "freq-ok.bin": b"1\t500\tma\n2\t+300\tm\xc3\xa1\n",
    "freq-bad-count.bin": b"1\t18446744073709551616\tma\n",
    "freq-marked.bin": b"1\t500\tma\xcc\x81\n",
    "json-ok.bin": b'{"cases":[{"orbit":"edge","letters":"AB","count":120}],'
    b'"adjacent":[{"from":0,"to":17,"count":3}],"cooccurring":[],'
    b'"total_draws":1000,"seed":20261012}',
    "json-over-from.bin": b'{"cases":[],"adjacent":[{"from":4294967296,"to":1,"count":1}],'
    b'"cooccurring":[],"total_draws":1,"seed":2}',
    "json-long-orbit.bin": b'{"cases":[{"orbit":"0123456789abcdef","letters":"AB","count":1}],'
    b'"adjacent":[],"cooccurring":[],"total_draws":1,"seed":2}',
}


def main():
    for target in (CORPUS, VENDORED):
        target.mkdir(parents=True, exist_ok=True)
        for name, payload in FILES.items():
            (target / name).write_bytes(payload)
    print(f"{len(FILES)} corpus inputs under {CORPUS} and {VENDORED}")


if __name__ == "__main__":
    main()
