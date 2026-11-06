import json
import os
import subprocess
import sys

ROOT = os.getcwd()
LANE = os.path.join(ROOT, "artifacts", "lane", "lane-cov-verify")
EXE = os.path.join(LANE, "build", "bin_cov", "verify.exe")
LLVM = os.environ.get("BT_LLVM", r"C:\Program Files\LLVM\bin")
ARMS = {
    "second_load": (32, 35),
    "digest": (149, 151),
    "to_json": (199, 202),
    "serialize": (208, 211),
}


def spawn_with_profile(k, artifacts, tag):
    profdir = os.path.join(LANE, "prof-seam")
    os.makedirs(profdir, exist_ok=True)
    raw = os.path.join(profdir, f"{tag}.profraw")
    if os.path.exists(raw):
        os.remove(raw)
    env = os.environ.copy()
    env.pop("BT_CONFIG", None)
    env.pop("BT_REF_GROUND", None)
    env.pop("BT_ARTIFACTS", None)
    if artifacts is not None:
        env["BT_ARTIFACTS"] = artifacts
    env["BT_VERIFY_ALLOC_FAIL_OK"] = str(k)
    env["LLVM_PROFILE_FILE"] = raw
    proc = subprocess.run([EXE], capture_output=True, text=True, env=env, timeout=300)
    if not os.path.exists(raw):
        return proc.returncode, None, "no profile"
    profdata = os.path.join(profdir, f"{tag}.profdata")
    subprocess.run(
        [os.path.join(LLVM, "llvm-profdata.exe"), "merge", f"--output={profdata}", raw],
        capture_output=True,
        check=True,
    )
    export = subprocess.run(
        [
            os.path.join(LLVM, "llvm-cov.exe"),
            "export",
            f"--instr-profile={profdata}",
            EXE,
        ],
        capture_output=True,
        text=True,
        check=True,
    )
    return proc.returncode, export.stdout, (proc.stderr.splitlines()[:1] or [""])[0][:80]


def arm_status(export):
    if export is None:
        return {}
    data = json.loads(export)["data"][0]
    out = {}
    for entry in data["files"]:
        rel = entry["filename"].replace("\\", "/")
        if not rel.endswith("tools/verify/main.c"):
            continue
        counts = {}
        for seg in entry["segments"]:
            line, count, has = seg[0], seg[2], seg[3]
            if has:
                counts.setdefault(line, 0)
                counts[line] = max(counts[line], count)
        for name, (lo, hi) in ARMS.items():
            lines = [counts.get(line, 0) for line in range(lo, hi + 1)]
            out[name] = "LIT" if any(value > 0 for value in lines) else "dark"
    return out


def main():
    cases = [
        ("a16384", 16384, "artifacts"),
        ("a17000", 17000, "artifacts"),
        ("a18000", 18000, "artifacts"),
        ("a20000", 20000, "artifacts"),
        ("a36000", 36000, "artifacts"),
        ("a100000", 100000, "artifacts"),
        ("a150000", 150000, "artifacts"),
        ("a200000", 200000, "artifacts"),
        ("a225000", 225000, "artifacts"),
        ("a230000", 230000, "artifacts"),
        ("a232000", 232000, "artifacts"),
        ("a232500", 232500, "artifacts"),
        ("a233000", 233000, "artifacts"),
        ("a233174", 233174, "artifacts"),
        ("b36000", 36000, None),
        ("b40000", 40000, None),
        ("b43000", 43000, None),
        ("b46000", 46000, None),
        ("b48000", 48000, None),
        ("b49000", 49000, None),
    ]
    for tag, k, artifacts in cases:
        code, export, first = spawn_with_profile(k, artifacts, tag)
        status = arm_status(export)
        print(tag, f"k={k}", f"code={code}", status, f"first[{first}]")


if __name__ == "__main__":
    main()
