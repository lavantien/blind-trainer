import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

import bt

BASE = Path(__file__).resolve().parent
HARNESS_SRC = BASE / "fault_harness.c"


def compile_harness(hub, flavor):
    compiler = [bt.resolve_tool("clang", hub)]
    flags = bt.cuda_compile_flags(hub, cuda=flavor == "cuda")
    includes = [bt.REPO_ROOT / d for d in bt.INCLUDE_DIRS]
    obj = BASE / "obj" / f"fault_harness_{flavor}.obj"
    obj.parent.mkdir(parents=True, exist_ok=True)
    argv = bt.compile_command(compiler, flags, includes, HARNESS_SRC, obj)
    bt.run(argv, f"compile harness {flavor}")
    return obj


def link_objects(hub, flavor, harness_obj):
    _, _, units = bt.discover_compile_units(hub, bt.REPO_ROOT, cuda=flavor == "cuda")
    by_source = {source: obj for source, obj in units}
    areas = ("src/common/", "src/core/", "src/scheme/", "src/data/", "src/ga/")
    picked = []
    for source, obj in units:
        rel = str(source.relative_to(bt.REPO_ROOT)).replace("\\", "/")
        if rel.startswith(areas) and not rel.endswith("main.c"):
            picked.append(obj)
        elif rel in ("tools/ga-run/lib.c", "tools/ga-run/out.c"):
            picked.append(obj)
        elif rel == "vendor/sqlite/sqlite3.c":
            picked.append(obj)
    if flavor == "cuda":
        cu_obj = bt.object_path(
            bt.REPO_ROOT / "build" / "obj" / bt.objects_dirname(hub, cuda=True),
            bt.REPO_ROOT, bt.REPO_ROOT / bt.CUDA_UNIT)
        if not cu_obj.is_file():
            raise bt.BtError(f"cuda cu object missing: {cu_obj}")
        picked.append(cu_obj)
    missing = [str(o) for o in picked if not o.is_file()]
    if missing:
        raise bt.BtError(f"flavor {flavor} objects missing, build it first: {missing[:3]}")
    libs = ["cudart_static"] if flavor == "cuda" else []
    lib_dirs = []
    if flavor == "cuda":
        lib_dirs = [bt.cuda_toolchain(hub)["lib_dir"]]
    exe = BASE / f"fault_harness_{flavor}.exe"
    argv = bt.link_command([bt.resolve_tool("clang", hub)], bt.cuda_compile_flags(hub, cuda=flavor == "cuda"),
                           [*picked, harness_obj], libs, exe, lib_dirs=lib_dirs)
    bt.run(argv, f"link harness {flavor}")
    return exe


def write_configs(hub, scenario):
    lane = BASE / "lanes" / scenario
    lane.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(bt.REPO_ROOT / hub["paths"]["data_ga"] / "freq.json", lane / "freq.json")
    configs = {}
    for name, enable in (("on", True), ("off", False)):
        config = json.loads(json.dumps(hub))
        config["cuda"]["enable"] = enable
        config["paths"]["data_ga"] = str(lane)
        config["paths"]["artifacts"] = str(lane)
        path = lane / f"config-{name}.json"
        path.write_text(json.dumps(config, indent=2) + "\n", encoding="utf-8")
        configs[name] = path
    return configs


def run_scenario(hub, exe, mode, configs, log_name, hide_gpu):
    env = bt.child_env(hub)
    env["BT_CONFIG"] = str(configs["on"])
    env["BT_ARTIFACTS"] = str(configs["on"].parent)
    if hide_gpu:
        env["CUDA_VISIBLE_DEVICES"] = "-1"
    log = BASE / log_name
    with open(log, "w", encoding="utf-8") as handle:
        proc = subprocess.run(
            [str(exe), mode, str(configs["off"])],
            capture_output=True, text=True, cwd=str(bt.REPO_ROOT), env=env, timeout=1800,
        )
        handle.write(proc.stdout)
        handle.write("\n---- stderr ----\n")
        handle.write(proc.stderr)
    tail = proc.stdout.strip().splitlines()[-4:]
    print(f"[{log_name}] exit={proc.returncode}")
    for line in tail:
        print(f"    {line}")
    return proc.returncode


def main():
    hub = bt.load_hub()
    codes = {}
    for flavor in (sys.argv[1:] or ["plain", "cuda"]):
        obj = compile_harness(hub, flavor)
        exe = link_objects(hub, flavor, obj)
        for mode, hide in (("seam", False), ("ctxfail", True)):
            scenario = f"{flavor}-{mode}"
            if len(sys.argv) > 2 and scenario != sys.argv[2]:
                continue
            configs = write_configs(hub, scenario)
            codes[scenario] = run_scenario(
                hub, exe, mode, configs, f"harness-{scenario}.log", hide
            )
    print("summary", json.dumps(codes))
    return 0 if all(code == 0 for code in codes.values()) else 1


if __name__ == "__main__":
    sys.exit(main())
