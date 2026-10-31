import hashlib
import json
import os
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
BASE = Path(__file__).resolve().parent
PLAIN = REPO / "build" / "bin" / "ga-run.exe"
CUDA_BIN = REPO / "build" / "bin-cuda" / "ga-run.exe"


def sha256_of(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def lane_config(name, enable):
    lane = BASE / f"lane-{name}"
    lane.mkdir(parents=True, exist_ok=True)
    source_freq = REPO / "data" / "ga" / "freq.json"
    if source_freq.is_file():
        import shutil

        shutil.copyfile(source_freq, lane / "freq.json")
    with open(REPO / "config" / "config.json", encoding="utf-8") as handle:
        config = json.load(handle)
    config["cuda"]["enable"] = enable
    config["paths"]["data_ga"] = str(lane)
    config["paths"]["artifacts"] = str(lane)
    config_path = BASE / f"lane-{name}-config.json"
    config_path.write_text(json.dumps(config, indent=2) + "\n", encoding="utf-8")
    return lane, config_path


def run_case(tag, exe, config_path, lane, hide_gpu):
    env = dict(os.environ)
    env["BT_CONFIG"] = str(config_path)
    env["BT_ARTIFACTS"] = str(lane)
    if hide_gpu:
        env["CUDA_VISIBLE_DEVICES"] = ""
    started = time.monotonic()
    proc = subprocess.run(
        [str(exe)],
        capture_output=True,
        text=True,
        cwd=str(REPO),
        env=env,
        timeout=1500,
        check=False,
    )
    dt = time.monotonic() - started
    combined = (proc.stdout or "") + (proc.stderr or "")
    (BASE / f"run-{tag}.log").write_text(combined, encoding="utf-8")
    report = lane / "report.json"
    pins = {}
    if report.is_file():
        with open(report, encoding="utf-8") as handle:
            body = json.load(handle)
        pins = {k: body.get(k) for k in ("best_fitness", "best_generation", "evaluations")}
    return {
        "tag": tag,
        "exe": str(exe),
        "hide_gpu": hide_gpu,
        "code": proc.returncode,
        "secs": round(dt, 1),
        "cuda_mentions_in_output": combined.lower().count("cuda"),
        "report_sha": sha256_of(report) if report.is_file() else None,
        "syllables_sha": sha256_of(lane / "syllables.json")
        if (lane / "syllables.json").is_file()
        else None,
        "pins": pins,
    }


def main():
    if not PLAIN.is_file() or not CUDA_BIN.is_file():
        print(json.dumps({"error": "missing binaries", "plain": PLAIN.is_file(),
                          "cuda": CUDA_BIN.is_file()}))
        return 1
    lane_plain, cfg_plain = lane_config("plaincpu", True)
    lane_hidden, cfg_hidden = lane_config("cudahidden", True)
    lane_gpu, cfg_gpu = lane_config("cudagpu", True)
    results = [
        run_case("plain_enable_true", PLAIN, cfg_plain, lane_plain, False),
        run_case("cuda_gpu_hidden", CUDA_BIN, cfg_hidden, lane_hidden, True),
        run_case("cuda_gpu_visible", CUDA_BIN, cfg_gpu, lane_gpu, False),
    ]
    out = {
        "results": results,
        "report_identical_plain_vs_hidden": results[0]["report_sha"] == results[1]["report_sha"],
        "report_identical_plain_vs_gpu": results[0]["report_sha"] == results[2]["report_sha"],
        "syllables_identical_plain_vs_gpu": results[0]["syllables_sha"] == results[2]["syllables_sha"],
    }
    (BASE / "garun_runtime.json").write_text(json.dumps(out, indent=2), encoding="utf-8")
    print(json.dumps(out, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
