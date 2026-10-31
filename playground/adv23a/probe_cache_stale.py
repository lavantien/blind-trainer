import json
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO))

import bt

STUB_TOOL = (
    "import sys\n"
    "from pathlib import Path\n"
    "argv = sys.argv[1:]\n"
    "with open(r'%s', 'a', encoding='utf-8') as log:\n"
    "    log.write(repr(argv) + '\\n')\n"
    "if '-o' in argv:\n"
    "    out = Path(argv[argv.index('-o') + 1])\n"
    "    out.parent.mkdir(parents=True, exist_ok=True)\n"
    "    out.write_bytes(b'stub')\n"
)


def hub_with_env(tmp):
    with open(REPO / "config" / "config.json", encoding="utf-8") as handle:
        hub = json.load(handle)
    hub["build"]["dirs"]["env"] = str(Path(tmp) / "env.json")
    return hub


def probe_arch_stale():
    tmp = tempfile.TemporaryDirectory()
    root = Path(tmp.name)
    log = root / "argv.log"
    for name in ("clang.py", "nvcc.py"):
        (root / name).write_text(STUB_TOOL % str(log), encoding="utf-8")
    (root / "src" / "ga" / "cuda").mkdir(parents=True)
    (root / "src" / "ga" / "cuda" / "ga_eval.cu").write_text("int cu;\n", encoding="utf-8")
    (root / "src" / "ga" / "cuda" / "ga_eval.h").write_text("int cu_h;\n", encoding="utf-8")
    (root / "vendor" / "sqlite").mkdir(parents=True)
    (root / "vendor" / "sqlite" / "sqlite3.c").write_text("int sq;\n", encoding="utf-8")
    with open(REPO / "config" / "config.json", encoding="utf-8") as handle:
        hub = json.load(handle)
    plan = {
        "ok": True,
        "nvcc": [sys.executable, "-I", str(root / "nvcc.py")],
        "ccbin": str(root / "hostx64"),
        "lib_dir": str(root / "lib" / "x64"),
    }
    compiler = [sys.executable, "-I", str(root / "clang.py")]
    bt.compile_stage(hub, root, compiler, bt.Logger(), cuda={**plan, "arch": "sm_86"})
    first = [ln for ln in log.read_text(encoding="utf-8").splitlines() if "-ccbin" in ln]
    bt.compile_stage(hub, root, compiler, bt.Logger(), cuda={**plan, "arch": "sm_120"})
    second = [ln for ln in log.read_text(encoding="utf-8").splitlines() if "-ccbin" in ln]
    ieee_change = {**plan, "arch": "sm_86"}
    result = {
        "nvcc_invocations_after_arch_switch": len(second) - len(first),
        "first_arch": first[0] if first else None,
        "total_nvcc_lines": len(second),
        "verdict": "stale cu reuse: arch change does not recompile the cu unit"
        if len(second) == len(first)
        else "recompiled",
    }
    tmp.cleanup()
    return result


def probe_cached_verdict_trust():
    tmp = tempfile.TemporaryDirectory()
    root = Path(tmp.name)
    hub = hub_with_env(root)
    hub["cuda"]["arch"] = "12.0"
    verdict = {
        "ok": True,
        "nvcc": str(root / "no-such-nvcc.exe"),
        "ccbin": str(root / "no-such-cl"),
        "arch": "sm_86",
        "lib_dir": str(root / "no-such-lib"),
        "detail": "poisoned",
    }
    bt.cache_cuda_verdict(hub, verdict)
    trusted = bt.cuda_toolchain(hub)
    result = {
        "returned_without_reprobe": trusted == verdict,
        "returned": trusted,
        "verdict": "cached verdict trusted blindly: nvcc path existence and the cuda.arch pin are never rechecked"
        if trusted == verdict
        else "reprobed",
    }
    tmp.cleanup()
    return result


def main():
    out = {
        "arch_stale": probe_arch_stale(),
        "verdict_trust": probe_cached_verdict_trust(),
    }
    here = Path(__file__).resolve().parent
    (here / "cache_stale.json").write_text(json.dumps(out, indent=2), encoding="utf-8")
    print(json.dumps(out, indent=2))


if __name__ == "__main__":
    main()
