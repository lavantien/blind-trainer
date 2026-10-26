import json
import os
import platform
import re
import shutil
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent
HUB_RELPATH = "config/config.json"
TARGET_PREFIX = "tgt_"
SOURCE_GLOBS = ("src/**/*.c", "tools/**/*.c", "tests/**/*.c")
INCLUDE_DIRS = ("src", "tools", "tests")
LLVM_MAJOR = "23"
REQUIRED_TOOLS = ("clang", "clang_format", "clang_tidy", "llvm_cov")
LINKER_FLAG = "-fuse-ld=lld-link"
FUZZ_HARNESS = (
    "#include <stdint.h>\n"
    "#include <stddef.h>\n"
    "int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {\n"
    "    (void)data;\n"
    "    (void)size;\n"
    "    return 0;\n"
    "}\n"
)


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


def run_units(units, jobs=1, timeout=None):
    def execute(unit):
        label, argv = unit
        unit_argv = [str(part) for part in argv]
        try:
            proc = subprocess.run(
                unit_argv, capture_output=True, text=True, timeout=timeout, check=False
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
            }
        )
    return suites


def suite_exe_name(suite):
    return "_".join(part for part in (suite.get("area", ""), suite["stem"]) if part)


def generated_main_source(suite_name, tests):
    ordered = sorted(tests)
    lines = ['#include "common/bt_harness.h"', ""]
    for group, name in ordered:
        lines.append(f"extern void bt_test_{group}_{name}(void);")
    lines += [
        "",
        "int main(void) {",
        "    bt_test_entry tests[] = {",
    ]
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


def compile_command(compiler, flags, includes, source, obj, dep=None):
    argv = [*compiler, *flags]
    for include in includes:
        argv += ["-I", str(include)]
    argv += ["-c", str(source), "-o", str(obj)]
    if dep is not None:
        argv += ["-MD", "-MF", str(dep)]
    return argv


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


def discover_compile_units(hub, root):
    includes = [root / d for d in INCLUDE_DIRS]
    obj_root = root / dirs_cfg(hub)["obj"]
    units = []
    for source in c_files(root):
        obj = object_path(obj_root, root, source)
        units.append((source, obj))
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


def compile_stage(hub, root, compiler, logger, extra=()):
    includes, obj_root, units = discover_compile_units(hub, root)
    flags = compile_flags(hub)
    pairs = [(source, object_path(obj_root, root, source)) for source, _ in units]
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
                    compile_command(compiler, flags, includes, source, obj, dep),
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
        logger.line(
            "compile", f"{len(commands)} compiled, {len(commands) - len(failed)} ok"
        )
    return obj_root, entries


def generate_stage(hub, root):
    gen_dir = root / dirs_cfg(hub)["gen"]
    obj_root = root / dirs_cfg(hub)["obj"]
    gen_dir.mkdir(parents=True, exist_ok=True)
    generated = []
    for suite in discover_test_suites(root):
        main_source = gen_dir / f"{suite['exe_name']}_main.c"
        main_source.write_text(
            generated_main_source(suite["exe_name"], suite["tests"]), encoding="utf-8"
        )
        generated.append(
            (main_source, obj_root / "gen" / f"{suite['exe_name']}_main.obj")
        )
    hello_source_path = gen_dir / "hello.c"
    hello_source_path.write_text(hello_source(), encoding="utf-8")
    generated.append((hello_source_path, obj_root / "gen" / "hello.obj"))
    return generated


def link_stage(hub, root, compiler, logger, obj_root, entries):
    cfg = build_cfg(hub)
    bin_dir = root / dirs_cfg(hub)["bin"]
    flags = compile_flags(hub)
    objs_by_source = {source: obj for source, obj in entries}
    built_objects = {obj for _, obj in entries}
    src_objects = [
        obj for source, obj in entries if source.is_relative_to(root / "src")
    ]
    links = []
    tools_dir = root / "tools"
    if tools_dir.is_dir():
        for tool_dir in sorted(tools_dir.glob("*")):
            tool_sources = sorted(tool_dir.glob("*.c"))
            if not tool_sources:
                continue
            tool_objs = [objs_by_source[s] for s in tool_sources if s in objs_by_source]
            links.append(
                (
                    f"link {tool_dir.name}.exe",
                    (bin_dir / f"{tool_dir.name}.exe", [*src_objects, *tool_objs], ()),
                )
            )
    for suite in discover_test_suites(root):
        test_obj = objs_by_source.get(suite["source"])
        main_obj = obj_root / "gen" / f"{suite['exe_name']}_main.obj"
        if test_obj is None or main_obj not in built_objects:
            continue
        links.append(
            (
                f"link {suite['exe_name']}.exe",
                (
                    bin_dir / f"{suite['exe_name']}.exe",
                    [*src_objects, test_obj, main_obj],
                    (),
                ),
            )
        )
    hello_exe = bin_dir / cfg["hello_exe"]
    hello_obj = obj_root / "gen" / "hello.obj"
    if hello_obj in built_objects:
        links.append((f"link {hello_exe.name}", (hello_exe, [hello_obj], ())))
    commands = []
    for label, (exe, objects, libs) in links:
        if needs_link(objects, exe):
            exe.parent.mkdir(parents=True, exist_ok=True)
            commands.append(
                (label, link_command(compiler, flags, objects, list(libs), exe))
            )
        else:
            logger.line("skip", f"{label} up to date")
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
    for line in output.splitlines():
        if "version" in line.lower():
            return line.strip()
    for line in output.splitlines():
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
    flags = [
        *compile_flags(hub),
        "-fsanitize=fuzzer",
        str(harness),
        "-o",
        str(exe),
    ]
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
        "clang": probe_tool("clang", hub),
        "clang_format": probe_tool("clang_format", hub),
        "clang_tidy": probe_tool("clang_tidy", hub),
        "llvm_cov": probe_tool("llvm_cov", hub),
        "llvm_ar": probe_tool("llvm_ar", hub),
        "nvcc": probe_tool("nvcc", hub),
        "python": {"path": sys.executable, "version": platform.python_version()},
    }
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
        p
        for pattern in ("src/**/*.h", "tools/**/*.h", "tests/**/*.h")
        for p in REPO_ROOT.glob(pattern)
    )
    if not files:
        logger.line("fmt", "no files")
        return 0
    argv = [
        tool,
        "--dry-run",
        "--Werror" if not write else "-i",
        *[str(f) for f in files],
    ]
    label = "fmt" if not write else "fmt-fix"
    proc = run(argv, label, check=False)
    if proc.returncode != 0:
        tail = ((proc.stderr or "") + (proc.stdout or ""))[-3000:].strip()
        Logger().fail(label, tail or "no output")
    return proc.returncode


def tgt_lint(args):
    """clang-tidy over every c file"""
    hub = load_hub()
    tool = resolve_tool("clang_tidy", hub)
    tidy_cfg = hub["build"]["tidy"]
    flags = compile_flags(hub)
    includes = [REPO_ROOT / d for d in INCLUDE_DIRS]
    files = c_files(REPO_ROOT)
    if not files:
        return 0
    units = []
    for source in files:
        argv = [
            tool,
            f"--checks={tidy_cfg['checks']}",
            f"--warnings-as-errors={tidy_cfg['warnings_as_errors']}",
            str(source),
            "--",
            *flags,
            *[part for include in includes for part in ("-I", str(include))],
        ]
        units.append((f"tidy {source.relative_to(REPO_ROOT)}", argv))
    results = run_units(units, jobs=jobs_of(hub))
    failed = [r for r in results if r.returncode != 0]
    for result in failed:
        Logger().fail(result.label, result.output[-2000:] or "no output")
    Logger().line("lint", f"{len(results) - len(failed)} clean, {len(failed)} failed")
    return 1 if failed else 0


def run_suite_exes(hub, suites):
    bin_dir = REPO_ROOT / dirs_cfg(hub)["bin"]
    timeout = build_cfg(hub)["test_timeout_secs"]
    units = [
        (suite["exe_name"], [bin_dir / f"{suite['exe_name']}.exe"]) for suite in suites
    ]
    results = run_units(units, jobs=jobs_of(hub), timeout=timeout)
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
    argv = [
        python or sys.executable,
        "-m",
        "unittest",
        "discover",
        "-s",
        "tests/python",
    ]
    proc = run(argv, "python suite", check=False, cwd=REPO_ROOT, timeout=600)
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
        compiler = [resolve_tool("clang", hub)]
        generated = generate_stage(hub, REPO_ROOT)
        obj_root, entries = compile_stage(
            hub, REPO_ROOT, compiler, logger, extra=generated
        )
        link_stage(hub, REPO_ROOT, compiler, logger, obj_root, entries)
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
    """run one area's unit tests: python bt.py test-unit <area>"""
    if not args:
        print("usage: python bt.py test-unit <area>")
        return 2
    area = args[0]
    hub = load_hub()
    logger = Logger()
    logger.entry("test-unit", area)
    try:
        compiler = [resolve_tool("clang", hub)]
        generated = generate_stage(hub, REPO_ROOT)
        obj_root, entries = compile_stage(
            hub, REPO_ROOT, compiler, logger, extra=generated
        )
        link_stage(hub, REPO_ROOT, compiler, logger, obj_root, entries)
    except (BtError, OSError, KeyError, ValueError) as e:
        logger.fail("test-build", f"{type(e).__name__}: {e}")
        return 1
    suites = [s for s in discover_test_suites(REPO_ROOT) if s["area"] == area]
    if not suites:
        logger.fail("test-unit", f"no test suites under tests/unit/{area}")
        return 1
    results = run_suite_exes(hub, suites)
    code, line = aggregate_results(results)
    logger.line("summary", line)
    logger.exit("test-unit", code)
    return code


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
