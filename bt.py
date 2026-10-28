import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent
HUB_RELPATH = "config/config.json"
TARGET_PREFIX = "tgt_"
SOURCE_GLOBS = ("src/**/*.c", "tools/**/*.c", "tests/**/*.c")
MUTANT_AREAS = ("core", "scheme", "data", "puzzles", "ga", "app")
MIGRATIONS_GENERATOR = "scripts/gen_migrations.py"
EMBEDDED_MIGRATIONS = ("src/data/migrations_embedded.c", "src/data/migrations_embedded.h")
INCLUDE_DIRS = ("src", "tools", "tests")
SYSTEM_INCLUDE_DIRS = ("vendor/sqlite",)
VENDOR_UNITS = (("vendor/sqlite/sqlite3.c", ("-DSQLITE_THREADSERIALIZED=1",)),)
LLVM_MAJOR = "23"
REQUIRED_TOOLS = ("clang", "clang_format", "clang_tidy", "llvm_cov")
LINKER_FLAG = "-fuse-ld=lld-link"
FUZZ_HARNESS = """#include <stdint.h>
#include <stddef.h>
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    (void)data;
    (void)size;
    return 0;
}
"""


class BtError(RuntimeError):
    pass


class Logger:
    def line(self, tag, text):
        stamp = time.strftime("%Y-%m-%dT%H:%M:%S")
        print(f"{stamp} [{tag}] {text}", flush=True)

    def entry(self, unit, detail=""):
        self.line("entry", f"{unit} {detail}".rstrip())

    def exit(self, unit, code, seconds=None):
        detail = f"{unit} code={code}"
        if seconds is not None:
            detail += f" dt={seconds:.3f}s"
        self.line("exit", detail)

    def fail(self, unit, message):
        self.line("fail", f"{unit} {message}")


class UnitResult:
    def __init__(self, label, returncode, output):
        self.label = label
        self.returncode = returncode
        self.output = output


def load_hub():
    raw = os.environ.get("BT_CONFIG", "")
    path = Path(raw) if raw else REPO_ROOT / HUB_RELPATH
    if not path.is_absolute():
        path = REPO_ROOT / path
    try:
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    except (OSError, json.JSONDecodeError) as e:
        raise BtError(f"config hub unreadable at {path}: {e}") from e


def build_cfg(hub=None):
    return (hub or load_hub())["build"]


def dirs_cfg(hub=None):
    return build_cfg(hub)["dirs"]


def resolve_tool(name, hub=None):
    override = build_cfg(hub)["tools"].get(name, "")
    if override:
        path = Path(override)
        if not path.is_file():
            raise BtError(f"tool override for {name} missing: {path}")
        return path
    found = shutil.which(name.replace("_", "-"))
    if not found:
        raise BtError(f"tool not found on PATH: {name}")
    return Path(found)


def run(argv, label, cwd=None, check=True, env=None, timeout=None):
    unit_argv = [str(part) for part in argv]
    logger = Logger()
    logger.entry(label, subprocess.list2cmdline(unit_argv))
    started = time.monotonic()
    proc = subprocess.run(
        unit_argv,
        capture_output=True,
        text=True,
        cwd=None if cwd is None else str(cwd),
        env=env,
        timeout=timeout,
        check=False,
    )
    logger.exit(label, proc.returncode, time.monotonic() - started)
    if proc.returncode != 0 and check:
        tail = ((proc.stderr or "") + (proc.stdout or ""))[-3000:].strip()
        raise BtError(f"{label} failed with code {proc.returncode}: {tail}")
    return proc


def run_units(units, jobs=1, timeout=None, env=None):
    def execute(unit):
        label, argv = unit
        unit_argv = [str(part) for part in argv]
        try:
            proc = subprocess.run(
                unit_argv,
                capture_output=True,
                text=True,
                timeout=timeout,
                check=False,
                env=env,
            )
            output = ((proc.stdout or "") + (proc.stderr or "")).strip()
            return UnitResult(label, proc.returncode, output)
        except subprocess.TimeoutExpired as e:
            return UnitResult(label, 124, f"timeout: {e}")

    workers = max(1, jobs)
    with ThreadPoolExecutor(max_workers=workers) as pool:
        return list(pool.map(execute, units))


def jobs_of(hub=None):
    raw = build_cfg(hub).get("jobs", 0)
    return raw if raw and raw > 0 else (os.cpu_count() or 1)


def target_doc(fn):
    return (fn.__doc__ or "").strip().splitlines()[0] if fn.__doc__ else ""


def target_functions_from(fns):
    pairs = []
    for fn in fns:
        if not fn.__name__.startswith(TARGET_PREFIX):
            continue
        if not target_doc(fn):
            raise BtError(f"target {fn.__name__} lacks a docstring")
        name = fn.__name__[len(TARGET_PREFIX) :].replace("_", "-")
        pairs.append((name, fn))
    return pairs


def target_functions(module):
    fns = [value for value in vars(module).values() if callable(value)]
    return target_functions_from(fns)


def target_table(pairs):
    return [(name, target_doc(fn)) for name, fn in pairs]


def format_help(rows):
    lines = ["usage: python bt.py <target> [args]", "", "targets:"]
    for name, doc in rows:
        lines.append(f"{name:<24} {doc}")
    return "\n".join(lines)


def extract_bt_tests(text):
    pairs = []
    for match in re.finditer(r"BT_TEST\s*\(", text):
        tail = text[match.end() :]
        pair = re.match(
            r"\s*([A-Za-z_][A-Za-z0-9_]*)\s*,\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)", tail
        )
        if pair is None:
            raise BtError(f"malformed BT_TEST at offset {match.start()}: {tail[:40]!r}")
        pairs.append((pair.group(1), pair.group(2)))
    return sorted(pairs)


def discover_test_suites(root):
    unit_dir = root / "tests" / "unit"
    suites = []
    if not unit_dir.is_dir():
        return suites
    for source in sorted(unit_dir.rglob("*_test.c")):
        area_parts = source.relative_to(unit_dir).parts[:-1]
        stem = source.stem
        tests = extract_bt_tests(source.read_text(encoding="utf-8"))
        if not tests:
            continue
        suites.append(
            {
                "source": source,
                "area_parts": area_parts,
                "area": "_".join(area_parts),
                "stem": stem,
                "exe_name": "_".join([*area_parts, stem]),
                "tests": tests,
            })
    return suites


def filter_suites(suites, area, only=None):
    picked = [s for s in suites if s["area"] == area]
    if only is not None:
        picked = [s for s in picked if only in s["stem"]]
    return picked


def suite_exe_name(suite):
    return "_".join(part for part in (suite.get("area", ""), suite["stem"]) if part)


def generated_main_source(suite_name, tests):
    ordered = sorted(tests)
    lines = ['#include "common/bt_harness.h"', ""]
    for group, name in ordered:
        lines.append(f"extern void bt_test_{group}_{name}(void);")
    lines += ["", "int main(void) {", "    bt_test_entry tests[] = {"]
    for group, name in ordered:
        lines.append(f'        {{"{group}.{name}", bt_test_{group}_{name}}},')
    lines += [
        "    };",
        f'    return bt_harness_run("{suite_name}", tests, {len(ordered)});',
        "}",
        "",
    ]
    return "\n".join(lines)


def hello_source():
    return '#include <stdio.h>\n\nint main(void) {\n    return puts("bt build ok") < 0 ? 1 : 0;\n}\n'


def compile_flags(hub):
    cfg = hub["build"]
    return [
        "-target",
        cfg["target"],
        f"-std={cfg['std']}",
        *cfg["cflags"],
        *cfg["release_flags"],
    ]


def objects_dirname(hub):
    payload = "\n".join(compile_flags(hub))
    return hashlib.sha256(payload.encode("utf-8")).hexdigest()[:12]


def compile_command(compiler, flags, includes, source, obj, dep=None, extra_flags=(),
                    system_dirs=None):
    argv = [*compiler, *flags, *extra_flags]
    for include in includes:
        argv += ["-I", str(include)]
    for include in system_dirs if system_dirs is not None else SYSTEM_INCLUDE_DIRS:
        argv += ["-isystem", str(REPO_ROOT / include)]
    argv += ["-c", str(source), "-o", str(obj)]
    if dep is not None:
        argv += ["-MD", "-MF", str(dep)]
    return argv


def vendor_extra_flags(source):
    name = str(source).replace("\\", "/")
    for candidate, defines in VENDOR_UNITS:
        if name == candidate or name.endswith("/" + candidate):
            return (*defines, "-Wno-error")
    return ()


def link_command(compiler, flags, objects, libs, exe):
    argv = [*compiler, *flags, LINKER_FLAG]
    for flag in link_flags_of(load_hub()):
        argv += ["-Xlinker", flag]
    argv += [str(obj) for obj in objects]
    argv += [f"-l{lib}" for lib in libs]
    argv += ["-o", str(exe)]
    return argv


def link_flags_of(hub=None):
    return build_cfg(hub)["link_flags"]


def object_path(obj_root, repo_root, source):
    relative = Path(source).relative_to(repo_root)
    return obj_root / relative.with_suffix(".obj")


def aggregate_results(results):
    failed = [r for r in results if r.returncode != 0]
    line = f"{len(results) - len(failed)} passed, {len(failed)} failed"
    return (1 if failed else 0), line


def c_files(root):
    files = []
    for pattern in SOURCE_GLOBS:
        files.extend(root.glob(pattern))
    return sorted(set(files))


def prune_stale_obj_roots(obj_base, current, logger):
    if not obj_base.is_dir():
        return
    for child in obj_base.iterdir():
        if child.is_dir() and child != current:
            shutil.rmtree(child, ignore_errors=True)
            logger.line("clean", f"removed stale objects {child.name}")


def discover_compile_units(hub, root):
    includes = [root / d for d in INCLUDE_DIRS]
    obj_base = root / dirs_cfg(hub)["obj"]
    obj_root = obj_base / objects_dirname(hub)
    units = []
    for source in c_files(root):
        obj = object_path(obj_root, root, source)
        units.append((source, obj))
    for source, _ in VENDOR_UNITS:
        units.append((root / source, object_path(obj_root, root, root / source)))
    return includes, obj_root, units


def dep_prerequisites(dep):
    if dep is None or not dep.is_file():
        return None
    text = dep.read_text(encoding="utf-8", errors="replace").replace("\\\n", " ")
    tokens = text.split()
    if not tokens:
        return None
    return [Path(token.rstrip(":")) for token in tokens[1:]]


def needs_compile(source, obj, dep=None):
    if not obj.is_file():
        return True
    obj_time = obj.stat().st_mtime
    if obj_time <= source.stat().st_mtime:
        return True
    prerequisites = dep_prerequisites(dep)
    if prerequisites is None:
        return True
    for prerequisite in prerequisites:
        if prerequisite.is_file() and prerequisite.stat().st_mtime >= obj_time:
            return True
    return False


def patch_coff_timestamp(path):
    header = path.open("rb").read(8)
    if len(header) < 8 or header[0] != 0x64 or header[1] != 0x86:
        return False
    if header[4:8] == b"\x00\x00\x00\x00":
        return True
    with path.open("r+b") as handle:
        handle.seek(4)
        handle.write(b"\x00\x00\x00\x00")
    return True


def matched_tool_dir(stem, tool_dirs):
    best = None
    best_len = -1
    for tool_dir in tool_dirs:
        name = tool_dir.name.replace("-", "_")
        if stem == name or stem.startswith(name + "_"):
            if len(name) > best_len:
                best = tool_dir
                best_len = len(name)
    return best


def tools_needed_for_suites(root, suites):
    tool_root = root / "tools"
    if not tool_root.is_dir():
        return set()
    tool_dirs = [d for d in sorted(tool_root.glob("*")) if d.is_dir()]
    needed = set()
    for suite in suites:
        if suite["area"] != "tools":
            continue
        tool_dir = matched_tool_dir(suite["stem"], tool_dirs)
        if tool_dir is not None:
            needed.add(tool_dir.name)
    return needed


def tools_needed_scoped(root, suites, only=None):
    if only is not None:
        suites = [s for s in suites if only in s["stem"]]
    return tools_needed_for_suites(root, suites)


def test_unit_scope(args):
    if len(args) == 1 and not args[0].startswith("--"):
        return args[0], None
    if len(args) == 3 and args[1] == "--only" and args[2] and not args[0].startswith("--"):
        return args[0], args[2]
    return None


def src_area_closure(hub, areas):
    deps = build_cfg(hub).get("src_deps", {})
    seen = set()
    pending = list(areas)
    while pending:
        area = pending.pop()
        if area in seen:
            continue
        seen.add(area)
        pending.extend(deps.get(area, []))
    return seen


def tool_area_closure(hub, tools):
    deps = build_cfg(hub).get("tool_deps", {})
    areas = set()
    for tool in tools:
        areas.update(deps.get(tool, []))
    return src_area_closure(hub, areas)


def system_include_dirs(hub=None):
    dirs = list(SYSTEM_INCLUDE_DIRS)
    dirs.extend(build_cfg(hub).get("raylib_build", {}).get("include_dirs", []))
    return dirs


def raylib_link_map(hub, root):
    cfg = build_cfg(hub).get("raylib_build", {})
    tools = set(cfg.get("tools", []))
    if not tools:
        return {}
    lib = root / cfg["lib"]
    if not lib.is_file():
        raise BtError(f"raylib lib missing at {lib}: run python scripts/vendor.py")
    return {name: (lib, list(cfg.get("system_libs", []))) for name in tools}


def raylib_src_pair(hub, root, src_sources):
    areas = set(build_cfg(hub).get("raylib_build", {}).get("src_areas", []))
    if not areas:
        return None
    for source in src_sources:
        rel = str(source.relative_to(root)).replace("\\", "/")
        parts = rel.split("/")
        if len(parts) > 2 and parts[1] in areas:
            cfg = build_cfg(hub).get("raylib_build", {})
            lib = root / cfg["lib"]
            if not lib.is_file():
                raise BtError(f"raylib lib missing at {lib}: run python scripts/vendor.py")
            return (lib, list(cfg.get("system_libs", [])))
    return None


def app_exe_link(hub, root, bin_dir, objs_by_source, src_objects, vendor_objects, src_lib):
    name = build_cfg(hub).get("app_exe", "")
    if not name:
        return None
    app_main = objs_by_source.get(root / "src" / "app" / "main.c")
    if app_main is None:
        return None
    objects = [*src_objects, app_main, *vendor_objects]
    libs = ()
    if src_lib is not None:
        objects = [*objects, src_lib[0]]
        libs = tuple(src_lib[1])
    return (bin_dir / name, objects, libs)


def compile_stage(hub, root, compiler, logger, extra=(), test_area=None, only=None):
    includes, obj_root, units = discover_compile_units(hub, root)
    prune_stale_obj_roots(root / dirs_cfg(hub)["obj"], obj_root, logger)
    flags = compile_flags(hub)
    system_dirs = system_include_dirs(hub)
    tools_needed = (
        tools_needed_scoped(root, discover_test_suites(root), only)
        if test_area == "tools"
        else set()
    )
    if test_area == "tools":
        src_areas = tool_area_closure(hub, tools_needed)
    elif test_area is not None:
        src_areas = src_area_closure(hub, [test_area])
    else:
        src_areas = None
    pairs = []
    for source, _ in units:
        rel = str(source.relative_to(root)).replace("\\", "/")
        if test_area is not None:
            if rel.startswith("tools/"):
                parts = rel.split("/")
                if test_area != "tools" or len(parts) < 2 or parts[1] not in tools_needed:
                    continue
            if rel.startswith("tests/"):
                if rel.startswith("tests/unit/"):
                    area_parts = rel.split("/")
                    if len(area_parts) < 3 or area_parts[2] != test_area:
                        continue
                    if only is not None and only not in Path(rel).stem:
                        continue
                elif not (test_area == "e2e" and rel.startswith("tests/e2e/")):
                    continue
            elif rel.startswith("src/") and not rel.startswith("src/common/"):
                area_parts = rel.split("/")
                if src_areas is None or len(area_parts) < 2 or area_parts[1] not in src_areas:
                    continue
        pairs.append((source, object_path(obj_root, root, source)))
    pairs += [(Path(source), Path(obj)) for source, obj in extra]
    commands = []
    entries = []
    for source, obj in pairs:
        dep = obj.with_suffix(".d")
        obj.parent.mkdir(parents=True, exist_ok=True)
        rel = source.relative_to(root)
        if needs_compile(source, obj, dep):
            commands.append(
                (
                    f"compile {rel}",
                    compile_command(
                        compiler,
                        flags,
                        includes,
                        source,
                        obj,
                        dep,
                        vendor_extra_flags(source),
                        system_dirs,
                    ),
                )
            )
        else:
            logger.line("skip", f"compile {rel} up to date")
        entries.append((source, obj))
    if commands:
        results = run_units(commands, jobs=jobs_of(hub))
        failed = [r for r in results if r.returncode != 0]
        for result in failed:
            logger.fail(result.label, result.output[-2000:] or "no output")
        if failed:
            raise BtError(f"{len(failed)} of {len(commands)} compiles failed")
        patched = 0
        for _, obj in pairs:
            if obj.is_file() and patch_coff_timestamp(obj):
                patched += 1
        logger.line("repro", f"{patched} object timestamps zeroed")
        logger.line(
            "compile", f"{len(commands)} compiled, {len(commands) - len(failed)} ok"
        )
    return obj_root, entries


def generate_stage(hub, root):
    gen_dir = root / dirs_cfg(hub)["gen"]
    obj_root = root / dirs_cfg(hub)["obj"] / objects_dirname(hub)
    gen_dir.mkdir(parents=True, exist_ok=True)
    generated = []
    for suite in discover_test_suites(root):
        main_source = gen_dir / f"{suite['exe_name']}_main.c"
        main_source.write_text(
            generated_main_source(suite["exe_name"], suite["tests"]), encoding="utf-8"
        )
        generated.append((main_source, obj_root / "gen" / f"{suite['exe_name']}_main.obj"))
    hello_source_path = gen_dir / "hello.c"
    hello_source_path.write_text(hello_source(), encoding="utf-8")
    generated.append((hello_source_path, obj_root / "gen" / "hello.obj"))
    return generated


def link_stamp_path(hub, root):
    return root / dirs_cfg(hub)["gen"] / "link_stamp.txt"


def link_stamp_payload(hub):
    return "\n".join(link_flags_of(hub))


def link_stamp_changed(hub, root):
    stamp = link_stamp_path(hub, root)
    return not stamp.is_file() or stamp.read_text(
        encoding="utf-8"
    ) != link_stamp_payload(hub)


def write_link_stamp(hub, root):
    stamp = link_stamp_path(hub, root)
    stamp.parent.mkdir(parents=True, exist_ok=True)
    stamp.write_text(link_stamp_payload(hub), encoding="utf-8")


def link_stage(hub, root, compiler, logger, obj_root, entries, only=None):
    cfg = build_cfg(hub)
    bin_dir = root / dirs_cfg(hub)["bin"]
    flags = compile_flags(hub)
    objs_by_source = {source: obj for source, obj in entries}
    built_objects = {obj for _, obj in entries}
    src_sources = [
        source for source, _ in entries if source.is_relative_to(root / "src")
    ]
    src_objects = [
        obj
        for source, obj in entries
        if source.is_relative_to(root / "src") and source.name != "main.c"
    ]
    raylib = raylib_link_map(hub, root)
    src_lib = raylib_src_pair(hub, root, src_sources)
    links = []
    vendor_objects = [
        obj for source, obj in entries if source.is_relative_to(root / "vendor")
    ]
    tools_dir = root / "tools"
    tool_dirs = [d for d in sorted(tools_dir.glob("*")) if d.is_dir()]
    e2e_dir = root / "tests" / "e2e"
    e2e_objects = (
        [objs_by_source[s] for s in sorted(e2e_dir.glob("*.c")) if s in objs_by_source]
        if e2e_dir.is_dir()
        else []
    )
    if tools_dir.is_dir():
        for tool_dir in tool_dirs:
            tool_sources = sorted(tool_dir.glob("*.c"))
            if not tool_sources:
                continue
            if only is not None and only.replace("_", "-") not in tool_dir.name:
                continue
            tool_objs = [objs_by_source[s] for s in tool_sources if s in objs_by_source]
            if not tool_objs:
                continue
            extra_lib, libs = raylib.get(tool_dir.name, (None, ()))
            if src_lib is not None:
                extra_lib, libs = src_lib
            objects = [*src_objects, *tool_objs, *vendor_objects]
            if extra_lib is not None:
                objects.append(extra_lib)
            links.append(
                (
                    f"link {tool_dir.name}.exe",
                    (bin_dir / f"{tool_dir.name}.exe", objects, libs),
                ))
    if e2e_objects:
        extra_lib, libs = (None, ()) if src_lib is None else src_lib
        objects = [*src_objects, *e2e_objects, *vendor_objects]
        if extra_lib is not None:
            objects.append(extra_lib)
        links.append(
            (
                f"link {cfg['e2e_exe']}",
                (bin_dir / cfg["e2e_exe"], objects, libs),
            ))
    for suite in discover_test_suites(root):
        test_obj = objs_by_source.get(suite["source"])
        main_obj = obj_root / "gen" / f"{suite['exe_name']}_main.obj"
        if test_obj is None or main_obj not in built_objects:
            continue
        if only is not None and only not in suite["stem"]:
            continue
        suite_tool_objects = []
        extra_lib = None
        libs = ()
        if suite["area"] == "tools" and tool_dirs:
            tool_dir = matched_tool_dir(suite["stem"], tool_dirs)
            if tool_dir is not None:
                suite_tool_objects = [
                    objs_by_source[s]
                    for s in sorted(tool_dir.glob("*.c"))
                    if s.name != "main.c" and s in objs_by_source
                ]
                if tool_dir.name in raylib:
                    extra_lib, libs = raylib[tool_dir.name]
        suite_e2e_objects = []
        if suite["area"] == "e2e" and e2e_objects:
            suite_e2e_objects = [obj for obj in e2e_objects if "main.obj" not in obj.name]
        if src_lib is not None:
            extra_lib, libs = src_lib
        objects = [
            *src_objects,
            *suite_tool_objects,
            *suite_e2e_objects,
            test_obj,
            main_obj,
            *vendor_objects,
        ]
        if extra_lib is not None:
            objects.append(extra_lib)
        links.append(
            (
                f"link {suite['exe_name']}.exe",
                (bin_dir / f"{suite['exe_name']}.exe", objects, libs),
            ))
    app_link = app_exe_link(
        hub, root, bin_dir, objs_by_source, src_objects, vendor_objects, src_lib
    )
    if app_link is not None:
        links.append((f"link {app_link[0].name}", app_link))
    hello_exe = bin_dir / cfg["hello_exe"]
    hello_obj = obj_root / "gen" / "hello.obj"
    if hello_obj in built_objects:
        links.append((f"link {hello_exe.name}", (hello_exe, [hello_obj], ())))
    commands = []
    stamp_changed = link_stamp_changed(hub, root)
    for label, (exe, objects, libs) in links:
        if stamp_changed or needs_link(objects, exe):
            exe.parent.mkdir(parents=True, exist_ok=True)
            commands.append(
                (label, link_command(compiler, flags, objects, list(libs), exe))
            )
        else:
            logger.line("skip", f"{label} up to date")
    write_link_stamp(hub, root)
    if commands:
        results = run_units(commands, jobs=jobs_of(hub))
        failed = [r for r in results if r.returncode != 0]
        for result in failed:
            logger.fail(result.label, result.output[-2000:] or "no output")
        if failed:
            raise BtError(f"{len(failed)} of {len(commands)} links failed")
    return [exe for _, (exe, _, _) in links]


def needs_link(objects, exe):
    if not exe.is_file():
        return True
    exe_time = exe.stat().st_mtime
    for obj in objects:
        if not obj.is_file() or obj.stat().st_mtime >= exe_time:
            return True
    return False


def version_line(output):
    lines = output.splitlines()
    for line in lines:
        if "version" in line.lower():
            return line.strip()
    for line in lines:
        if line.strip():
            return line.strip()
    return ""


def probe_tool(name, hub=None):
    try:
        path = resolve_tool(name, hub)
    except BtError:
        return {"path": None, "version": None}
    try:
        proc = run([path, "--version"], f"probe {name}", check=False)
    except (OSError, subprocess.SubprocessError) as e:
        return {"path": str(path), "version": None, "error": str(e)}
    return {"path": str(path), "version": version_line(proc.stdout)}


def probe_fuzz(hub, root, compiler):
    gen_dir = root / dirs_cfg(hub)["gen"] / "fuzz-probe"
    gen_dir.mkdir(parents=True, exist_ok=True)
    harness = gen_dir / "harness.c"
    harness.write_text(FUZZ_HARNESS, encoding="utf-8")
    exe = gen_dir / "fuzz_probe.exe"
    flags = [*compile_flags(hub), "-fsanitize=fuzzer", str(harness), "-o", str(exe)]
    proc = run([*compiler, *flags], "probe fuzz compile", check=False)
    if proc.returncode != 0:
        tail = ((proc.stderr or "") + (proc.stdout or ""))[-400:].strip()
        return {"mode": "plainloop", "detail": f"compile failed: {tail}"}
    runproc = run([exe, "-runs=1"], "probe fuzz run", check=False, timeout=60)
    if runproc.returncode == 0:
        return {"mode": "libfuzzer", "detail": "runs=1 exit 0"}
    tail = ((runproc.stderr or "") + (runproc.stdout or ""))[-400:].strip()
    return {"mode": "plainloop", "detail": f"run exit {runproc.returncode}: {tail}"}


def write_env(hub, env):
    path = REPO_ROOT / dirs_cfg(hub)["env"]
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(".json.tmp")
    tmp.write_text(json.dumps(env, indent=2) + "\n", encoding="utf-8")
    tmp.replace(path)
    return path


def doctor_collect():
    hub = load_hub()
    env = {
        name: probe_tool(name, hub)
        for name in ("clang", "clang_format", "clang_tidy", "llvm_cov", "llvm_ar", "nvcc")
    }
    env["python"] = {"path": sys.executable, "version": platform.python_version()}
    clang = env["clang"]
    if clang["path"] is None:
        env["fuzz"] = {"mode": "plainloop", "detail": "clang missing"}
        return env, "clang not found on PATH or in the build tools overrides"
    version = clang["version"] or ""
    if not version.startswith(f"clang version {LLVM_MAJOR}."):
        return env, f"clang is not LLVM {LLVM_MAJOR}.x: {version or 'no version'}"
    env["fuzz"] = probe_fuzz(hub, REPO_ROOT, [clang["path"]])
    return env, None


def tgt_help(args):
    """show this help"""
    pairs = target_functions(sys.modules[__name__])
    print(format_help(target_table(pairs)))
    return 0


def tgt_doctor(args):
    """probe the toolchain and write build/env.json"""
    logger = Logger()
    logger.entry("doctor")
    env, defect = doctor_collect()
    path = write_env(load_hub(), env)
    logger.line("write", f"{path}")
    for name in ("clang", "clang_format", "clang_tidy", "llvm_cov", "llvm_ar", "nvcc"):
        entry = env[name]
        detail = entry["version"] or "missing"
        logger.line("probe", f"{name} {detail}")
    logger.line("probe", f"fuzz mode {env['fuzz']['mode']} ({env['fuzz']['detail']})")
    if defect:
        logger.fail("doctor", defect)
        return 1
    logger.exit("doctor", 0)
    return 0


def tgt_setup(args):
    """verify the required toolchain and report optional probes"""
    logger = Logger()
    logger.entry("setup")
    env, defect = doctor_collect()
    write_env(load_hub(), env)
    missing = [name for name in REQUIRED_TOOLS if env[name]["path"] is None]
    if defect:
        logger.fail("setup", defect)
    for name in missing:
        logger.fail("setup", f"required tool missing: {name}")
    if defect or missing:
        return 1
    for name in REQUIRED_TOOLS:
        logger.line("pass", f"{name} {env[name]['version']}")
    nvcc = env["nvcc"]
    if nvcc["path"]:
        logger.line("pass", f"nvcc {nvcc['version']}")
    else:
        logger.line("info", "nvcc missing, cuda lane disabled")
    logger.line("info", f"fuzz mode {env['fuzz']['mode']}")
    logger.exit("setup", 0)
    return 0


def tgt_build(args):
    """compile and link every c unit, test exe, and the hello smoke exe"""
    hub = load_hub()
    compiler = [resolve_tool("clang", hub)]
    logger = Logger()
    logger.entry("build")
    try:
        verify_migrations_fresh(REPO_ROOT, logger)
        generated = generate_stage(hub, REPO_ROOT)
        obj_root, entries = compile_stage(
            hub, REPO_ROOT, compiler, logger, extra=generated
        )
        exes = link_stage(hub, REPO_ROOT, compiler, logger, obj_root, entries)
        for exe in exes:
            logger.line("artifact", f"{exe} size={exe.stat().st_size}")
        logger.exit("build", 0)
        return 0
    except (BtError, OSError, KeyError, ValueError) as e:
        logger.fail("build", f"{type(e).__name__}: {e}")
        return 1


def tgt_check(args):
    """compile every translation unit without linking"""
    hub = load_hub()
    compiler = [resolve_tool("clang", hub)]
    logger = Logger()
    logger.entry("check")
    try:
        verify_migrations_fresh(REPO_ROOT, logger)
        compile_stage(hub, REPO_ROOT, compiler, logger)
        logger.exit("check", 0)
        return 0
    except (BtError, OSError, KeyError, ValueError) as e:
        logger.fail("check", f"{type(e).__name__}: {e}")
        return 1


def tgt_fmt(args):
    """clang-format dry run over every c and h file"""
    return fmt_run(write=False)


def tgt_fmt_fix(args):
    """clang-format in place"""
    return fmt_run(write=True)


def fmt_run(write):
    hub = load_hub()
    tool = resolve_tool("clang_format", hub)
    logger = Logger()
    files = [f for f in c_files(REPO_ROOT)] + sorted(
        p for pattern in ("src/**/*.h", "tools/**/*.h", "tests/**/*.h")
        for p in REPO_ROOT.glob(pattern)
    )
    if not files:
        logger.line("fmt", "no files")
        return 0
    mode = ["-i"] if write else ["--dry-run", "--Werror"]
    label = "fmt" if not write else "fmt-fix"
    for at in range(0, len(files), 64):
        argv = [tool, *mode, *[str(f) for f in files[at : at + 64]]]
        proc = run(argv, label, check=False)
        if proc.returncode != 0:
            tail = ((proc.stderr or "") + (proc.stdout or ""))[-3000:].strip()
            Logger().fail(label, tail or "no output")
            return proc.returncode
    return 0


def in_lint_area(file, area):
    rel = str(file.relative_to(REPO_ROOT)).replace("\\", "/")
    if rel.startswith("src/common/") or rel.startswith(f"src/{area}/"):
        return True
    if rel.startswith(f"tests/unit/{area}/"):
        return True
    if rel.startswith(f"tools/{area}/"):
        return True
    return False


def tgt_lint(args):
    """clang-tidy over every c file, or one area: python bt.py lint <area>"""
    hub = load_hub()
    tool = resolve_tool("clang_tidy", hub)
    flags = compile_flags(hub)
    includes = [REPO_ROOT / d for d in INCLUDE_DIRS]
    system_dirs = system_include_dirs(hub)
    files = c_files(REPO_ROOT)
    area = args[0] if args else None
    if area is not None:
        files = [f for f in files if in_lint_area(f, area)]
    if not files:
        return 0
    units = []
    for source in files:
        argv = [
            tool,
            "--config-file=.clang-tidy",
            str(source),
            "--",
            *flags,
            *[part for include in includes for part in ("-I", str(include))],
            *[part for include in system_dirs
              for part in ("-isystem", str(REPO_ROOT / include))],
        ]
        units.append((f"tidy {source.relative_to(REPO_ROOT)}", argv))
    results = run_units(units, jobs=jobs_of(hub))
    failed = [r for r in results if r.returncode != 0]
    for result in failed:
        Logger().fail(result.label, result.output[-2000:] or "no output")
    Logger().line("lint", f"{len(results) - len(failed)} clean, {len(failed)} failed")
    return 1 if failed else 0


def child_env(hub):
    paths = hub.get("paths", {})
    env = os.environ.copy()
    env["BT_REF_GROUND"] = paths.get("ref_ground", "ref_ground")
    env["BT_CONFIG"] = str(REPO_ROOT / HUB_RELPATH)
    env["BT_ARTIFACTS"] = paths.get("artifacts", "artifacts")
    return env


def fuzz_suites(root):
    return [s for s in discover_test_suites(root) if s["stem"].startswith("fuzz_")]


def verify_suites(root):
    return [s for s in discover_test_suites(root) if s["stem"].startswith("verify_")]


def mutants_plan(root, areas):
    plans = []
    for area in areas:
        if area not in MUTANT_AREAS:
            raise BtError(f"unknown mutants area: {area}")
        sources = sorted(root.glob(f"src/{area}/**/*.c"))
        suites = [
            s
            for s in discover_test_suites(root)
            if s["area"] == area and not s["stem"].startswith("fuzz_")
        ]
        if not sources:
            raise BtError(f"no c sources under src/{area}")
        if not suites:
            raise BtError(f"no unit suites under tests/unit/{area}")
        rels = [str(s.relative_to(root)).replace("\\", "/") for s in sources]
        for suite in suites:
            name = suite["exe_name"]
            plans.append(
                {
                    "area": area,
                    "suite": name,
                    "sources": rels,
                    "ledger": f"artifacts/mutants/ledger-{name}.tsv",
                    "outcomes": f"artifacts/mutants/outcomes-{name}.json",
                }
            )
    return plans


def run_suite_exes(hub, suites):
    bin_dir = REPO_ROOT / dirs_cfg(hub)["bin"]
    timeout = build_cfg(hub)["test_timeout_secs"]
    units = [
        (suite["exe_name"], [bin_dir / f"{suite['exe_name']}.exe"]) for suite in suites
    ]
    results = run_units(units, jobs=jobs_of(hub), timeout=timeout, env=child_env(hub))
    for result in results:
        if result.returncode != 0:
            print(f"[fail] {result.label}", flush=True)
            print(result.output[-3000:], flush=True)
    return results


def run_python_suite(hub):
    suite_dir = REPO_ROOT / "tests" / "python"
    if not suite_dir.is_dir() or not list(suite_dir.glob("test_*.py")):
        return []
    python = build_cfg(hub)["tools"].get("python", "")
    argv = [python or sys.executable, "-m", "unittest", "discover", "-s", "tests/python"]
    proc = run(
        argv,
        "python suite",
        check=False,
        cwd=REPO_ROOT,
        timeout=600,
        env=child_env(hub),
    )
    output = ((proc.stdout or "") + (proc.stderr or "")).strip()
    if proc.returncode != 0:
        print(output[-3000:], flush=True)
    return [UnitResult("python-suite", proc.returncode, output)]


def tgt_test(args):
    """run every unit test exe and the python suite"""
    hub = load_hub()
    logger = Logger()
    logger.entry("test")
    try:
        build_suites(hub, logger)
    except (BtError, OSError, KeyError, ValueError) as e:
        logger.fail("test-build", f"{type(e).__name__}: {e}")
        return 1
    results = run_suite_exes(hub, discover_test_suites(REPO_ROOT))
    results += run_python_suite(hub)
    code, line = aggregate_results(results)
    logger.line("summary", line)
    logger.exit("test", code)
    return code


def tgt_test_unit(args):
    """run one area's unit tests: python bt.py test-unit <area> [--only substring]"""
    if not args:
        print("usage: python bt.py test-unit <area> [--only substring]")
        return 2
    scope = test_unit_scope(args)
    if scope is None:
        print("usage: python bt.py test-unit <area> [--only substring]")
        return 2
    area, only = scope
    hub = load_hub()
    logger = Logger()
    logger.entry("test-unit", area if only is None else f"{area} --only {only}")
    try:
        compiler = [resolve_tool("clang", hub)]
        generated = generate_stage(hub, REPO_ROOT)
        obj_root, entries = compile_stage(
            hub, REPO_ROOT, compiler, logger, extra=generated, test_area=area, only=only
        )
        link_stage(hub, REPO_ROOT, compiler, logger, obj_root, entries, only=only)
    except (BtError, OSError, KeyError, ValueError) as e:
        logger.fail("test-build", f"{type(e).__name__}: {e}")
        return 1
    suites = filter_suites(discover_test_suites(REPO_ROOT), area, only)
    if not suites:
        logger.fail("test-unit", f"no test suites under tests/unit/{area}")
        return 1
    results = run_suite_exes(hub, suites)
    code, line = aggregate_results(results)
    logger.line("summary", line)
    logger.exit("test-unit", code)
    return code


def migrations_stale(root):
    with tempfile.TemporaryDirectory() as tmp:
        probe = Path(tmp)
        (probe / "scripts").mkdir(parents=True)
        shutil.copyfile(root / MIGRATIONS_GENERATOR, probe / MIGRATIONS_GENERATOR)
        shutil.copyfile(root / ".clang-format", probe / ".clang-format")
        shutil.copytree(root / "db" / "migrations", probe / "db" / "migrations")
        (probe / "src" / "data").mkdir(parents=True)
        run([sys.executable, "-I", probe / MIGRATIONS_GENERATOR], "gen_migrations probe")
        fmt_tool = resolve_tool("clang_format", load_hub())
        generated = [probe / rel for rel in EMBEDDED_MIGRATIONS]
        run([fmt_tool, "-i", "--style=file", *generated], "gen_migrations format")
        for rel in EMBEDDED_MIGRATIONS:
            committed = root / rel
            if not committed.is_file() or committed.read_bytes() != (probe / rel).read_bytes():
                return rel
    return None


def verify_migrations_fresh(root, logger):
    stale = migrations_stale(root)
    if stale is not None:
        raise BtError(
            f"{stale} is stale: run python {MIGRATIONS_GENERATOR}, python bt.py fmt-fix,"
            " and commit"
        )
    logger.line("check", "embedded migrations fresh")


def build_suites(hub, logger):
    compiler = [resolve_tool("clang", hub)]
    generated = generate_stage(hub, REPO_ROOT)
    obj_root, entries = compile_stage(hub, REPO_ROOT, compiler, logger, extra=generated)
    link_stage(hub, REPO_ROOT, compiler, logger, obj_root, entries)


def ensure_tool_exes(hub, logger, build_label, names):
    build_suites(hub, logger)
    bin_dir = REPO_ROOT / dirs_cfg(hub)["bin"]
    exes = []
    for name in names:
        exe = bin_dir / name
        if not exe.is_file():
            raise BtError(f"{build_label}: {name} missing after build: {exe}")
        exes.append(exe)
    return exes


def run_policy_exe(exe, argv, label, logger, env=None):
    try:
        proc = run([exe, *argv], label, check=False, timeout=300, env=env)
    except subprocess.TimeoutExpired as e:
        logger.fail(label, f"timeout after {e.timeout}s: {e.cmd}")
        logger.exit(label, 2)
        return 2
    if proc.returncode != 0:
        tail = ((proc.stderr or "") + (proc.stdout or ""))[-3000:].strip()
        if tail:
            print(tail, flush=True)
    logger.exit(label, proc.returncode)
    return proc.returncode


def tool_run(hub, build_label, exe_name, argv, label, env=None):
    logger = Logger()
    logger.entry(label)
    try:
        exe = ensure_tool_exes(hub, logger, build_label, (exe_name,))[0]
    except (BtError, OSError, KeyError, ValueError) as e:
        logger.fail(build_label, f"{type(e).__name__}: {e}")
        return 1
    return run_policy_exe(exe, argv, label, logger, env=env)


def tgt_policy(args):
    """selftest then the zero-comment, constants-hub, and sloc scans"""
    hub = load_hub()
    workdir = str(REPO_ROOT / "artifacts" / "policy-selftest")
    code = tool_run(hub, "policy-build", "policy.exe", ["selftest", workdir], "policy-selftest")
    if code != 0:
        return code
    return tool_run(hub, "policy-build", "policy.exe", [], "policy-scan")


def tgt_policy_selftest(args):
    """prove bt-policy detects planted comments, literals, and cap breaches"""
    workdir = REPO_ROOT / "artifacts" / "policy-selftest"
    return tool_run(
        load_hub(),
        "policy-selftest-build",
        "policy.exe",
        ["selftest", str(workdir)],
        "policy-selftest",
    )


def tgt_fuzz(args):
    """run every fuzz_*_test suite exe in plainloop mode, harness bodies keep the early-return, exit-at-bound applies only under a libFuzzer driver"""
    hub = load_hub()
    logger = Logger()
    logger.entry("fuzz")
    try:
        build_suites(hub, logger)
    except (BtError, OSError, KeyError, ValueError) as e:
        logger.fail("fuzz-build", f"{type(e).__name__}: {e}")
        return 1
    results = run_suite_exes(hub, fuzz_suites(REPO_ROOT))
    code, line = aggregate_results(results)
    logger.line("summary", line)
    logger.exit("fuzz", code)
    return code


def tgt_mutants(args):
    """mutate area sources against their unit suites: python bt.py mutants [area...]"""
    hub = load_hub()
    logger = Logger()
    logger.entry("mutants", " ".join(args) or "all")
    try:
        areas = args or [a for a in MUTANT_AREAS if (REPO_ROOT / "src" / a).is_dir()]
        mutate, gate = ensure_tool_exes(
            hub, logger, "mutants-build", ("mutate.exe", "mutants-gate.exe")
        )
        plans = mutants_plan(REPO_ROOT, areas)
        env = child_env(hub)
        for plan in plans:
            run(
                [mutate, "run", "--ledger", plan["ledger"], "--exe", plan["suite"],
                 *plan["sources"]],
                f"mutate {plan['suite']}",
                env=env,
            )
            run(
                [mutate, "report", "--ledger", plan["ledger"], "--out", plan["outcomes"]],
                f"report {plan['suite']}",
                env=env,
            )
        merged = REPO_ROOT / "artifacts/mutants/outcomes.json"
        inputs = [REPO_ROOT / plan["outcomes"] for plan in plans]
        if len(inputs) > 1:
            run([gate, "merge", merged, *inputs], "merge outcomes", env=env)
        else:
            shutil.copyfile(inputs[0], merged)
            logger.line("merge", f"single ledger copied to {merged}")
        logger.exit("mutants", 0)
        return 0
    except (BtError, OSError, KeyError, ValueError) as e:
        logger.fail("mutants", f"{type(e).__name__}: {e}")
        return 1


def tgt_mutants_gate(args):
    """adjudicate mutation survivors in artifacts/mutants/outcomes.json against the allowlist"""
    return tool_run(
        load_hub(),
        "mutants-gate-build",
        "mutants-gate.exe",
        [str(REPO_ROOT / "artifacts/mutants/outcomes.json"), str(REPO_ROOT / "config/allowlist.tsv")],
        "mutants-gate",
    )


def tgt_mutants_gate_selftest(args):
    """the mutants gate selftest over planted outcomes"""
    return tool_run(
        load_hub(), "mutants-gate-selftest-build", "mutants-gate.exe", ["selftest"],
        "mutants-gate-selftest",
    )


def tgt_visual_diff(args):
    """diff e2e shots against the committed baselines"""
    hub = load_hub()
    shots = REPO_ROOT / "artifacts" / "e2e"
    bases = REPO_ROOT / hub.get("paths", {}).get("baselines", "tests/visual/baselines")
    return tool_run(
        hub, "visual-diff-build", "visual-diff.exe", [shots, bases], "visual-diff"
    )


def tgt_visual_selftest(args):
    """prove the diff passes identical and catches drift"""
    workdir = REPO_ROOT / "artifacts" / "visual-selftest"
    return tool_run(
        load_hub(),
        "visual-diff-build",
        "visual-diff.exe",
        ["--selftest", workdir],
        "visual-selftest",
    )


def tgt_gen_data(args):
    """build data/vendor from ref_ground and synthesize uncovered pairs"""
    return tool_run(load_hub(), "gendata-build", "gendata.exe", [], "gen-data")


def tgt_memofreq(args):
    """memo pair frequency monte carlo"""
    return tool_run(load_hub(), "memofreq-build", "memofreq.exe", [], "memofreq")


def tgt_ga(args):
    """run the syllable optimizer into data/ga/"""
    return tool_run(load_hub(), "ga-run-build", "ga-run.exe", [], "ga")


def tgt_verify_algs(args):
    """verify every vendor alg and pack row, write the verify and packs reports"""
    hub = load_hub()
    return tool_run(hub, "verify-build", "verify.exe", [], "verify-algs", env=child_env(hub))


def tgt_verify_algs_selftest(args):
    """run every verify_* unit suite with the ref ground and config env"""
    hub = load_hub()
    logger = Logger()
    logger.entry("verify-algs-selftest")
    try:
        build_suites(hub, logger)
    except (BtError, OSError, KeyError, ValueError) as e:
        logger.fail("verify-algs-selftest-build", f"{type(e).__name__}: {e}")
        return 1
    results = run_suite_exes(hub, verify_suites(REPO_ROOT))
    code, line = aggregate_results(results)
    logger.line("summary", line)
    logger.exit("verify-algs-selftest", code)
    return code


def _screenshots_steps(hub):
    artifacts = REPO_ROOT / "artifacts"
    usage_user = artifacts / "usage-user"
    absent = artifacts / "usage-absent-config.json"
    shots = REPO_ROOT / "docs" / "screenshots"
    base = child_env(hub)
    tour_env = dict(base)
    tour_env["APPDATA"] = str(usage_user)
    fatal_env = dict(base)
    fatal_env["BT_CONFIG"] = str(absent)
    return [
        (["--usage", str(shots)], tour_env, usage_user),
        (["--fatal-shot", str(shots)], fatal_env, None),
    ]


def tgt_screenshots(args):
    """regenerate every usage shot and the fatal screen"""
    hub = load_hub()
    logger = Logger()
    logger.entry("screenshots")
    code = tgt_build([])
    if code != 0:
        return code
    shots = REPO_ROOT / "docs" / "screenshots"
    if shots.is_dir():
        for png in shots.glob("*.png"):
            png.unlink()
    shots.mkdir(parents=True, exist_ok=True)
    exe = REPO_ROOT / dirs_cfg(hub)["bin"] / build_cfg(hub).get("app_exe", "bt-app.exe")
    code = 0
    for argv, env, wipe in _screenshots_steps(hub):
        if wipe is not None and wipe.is_dir():
            shutil.rmtree(wipe)
        step = run_policy_exe(exe, argv, "screenshots", logger, env=env)
        code = code or step
    logger.exit("screenshots", code)
    return code


def build_scope(hub, logger, area, only=None):
    compiler = [resolve_tool("clang", hub)]
    generated = generate_stage(hub, REPO_ROOT)
    obj_root, entries = compile_stage(
        hub, REPO_ROOT, compiler, logger, extra=generated, test_area=area, only=only
    )
    link_stage(hub, REPO_ROOT, compiler, logger, obj_root, entries, only=only)


def tgt_e2e_smoke(args):
    """sandboxed smoke tour over the release exe"""
    hub = load_hub()
    logger = Logger()
    logger.entry("e2e-smoke")
    try:
        build_scope(hub, logger, "app")
        build_scope(hub, logger, "e2e")
    except (BtError, OSError, KeyError, ValueError) as e:
        logger.fail("e2e-smoke", f"{type(e).__name__}: {e}")
        return 1
    bin_dir = REPO_ROOT / dirs_cfg(hub)["bin"]
    out_dir = REPO_ROOT / "artifacts" / "e2e"
    user_dir = REPO_ROOT / "artifacts" / "e2e-user"
    for path in (out_dir, user_dir):
        path.mkdir(parents=True, exist_ok=True)
    env = {**os.environ, "APPDATA": str(user_dir)}
    driver = bin_dir / build_cfg(hub)["e2e_exe"]
    app = bin_dir / build_cfg(hub).get("app_exe", "bt-app.exe")
    code = run_policy_exe(driver, [str(app), str(out_dir)], "e2e-smoke", logger, env=env)
    logger.exit("e2e-smoke", code)
    return code


def tgt_e2e(args):
    """headless flow suite then the smoke tour"""
    hub = load_hub()
    logger = Logger()
    logger.entry("e2e")
    try:
        build_scope(hub, logger, "app", only="e2e")
        build_scope(hub, logger, "tools", only="jobwrap")
    except (BtError, OSError, KeyError, ValueError) as e:
        logger.fail("e2e", f"{type(e).__name__}: {e}")
        return 1
    bin_dir = REPO_ROOT / dirs_cfg(hub)["bin"]
    jobwrap = bin_dir / "jobwrap.exe"
    suite = bin_dir / "app_e2e_test.exe"
    for exe in (jobwrap, suite):
        if not exe.is_file():
            logger.fail("e2e", f"missing exe after build: {exe}")
            return 1
    code = run_policy_exe(jobwrap, ["--anchor", "e2e", "--", str(suite)], "e2e", logger)
    if code != 0:
        logger.exit("e2e", code)
        return code
    logger.exit("e2e", 0)
    return tgt_e2e_smoke(args)


def tgt_clean(args):
    """remove artifacts and recreate the keep file"""
    logger = Logger()
    artifacts = REPO_ROOT / "artifacts"
    if artifacts.is_dir():
        shutil.rmtree(artifacts)
    artifacts.mkdir(parents=True, exist_ok=True)
    keep = artifacts / ".gitkeep"
    if not keep.is_file():
        keep.write_text("", encoding="utf-8")
    logger.exit("clean", 0)
    return 0


def tgt_clean_c(args):
    """remove the c build tree"""
    hub = load_hub()
    logger = Logger()
    build_dir = REPO_ROOT / dirs_cfg(hub)["root"]
    if build_dir.is_dir():
        shutil.rmtree(build_dir)
    logger.exit("clean-c", 0)
    return 0


def main(argv=None):
    args = list(sys.argv[1:] if argv is None else argv)
    if not args:
        print(format_help(target_table(target_functions(sys.modules[__name__]))))
        return 2
    name = args[0].replace("_", "-")
    pairs = dict(target_functions(sys.modules[__name__]))
    if name not in pairs:
        print(f"unknown target: {name}")
        print(format_help(target_table(target_functions(sys.modules[__name__]))))
        return 2
    return pairs[name](args[1:])


if __name__ == "__main__":
    sys.exit(main())
