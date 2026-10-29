import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SLOT = REPO / "playground" / "mutcrash" / "slot"
SUITE = sys.argv[1] if len(sys.argv) > 1 else "data_paths_layer_test"
AREA_OF_SUITE = {
    "data_paths_layer_test": "data",
    "data_paths_more_test": "data",
    "common_config_test": "common",
}
CLOSURE = {
    "data": ["data", "core", "scheme", "common"],
    "common": ["common"],
}
TARGET_FILE = {
    "data_paths_layer_test": "src/data/commutator.c",
    "data_paths_more_test": "src/data/commutator.c",
    "common_config_test": "src/common/when.c",
}


def main():
    cfg = json.loads((REPO / "config" / "config.json").read_text(encoding="utf-8"))
    envj = json.loads((REPO / "build" / "env.json").read_text(encoding="utf-8"))
    clang = envj["clang"]["path"]
    build = cfg["build"]
    hash_dirs = [d for d in (REPO / "build" / "obj").iterdir() if d.is_dir()]
    obj_root = hash_dirs[0]
    print("obj root:", obj_root.name, "of", [d.name for d in hash_dirs])
    cflags = ["-target", build["target"], "-std=" + build["std"]]
    cflags += build["cflags"] + build["release_flags"]
    area = AREA_OF_SUITE[SUITE]
    closure = CLOSURE[area]
    if SLOT.exists():
        shutil.rmtree(SLOT)
    SLOT.mkdir(parents=True)
    target = TARGET_FILE[SUITE]
    shutil.copyfile(REPO / target, SLOT / Path(target).name)
    compile_argv = [
        clang, *cflags,
        "-I", str(REPO / Path(target).parent),
        "-I", str(REPO / "src"),
        "-I", str(REPO / "tools"),
        "-I", str(REPO / "tests"),
        "-isystem", str(REPO / "vendor" / "sqlite"),
        "-c", str(SLOT / Path(target).name),
        "-o", str(SLOT / (Path(target).stem + ".obj")),
    ]
    print("compile:", " ".join(compile_argv))
    r = subprocess.run(compile_argv, capture_output=True, text=True)
    print("compile rc:", r.returncode, r.stderr[-400:] if r.stderr else "")
    if r.returncode != 0:
        return 1
    objs = []
    for c in sorted((obj_root / "src").rglob("*.obj")):
        src_rel = c.relative_to(obj_root / "src").as_posix()
        file_area = src_rel.split("/")[0]
        if src_rel == Path(target).with_suffix(".obj").as_posix()[len("src/"):]:
            continue
        if file_area == "common" or file_area in closure:
            objs.append(str(c))
    test_obj = obj_root / "tests" / "unit" / (AREA_OF_SUITE[SUITE] + "/" + SUITE[len(AREA_OF_SUITE[SUITE]) + 1:] + ".obj")
    main_obj = obj_root / "gen" / (SUITE + "_main.obj")
    objs.append(str(SLOT / (Path(target).stem + ".obj")))
    objs.append(str(test_obj))
    objs.append(str(main_obj))
    for v in sorted((obj_root / "vendor").rglob("*.obj")):
        objs.append(str(v))
    link_argv = [
        clang, *cflags, "-fuse-ld=lld-link",
        "-Xlinker", "/SUBSYSTEM:CONSOLE", "-Xlinker", "/OPT:REF",
        "-Xlinker", "/OPT:ICF", "-Xlinker", "/DEBUG:NONE", "-Xlinker", "/brepro",
        *objs, "-o", str(SLOT / (SUITE + ".exe")),
    ]
    print("link objs:", len(objs))
    r = subprocess.run(link_argv, capture_output=True, text=True)
    print("link rc:", r.returncode, r.stderr[-600:] if r.stderr else "")
    if r.returncode != 0:
        return 1
    env = dict(os.environ)
    env["BT_CONFIG"] = str(REPO / "config" / "config.json")
    env["BT_ARTIFACTS"] = "artifacts"
    env["BT_REF_GROUND"] = "ref_ground"
    print("run:", SUITE)
    try:
        r = subprocess.run(
            [str(SLOT / (SUITE + ".exe"))],
            cwd=REPO, env=env, capture_output=True, text=True, timeout=30,
        )
        print("run rc:", r.returncode)
        print("stdout head:", r.stdout[:600])
        print("stdout tail:", r.stdout[-600:])
        print("stderr tail:", r.stderr[-400:] if r.stderr else "")
    except subprocess.TimeoutExpired as e:
        print("run TIMEOUT")
        print("stdout tail:", (e.stdout or b"")[-600:] if isinstance(e.stdout, bytes) else e.stdout)
    return 0


if __name__ == "__main__":
    sys.exit(main())
