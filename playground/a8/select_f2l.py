import json
import re

ROT_TOKEN = re.compile(r"^[xyz]'?2?$")


def has_rotation(alg):
    return any(ROT_TOKEN.match(t) for t in alg.split())


def section1(cases):
    out = []
    for c in cases:
        if c["page"] > 5:
            break
        out.append(c)
    return out


def main():
    cases = json.load(open("f2l_cases.json", encoding="utf-8"))
    sec1 = section1(cases)
    print("section 1 cases:", len(sec1))
    for i, c in enumerate(sec1, 1):
        algs = c["algs"]
        pick = None
        for idx, (alg, multi) in enumerate(zip(algs, c["multi"])):
            if not has_rotation(alg) and not multi:
                pick = (idx, alg, "clean")
                break
        if pick is None:
            for idx, (alg, multi) in enumerate(zip(algs, c["multi"])):
                if not has_rotation(alg):
                    pick = (idx, alg, "multi" if multi else "clean-rot-skip")
                    break
        if pick is None:
            pick = (0, algs[0], "rot-only")
        idx, alg, kind = pick
        print(f"{i:02d} pick{idx} {kind:12s} {alg}")
        if kind != "clean":
            print("    all:", algs)


if __name__ == "__main__":
    main()
