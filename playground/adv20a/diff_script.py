import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

KEYS = {
    32: "Space",
    256: "Escape",
    257: "Enter",
    262: "Right",
    263: "Left",
    264: "Down",
    265: "Up",
    47: "Slash",
    49: "Digit1",
    50: "Digit2",
    51: "Digit3",
    52: "Digit4",
    66: "B",
    68: "D",
    78: "N",
    80: "P",
}

DRILL = {0: "Card", 1: "Reps", 2: "Rate"}
MEMO = {0: "Intro", 1: "Stream", 2: "Recall", 3: "Review"}
SOLVE = {0: "Ready", 1: "Memo", 2: "Exec", 3: "Review"}
CASE = {0: "Show", 1: "Grade"}


def c_actions(path):
    lines = path.read_text().splitlines()
    actions = []
    count = None
    fatal_count = None
    for line in lines:
        parts = line.split()
        if parts[0] == "count":
            count = int(parts[1])
            continue
        if parts[0] == "fatal-count":
            fatal_count = int(parts[1])
            continue
        index = int(parts[0])
        kind = parts[1]
        token = ""
        if len(parts) > 2:
            if kind == "Press":
                token = KEYS[int(parts[2])]
            elif kind == "PressChar":
                token = KEYS[int(parts[2])] + ":" + parts[3]
            elif kind in ("NavOpen", "WaitApp"):
                token = parts[2]
            elif kind in ("WaitDrill", "WaitMemo", "WaitSolve", "WaitCaseDrill"):
                n = int(parts[2])
                if kind == "WaitDrill":
                    token = DRILL[n]
                elif kind == "WaitMemo":
                    token = MEMO[n]
                elif kind == "WaitSolve":
                    token = SOLVE[n]
                else:
                    token = CASE[n]
            else:
                token = parts[2]
        actions.append((index, kind, token))
    return count, fatal_count, actions


STATE_MAP = {
    "Menu": "Menu",
    "Drill": "Drill",
    "Memo": "Memo",
    "Solve": "Solve",
    "Algs": "Algs",
    "CaseDrill": "CaseDrill",
    "Stats": "Stats",
    "Settings": "Settings",
    "Replay": "Replay",
    "Fatal": "Fatal",
}


def rust_actions(path):
    text = path.read_text()
    body = text[text.index("fn script(") :]
    body = body[: body.index("pub(crate) fn fatal_script")]
    lines = body.splitlines()
    actions = []
    pending = []
    partial = 0.8 * 3.25
    joined = []
    buffer = ""
    for line in lines:
        stripped = line.strip()
        if buffer:
            buffer += " " + stripped
            if "}" in stripped:
                joined.append(buffer.rstrip(","))
                buffer = ""
            continue
        if stripped.startswith("Action::PressChar {") and "}" not in stripped:
            buffer = stripped
            continue
        joined.append(stripped.rstrip(","))
    for stripped in joined:
        m = re.match(r"Action::(\w+)(.*)$", stripped)
        if not m:
            continue
        kind, rest = m.group(1), m.group(2).strip()
        if kind in ("Press",):
            key = re.search(r"KeyCode::(\w+)", rest).group(1)
            actions.append(("Press", key))
        elif kind == "PressChar":
            key = re.search(r"KeyCode::(\w+)", rest).group(1)
            ch = re.search(r"ch: '(.)'", rest).group(1)
            actions.append(("PressChar", key + ":" + ch))
        elif kind == "NavOpen":
            state = re.search(r"AppState::(\w+)", rest).group(1)
            actions.append(("NavOpen", STATE_MAP[state]))
        elif kind == "NavMenu":
            actions.append(("NavMenu", ""))
        elif kind == "PinDrill":
            actions.append(("PinDrill", ""))
        elif kind == "PickSolveSeed":
            actions.append(("PickSolveSeed", ""))
        elif kind == "WaitApp":
            state = re.search(r"AppState::(\w+)", rest).group(1)
            actions.append(("WaitApp", STATE_MAP[state]))
        elif kind == "WaitFonts":
            actions.append(("WaitFonts", ""))
        elif kind == "WaitDrill":
            phase = re.search(r"DrillPhase::(\w+)", rest).group(1)
            actions.append(("WaitDrill", phase))
        elif kind == "WaitMemo":
            phase = re.search(r"MemoPhase::(\w+)", rest).group(1)
            actions.append(("WaitMemo", phase))
        elif kind == "WaitSolve":
            phase = re.search(r"SolvePhase::(\w+)", rest).group(1)
            actions.append(("WaitSolve", phase))
        elif kind == "WaitCaseDrill":
            phase = re.search(r"CaseDrillPhase::(\w+)", rest).group(1)
            actions.append(("WaitCaseDrill", phase))
        elif kind == "Elapse":
            secs = rest.strip("()").split("(")[-1].rstrip(")")
            expr = rest.strip("(),")
            if "partial" in expr:
                value = partial
            else:
                value = float(expr.rstrip("f").rstrip("."))
            actions.append(("Elapse", "%.6f" % value))
        elif kind == "Shot":
            name = re.search(r'"([^"]+)"', rest).group(1)
            actions.append(("Shot", name))
    return actions


def normalize_key(name):
    fixed = {
        "Space": "Space",
        "Escape": "Escape",
        "Enter": "Enter",
        "ArrowRight": "Right",
        "ArrowLeft": "Left",
        "ArrowDown": "Down",
        "ArrowUp": "Up",
        "Slash": "Slash",
        "Digit1": "Digit1",
        "Digit2": "Digit2",
        "Digit3": "Digit3",
        "Digit4": "Digit4",
        "KeyB": "B",
        "KeyD": "D",
        "KeyN": "N",
        "KeyP": "P",
    }
    return fixed.get(name, name)


def main():
    count, fatal_count, c = c_actions(ROOT / "build" / "adv20a_script_dump.txt")
    r = rust_actions(ROOT / "crates" / "bt-app" / "src" / "usage_script.rs")
    print("c-count", count, "rust-count", len(r), "fatal-count", fatal_count)
    mismatches = 0
    for i in range(max(len(c), len(r))):
        cc = c[i] if i < len(c) else None
        rr = r[i] if i < len(r) else None
        ck = (cc[1], cc[2]) if cc else None
        rk = (rr[0], rr[1]) if rr else None
        if rk and rk[0] == "Press":
            rk = ("Press", normalize_key(rk[1]))
        elif rk and rk[0] == "PressChar":
            key, ch = rk[1].split(":")
            rk = ("PressChar", normalize_key(key) + ":" + ch)
        if ck != rk:
            mismatches += 1
            print("MISMATCH", i, ck, rk)
    if count != len(r):
        mismatches += 1
    print("mismatches", mismatches)
    if mismatches or count != 124 or fatal_count != 3:
        sys.exit(1)
    print("TABLE-OK")


if __name__ == "__main__":
    main()
