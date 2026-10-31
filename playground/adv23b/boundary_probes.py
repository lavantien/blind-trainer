import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

import bt

BASE = Path(__file__).resolve().parent
SCRATCH = BASE / "scratch"
REPO = bt.REPO_ROOT
PLAIN_GA = REPO / "build" / "bin" / "ga-run.exe"
CUDA_GA = REPO / "build" / "bin-cuda" / "ga-run.exe"
GOLDEN = REPO / "ref_ground" / "ga"


def sha256_of(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def hub_variant(name, mutate):
    hub = json.loads((REPO / "config" / "config.json").read_text(encoding="utf-8"))
    mutate(hub)
    path = SCRATCH / "configs" / f"{name}.json"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(hub, indent=2) + "\n", encoding="utf-8")
    return path


def seed_env(name, verdict_mutate):
    env_dir = SCRATCH / "envs" / name
    env_dir.mkdir(parents=True, exist_ok=True)
    real = json.loads((REPO / "build" / "env.json").read_text(encoding="utf-8"))
    verdict = dict(real["cuda"])
    verdict_mutate(verdict)
    env = {"cuda": verdict}
    path = env_dir / "env.json"
    path.write_text(json.dumps(env, indent=2) + "\n", encoding="utf-8")
    return path


def run_bt(target, config_path, log_name, extra_env=None):
    env = os.environ.copy()
    env["BT_CONFIG"] = str(config_path)
    if extra_env:
        env.update(extra_env)
    log = BASE / log_name
    with open(log, "w", encoding="utf-8") as handle:
        proc = subprocess.run(
            [sys.executable, "bt.py", *target], capture_output=True, text=True,
            cwd=str(REPO), env=env, timeout=900,
        )
        handle.write(proc.stdout)
        handle.write("\n---- stderr ----\n")
        handle.write(proc.stderr)
    return proc


def make_lane(name, config_mutate):
    lane = SCRATCH / "lanes" / name
    lane.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(REPO / "data" / "ga" / "freq.json", lane / "freq.json")

    def mutate(hub):
        hub["cuda"]["enable"] = False
        hub["paths"]["data_ga"] = str(lane)
        hub["paths"]["artifacts"] = str(lane)
        config_mutate(hub)

    config = hub_variant(f"lane-{name}", mutate)
    return lane, config


def run_ga(exe, config, lane, log_name, extra_env=None):
    env = bt.child_env(bt.load_hub())
    env["BT_CONFIG"] = str(config)
    env["BT_ARTIFACTS"] = str(lane)
    if extra_env:
        env.update(extra_env)
    log = BASE / log_name
    with open(log, "w", encoding="utf-8") as handle:
        proc = subprocess.run(
            [str(exe)], capture_output=True, text=True, cwd=str(REPO), env=env, timeout=1200,
        )
        handle.write(proc.stdout)
        handle.write("\n---- stderr ----\n")
        handle.write(proc.stderr)
    return proc


def lane_state(lane):
    report = json.loads((lane / "report.json").read_text(encoding="utf-8"))
    return {
        "syllables_sha": sha256_of(lane / "syllables.json"),
        "report_md_sha": sha256_of(lane / "ga" / "report.md"),
        "report_fields": {k: v for k, v in report.items() if k != "run_config"},
    }


def main():
    SCRATCH.mkdir(parents=True, exist_ok=True)
    results = []

    def record(name, detail, defect):
        results.append((name, detail, defect))
        print(f"[{name}] {detail} defect={defect}")

    ref_lane, ref_config = make_lane("cpu-ref", lambda hub: None)
    proc = run_ga(PLAIN_GA, ref_config, ref_lane, "ga-cpu-ref.log")
    ref = lane_state(ref_lane)
    record("cpu-ref", f"exit={proc.returncode} evals={ref['report_fields']['evaluations']}",
           proc.returncode != 0)
    golden_match = (
        ref["syllables_sha"] == sha256_of(GOLDEN / "syllables.json")
        and ref["report_md_sha"] == sha256_of(GOLDEN / "report.md")
    )
    golden_report = json.loads((GOLDEN / "report.json").read_text(encoding="utf-8"))
    golden_fields = {k: v for k, v in golden_report.items() if k != "run_config"}
    record("defaults-golden",
           f"syllables+report.md byte identical={golden_match} report fields equal="
           f"{golden_fields == ref['report_fields']}",
           not (golden_match and golden_fields == ref['report_fields']))

    lane, config = make_lane("plain-enable-on", lambda hub: hub["cuda"].update(enable=True))
    proc = run_ga(PLAIN_GA, config, lane, "ga-plain-enable-on.log")
    record("plain-enable-on",
           f"exit={proc.returncode} outputs_match_cpu={lane_state(lane) == ref} "
           f"stderr_empty={not proc.stderr.strip()}",
           proc.returncode != 0 or lane_state(lane) != ref)

    for label, value in (("enable-string", "on"), ("enable-int", 1), ("enable-null", None)):
        lane, config = make_lane(f"cuda-{label}", lambda hub, v=value: hub["cuda"].update(enable=v))
        proc = run_ga(CUDA_GA, config, lane, f"ga-cuda-{label}.log")
        record(f"cuda-{label}",
               f"exit={proc.returncode} outputs_match_cpu={lane_state(lane) == ref} "
               f"stderr_empty={not proc.stderr.strip()}",
               proc.returncode != 0 or lane_state(lane) != ref)

    lane, config = make_lane("cuda-ctxfail", lambda hub: hub["cuda"].update(enable=True))
    proc = run_ga(CUDA_GA, config, lane, "ga-cuda-ctxfail.log",
                  extra_env={"CUDA_VISIBLE_DEVICES": ""})
    record("cuda-ctxfail",
           f"exit={proc.returncode} outputs_match_cpu={lane_state(lane) == ref} "
           f"stderr_empty={not proc.stderr.strip()}",
           proc.returncode != 0 or lane_state(lane) != ref)

    arch_null = hub_variant(
        "arch-null",
        lambda hub: (hub["cuda"].update(arch=None),
                     hub["build"]["dirs"].update(env=str(SCRATCH / "envs" / "arch-null" / "env.json"))),
    )
    proc = run_bt(["doctor"], arch_null, "bt-doctor-arch-null.log")
    tail = (proc.stderr or "").strip().splitlines()[-1:] or ["<no stderr>"]
    record("doctor-arch-null", f"exit={proc.returncode} last={tail[0][:160]}", proc.returncode != 0)
    proc = run_bt(["build", "--cuda"], arch_null, "bt-build-cuda-arch-null.log")
    fail_line = [line for line in (proc.stdout or "").splitlines() if "fail" in line][-1:]
    record("build-cuda-arch-null",
           f"exit={proc.returncode} {fail_line[0][:160] if fail_line else '<no fail line>'}",
           proc.returncode != 0)

    override = hub_variant(
        "override-fail-cold",
        lambda hub: (hub["cuda"].update(doctor_override="fail"),
                     hub["build"]["dirs"].update(
                         env=str(SCRATCH / "envs" / "override-fail-cold" / "env.json"))),
    )
    proc = run_bt(["build", "--cuda"], override, "bt-build-cuda-override-cold.log")
    fail_line = [line for line in (proc.stdout or "").splitlines() if "fail" in line]
    record("override-fail-cold-cache",
           f"exit={proc.returncode} {fail_line[-1][:160] if fail_line else '<built>'}",
           proc.returncode == 0)

    seeded = seed_env("override-fail-warm", lambda v: None)
    override_warm = hub_variant(
        "override-fail-warm",
        lambda hub: (hub["cuda"].update(doctor_override="fail"),
                     hub["build"]["dirs"].update(env=str(seeded))),
    )
    proc = run_bt(["build", "--cuda"], override_warm, "bt-build-cuda-override-warm.log")
    record("override-fail-warm-cache", f"exit={proc.returncode}", proc.returncode == 0)

    poisoned = seed_env("nvcc-bogus", lambda v: v.update(nvcc="C:/nonexistent/nvcc.exe"))
    bogus_cfg = hub_variant(
        "nvcc-bogus",
        lambda hub: hub["build"]["dirs"].update(env=str(poisoned)),
    )
    proc = run_bt(["build", "--cuda"], bogus_cfg, "bt-build-cuda-nvcc-bogus.log")
    record("poisoned-verdict-warm-tree", f"exit={proc.returncode}", proc.returncode == 0)

    pinned = seed_env("arch-pin-warm", lambda v: None)
    pin_cfg = hub_variant(
        "arch-pin-9",
        lambda hub: (hub["cuda"].update(arch="9.0"),
                     hub["build"]["dirs"].update(env=str(pinned))),
    )
    proc = run_bt(["build", "--cuda"], pin_cfg, "bt-build-cuda-arch-pin9-warm.log")
    cuda_line = [line for line in (proc.stdout or "").splitlines() if "[cuda]" in line]
    record("arch-pin-ignored-warm-cache",
           f"exit={proc.returncode} {cuda_line[0][-90:] if cuda_line else '<no cuda line>'}",
           proc.returncode != 0 or (cuda_line and "sm_86" in cuda_line[0]))

    pin_cold = hub_variant(
        "arch-pin-9-cold",
        lambda hub: (hub["cuda"].update(arch="9.0"),
                     hub["build"]["dirs"].update(
                         env=str(SCRATCH / "envs" / "arch-pin-9-cold" / "env.json"))),
    )
    proc = run_bt(["build", "--cuda"], pin_cold, "bt-build-cuda-arch-pin9-cold.log")
    fail_line = [line for line in (proc.stdout or "").splitlines() if "fail" in line]
    record("arch-pin-9-cold-cache",
           f"exit={proc.returncode} {fail_line[-1][:160] if fail_line else '<built>'}",
           proc.returncode == 0)

    print("\nsummary")
    for name, detail, defect in results:
        print(f"  {name}: defect={defect} {detail}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
