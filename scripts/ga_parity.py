import hashlib
import json
import shutil
import subprocess
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT))

import bt

SCRATCH_RELPATH = "artifacts/parity"
EXACT_REPORT_FIELDS = (
    "best_generation",
    "generations_run",
    "stagnation_tail",
    "max_stagnation",
    "evaluations",
    "freq_sha256",
    "syllables_sha256",
)
TOLERANCE_REPORT_FIELDS = ("best_fitness", "best_parts", "per_gen_best")
GOLDEN_OUTPUTS = (("syllables.json", "syllables.json"), ("ga/report.md", "report.md"))


class ParityError(RuntimeError):
    pass


def log(tag, text):
    print(f"{time.strftime('%Y-%m-%dT%H:%M:%S')} [{tag}] {text}", flush=True)


def sha256_of(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def improvement_generations(per_gen):
    improved = []
    best = None
    for index, value in enumerate(per_gen):
        if best is None or value < best:
            improved.append(index)
            best = value
    return improved


def within_tolerance(left, right, tolerance):
    return abs(left - right) <= tolerance


def run_step(label, argv, env, timeout):
    unit_argv = [str(part) for part in argv]
    log("entry", f"{label} {subprocess.list2cmdline(unit_argv)}")
    started = time.monotonic()
    try:
        proc = subprocess.run(
            unit_argv,
            capture_output=True,
            text=True,
            cwd=str(REPO_ROOT),
            env=env,
            timeout=timeout,
            check=False,
        )
    except subprocess.TimeoutExpired as e:
        log("fail", f"{label} timeout after {timeout}s")
        raise ParityError(f"{label} timed out after {timeout}s") from e
    log("exit", f"{label} code={proc.returncode} dt={time.monotonic() - started:.3f}s")
    if proc.returncode != 0:
        tail = ((proc.stderr or "") + (proc.stdout or ""))[-2000:].strip()
        raise ParityError(f"{label} failed with code {proc.returncode}: {tail}")
    return proc


def scratch_config(repo_config, enable, data_ga, artifacts):
    config = json.loads(json.dumps(repo_config))
    config["cuda"]["enable"] = enable
    config["paths"]["data_ga"] = data_ga
    config["paths"]["artifacts"] = artifacts
    return config


def prepare_lane(hub, base, name, enable, source_freq):
    lane = base / name
    lane.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source_freq, lane / "freq.json")
    config_path = base / f"{name}-config.json"
    config_path.write_text(
        json.dumps(scratch_config(hub, enable, str(lane), str(lane)), indent=2) + "\n",
        encoding="utf-8",
    )
    return lane, config_path


def lane_env(hub, config_path, lane):
    env = bt.child_env(hub)
    env["BT_CONFIG"] = str(config_path)
    env["BT_ARTIFACTS"] = str(lane)
    return env


def load_report(lane):
    with open(lane / "report.json", encoding="utf-8") as handle:
        return json.load(handle)


def compare_reports(cpu, cuda, tolerance):
    drift = []
    for field in EXACT_REPORT_FIELDS:
        if cpu[field] != cuda[field]:
            drift.append(f"{field}: cpu {cpu[field]} vs cuda {cuda[field]}")
    for field in TOLERANCE_REPORT_FIELDS:
        left = cpu[field] if isinstance(cpu[field], list) else [cpu[field]]
        right = cuda[field] if isinstance(cuda[field], list) else [cuda[field]]
        if len(left) != len(right):
            drift.append(f"{field}: length {len(left)} vs {len(right)}")
            continue
        for index, (a, b) in enumerate(zip(left, right)):
            if not within_tolerance(a, b, tolerance):
                drift.append(f"{field}[{index}]: cpu {a} vs cuda {b} beyond {tolerance}")
    cpu_improved = improvement_generations(cpu["per_gen_best"])
    cuda_improved = improvement_generations(cuda["per_gen_best"])
    if cpu_improved != cuda_improved:
        drift.append(f"improvement generations: cpu {cpu_improved} vs cuda {cuda_improved}")
    return drift, cpu_improved


def check_cpu_golden(lane, golden_dir):
    drift = []
    hashes = {}
    for relative, name in GOLDEN_OUTPUTS:
        produced = lane / relative
        if not produced.is_file():
            drift.append(f"missing cpu lane output {name}")
            continue
        digest = sha256_of(produced)
        hashes[name] = digest
        if digest != sha256_of(golden_dir / name):
            drift.append(f"{name}: cpu lane hash {digest} diverges from the golden")
    cpu_report = load_report(lane)
    with open(golden_dir / "report.json", encoding="utf-8") as handle:
        golden_report = json.load(handle)
    cpu_fields = {k: v for k, v in cpu_report.items() if k != "run_config"}
    golden_fields = {k: v for k, v in golden_report.items() if k != "run_config"}
    if cpu_fields != golden_fields:
        drift.append("report.json outside run_config diverges from the golden")
    return drift, hashes


def run_suite_stem(hub, bin_dir, stem, config_path, lane, timeout):
    exe = bin_dir / f"ga_{stem}_test.exe"
    if not exe.is_file():
        raise ParityError(f"parity suite exe missing after the cuda build: {exe}")
    proc = run_step(f"suite {stem}", [exe], lane_env(hub, config_path, lane), timeout)
    tail = (proc.stdout or "")[-400:].strip()
    return {"stem": stem, "code": proc.returncode, "tail": tail.splitlines()[-1] if tail else ""}


def run_parity(hub):
    toolchain = bt.cuda_toolchain(hub)
    tolerance = float(hub["cuda"]["parity_tolerance"])
    timeout = int(bt.build_cfg(hub)["test_timeout_secs"])
    log("entry", "ga-parity build plain")
    if bt.tgt_build([]) != 0:
        raise ParityError("plain build failed")
    log("exit", "ga-parity build plain code=0")
    log("entry", "ga-parity build cuda")
    if bt.tgt_build(["--cuda"]) != 0:
        raise ParityError("cuda build failed")
    log("exit", "ga-parity build cuda code=0")
    base = REPO_ROOT / SCRATCH_RELPATH
    if base.is_dir():
        shutil.rmtree(base)
    base.mkdir(parents=True)
    source_freq = REPO_ROOT / hub["paths"]["data_ga"] / "freq.json"
    if not source_freq.is_file():
        raise ParityError(f"memo pair catalog missing: {source_freq}")
    golden_dir = REPO_ROOT / hub["paths"]["ref_ground"] / "ga"
    cpu_lane, cpu_config = prepare_lane(hub, base, "cpu", False, source_freq)
    cuda_lane, cuda_config = prepare_lane(hub, base, "cuda", True, source_freq)
    plain_bin = REPO_ROOT / bt.dirs_cfg(hub)["bin"]
    run_step("ga-run cpu", [plain_bin / "ga-run.exe"],
             lane_env(hub, cpu_config, cpu_lane), timeout * 4)
    run_step("ga-run cuda", [bt.cuda_bin_dir(hub) / "ga-run.exe"],
             lane_env(hub, cuda_config, cuda_lane), timeout * 4)
    golden_drift, golden_hashes = check_cpu_golden(cpu_lane, golden_dir)
    cpu_report = load_report(cpu_lane)
    cuda_report = load_report(cuda_lane)
    drift, improved = compare_reports(cpu_report, cuda_report, tolerance)
    if sha256_of(cuda_lane / "syllables.json") != sha256_of(cpu_lane / "syllables.json"):
        drift.append("syllables.json: cuda lane bytes differ from the cpu lane")
    suites = [
        run_suite_stem(hub, bt.cuda_bin_dir(hub), stem, cuda_config, cuda_lane, timeout)
        for stem in hub["cuda"]["parity_suites"]
    ]
    failures = golden_drift + drift
    verdict = {
        "ok": not failures,
        "toolchain": toolchain,
        "tolerance": tolerance,
        "cpu_golden_hashes": golden_hashes,
        "cpu_report_pins": {field: cpu_report[field] for field in EXACT_REPORT_FIELDS},
        "cuda_report_pins": {field: cuda_report[field] for field in EXACT_REPORT_FIELDS},
        "improvement_generations": improved,
        "drift": failures,
        "suites": suites,
    }
    return verdict


def write_verdict(verdict):
    path = REPO_ROOT / SCRATCH_RELPATH / "verdict.json"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(verdict, indent=2) + "\n", encoding="utf-8")
    return path


def main():
    started = time.monotonic()
    log("entry", "ga-parity")
    try:
        verdict = run_parity(bt.load_hub())
    except (ParityError, bt.BtError, OSError, KeyError, ValueError, TypeError) as e:
        log("fail", f"ga-parity {type(e).__name__}: {e}")
        write_verdict({"ok": False, "error": f"{type(e).__name__}: {e}"})
        log("exit", f"ga-parity code=1 dt={time.monotonic() - started:.3f}s")
        return 1
    base = REPO_ROOT / SCRATCH_RELPATH
    for lane in base.glob("*"):
        if lane.is_dir():
            shutil.rmtree(lane)
    for config in base.glob("*-config.json"):
        config.unlink()
    path = write_verdict(verdict)
    log("write", f"{path}")
    for line in verdict["drift"]:
        log("fail", f"drift {line}")
    for suite in verdict["suites"]:
        log("suite", f"{suite['stem']} code={suite['code']} {suite['tail']}")
    code = 0 if verdict["ok"] else 1
    log("exit", f"ga-parity code={code} dt={time.monotonic() - started:.3f}s")
    return code


if __name__ == "__main__":
    sys.exit(main())
