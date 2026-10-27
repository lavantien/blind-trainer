import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO))

import bt


def compile_unit(compiler, flags, includes, system_dirs, source, obj_root, logger):
    obj = bt.object_path(obj_root, bt.REPO_ROOT, source)
    dep = obj.with_suffix(".d")
    obj.parent.mkdir(parents=True, exist_ok=True)
    if not bt.needs_compile(source, obj, dep):
        return obj
    argv = bt.compile_command(compiler, flags, includes, source, obj, dep,
                              bt.vendor_extra_flags(source), system_dirs)
    proc = bt.run(argv, f"compile {source.relative_to(bt.REPO_ROOT)}", check=False)
    if proc.returncode != 0:
        tail = ((proc.stderr or "") + (proc.stdout or ""))[-3000:].strip()
        logger.fail("compile", tail or "no output")
        raise SystemExit(1)
    bt.patch_coff_timestamp(obj)
    return obj


def main():
    stems = sys.argv[1:]
    if not stems:
        print("usage: python playground/l10/run_suite.py <area>:<stem>...")
        return 2
    hub = bt.load_hub()
    compiler = [bt.resolve_tool("clang", hub)]
    logger = bt.Logger()
    root = bt.REPO_ROOT
    flags = bt.compile_flags(hub)
    includes = [root / d for d in bt.INCLUDE_DIRS]
    system_dirs = bt.system_include_dirs(hub)
    obj_base = root / bt.dirs_cfg(hub)["obj"]
    obj_root = obj_base / bt.objects_dirname(hub)
    bt.prune_stale_obj_roots(obj_base, obj_root, logger)
    areas = bt.tool_area_closure(hub, ["verify"])
    sources = sorted(root.glob("src/common/**/*.c"))
    for area in sorted(areas):
        sources.extend(sorted(root.glob(f"src/{area}/**/*.c")))
    sources.extend(sorted(p for p in root.glob("tools/verify/*.c") if p.name != "main.c"))
    sources.append(root / "vendor/sqlite/sqlite3.c")
    objects = [
        compile_unit(compiler, flags, includes, system_dirs, source, obj_root, logger)
        for source in sources
    ]
    bin_dir = root / bt.dirs_cfg(hub)["bin"]
    bin_dir.mkdir(parents=True, exist_ok=True)
    timeout = bt.build_cfg(hub)["test_timeout_secs"]
    failed = 0
    for spec in stems:
        if spec.startswith("exe:"):
            tool = spec.split(":", 1)[1]
            tool_sources = sorted(root.glob(f"tools/{tool}/*.c"))
            tool_objects = [
                compile_unit(compiler, flags, includes, system_dirs, source, obj_root, logger)
                for source in tool_sources
            ]
            exe = bin_dir / f"{tool}.exe"
            argv = bt.link_command(compiler, flags, [*objects, *tool_objects], [], exe)
            proc = bt.run(argv, f"link {exe.name}", check=False)
            if proc.returncode != 0:
                tail = ((proc.stderr or "") + (proc.stdout or ""))[-3000:].strip()
                logger.fail("link", tail or "no output")
                failed += 1
                continue
            proc = bt.run([exe], tool, check=False, timeout=timeout, env=bt.child_env(hub))
            print(((proc.stdout or "") + (proc.stderr or "")).strip(), flush=True)
            failed += proc.returncode != 0
            continue
        if ":" in spec:
            area, stem = spec.split(":", 1)
        else:
            area, stem = "tools", spec
        source = root / "tests" / "unit" / area / f"{stem}.c"
        suite = f"{area}_{stem}"
        test_obj = compile_unit(compiler, flags, includes, system_dirs, source, obj_root, logger)
        tests = bt.extract_bt_tests(source.read_text(encoding="utf-8"))
        gen_dir = root / bt.dirs_cfg(hub)["gen"]
        gen_dir.mkdir(parents=True, exist_ok=True)
        main_source = gen_dir / f"{suite}_main.c"
        main_source.write_text(bt.generated_main_source(suite, tests), encoding="utf-8")
        main_obj = compile_unit(compiler, flags, includes, system_dirs, main_source, obj_root,
                                logger)
        exe = bin_dir / f"{suite}.exe"
        argv = bt.link_command(compiler, flags, [*objects, test_obj, main_obj], [], exe)
        proc = bt.run(argv, f"link {exe.name}", check=False)
        if proc.returncode != 0:
            tail = ((proc.stderr or "") + (proc.stdout or ""))[-3000:].strip()
            logger.fail("link", tail or "no output")
            failed += 1
            continue
        proc = bt.run([exe], suite, check=False, timeout=timeout, env=bt.child_env(hub))
        print(((proc.stdout or "") + (proc.stderr or "")).strip(), flush=True)
        if proc.returncode != 0:
            failed += 1
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
