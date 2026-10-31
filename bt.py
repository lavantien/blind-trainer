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
MUTANT_AREAS = ("core", "scheme", "data", "puzzles", "ga", "common")
MIGRATIONS_GENERATOR = "scripts/gen_migrations.py"
EMBEDDED_MIGRATIONS = ("src/data/migrations_embedded.c", "src/data/migrations_embedded.h")
INCLUDE_DIRS = ("src", "tools", "tests")
SYSTEM_INCLUDE_DIRS = ("vendor/sqlite",)
VENDOR_UNITS = (("vendor/sqlite/sqlite3.c", ("-DSQLITE_THREADSERIALIZED=1",)),)
LLVM_MAJOR = "23"
REQUIRED_TOOLS = ("clang", "clang_format", "clang_tidy", "llvm_cov")
LINKER_FLAG = "-fuse-ld=lld-link"
RELEASE_SCENARIOS = 4
RELEASE_OVERHEAD_SECS = 60
FUZZ_HARNESS = """#include <stdint.h>
#include <stddef.h>
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    (void)data;
    (void)size;
    return 0;
}
"""
CUDA_DEFINE = "-DBT_GA_EVAL_CUDA"
CUDA_IEEE_FLAGS = ("--fmad=false", "-ftz=false", "--prec-div=true", "--prec-sqrt=true")
CUDA_UNIT = "src/ga/cuda/ga_eval.cu"
CUDA_LIB_NAME = "cudart_static"
GA_PARITY_SCRIPT = Path("scripts/ga_parity.py")
GA_PARITY_BUILD_SECS = 900
CUDA_SMOKE_SOURCE = """#include <cuda_runtime.h>

__global__ void bt_cuda_smoke(float *dst) {
    dst[0] = 0.25f + 0.25f;
}

int main(void) {
    float *dev = NULL;
    float host = 0.0f;
    if (cudaMalloc(&dev, sizeof host) != cudaSuccess) {
        return 2;
    }
    bt_cuda_smoke<<<1, 1>>>(dev);
    if (cudaGetLastError() != cudaSuccess) {
        return 3;
    }
    if (cudaDeviceSynchronize() != cudaSuccess) {
        return 4;
    }
    if (cudaMemcpy(&host, dev, sizeof host, cudaMemcpyDeviceToHost) != cudaSuccess) {
        return 5;
    }
    cudaFree(dev);
    return host == 0.5f ? 0 : 6;
}
"""
VSWHERE_RELPATH = Path("Microsoft Visual Studio") / "Installer" / "vswhere.exe"


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


def cuda_cfg(hub=None):
    return (hub or load_hub()).get("cuda", {})


def cuda_compile_flags(hub, cuda=False):
    flags = compile_flags(hub)
    return [*flags, CUDA_DEFINE] if cuda else flags


def cuda_bin_dir(hub=None):
    return REPO_ROOT / cuda_cfg(hub).get("bin_dir", "build/bin-cuda")


def objects_dirname(hub, cuda=False):
    payload = "\n".join(cuda_compile_flags(hub, cuda=cuda))
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


def nvcc_argv_of(nvcc):
    if isinstance(nvcc, (str, Path)):
        return [str(nvcc)]
    return [str(part) for part in nvcc]


def nvcc_base_flags(ccbin, arch):
    return ["-ccbin", str(ccbin), f"-arch={arch}", *CUDA_IEEE_FLAGS, "-O3", "-cudart=static"]


def nvcc_compile_command(nvcc, ccbin, arch, source, obj, includes=()):
    argv = [*nvcc_argv_of(nvcc), *nvcc_base_flags(ccbin, arch)]
    for include in includes:
        argv += ["-I", str(include)]
    argv += ["-c", str(source), "-o", str(obj)]
    return argv


def nvcc_smoke_command(nvcc, ccbin, arch, source, exe):
    return [*nvcc_argv_of(nvcc), *nvcc_base_flags(ccbin, arch), str(source), "-o", str(exe)]


def gpu_compute_cap(smi_argv):
    try:
        proc = run(
            [*smi_argv, "--query-gpu=compute_cap", "--format=csv,noheader"],
            "probe gpu cap",
            check=False,
        )
    except (OSError, subprocess.SubprocessError):
        return None
    if proc.returncode != 0:
        return None
    lines = (proc.stdout or "").strip().splitlines()
    if not lines:
        return None
    cap = lines[0].strip()
    return cap if re.fullmatch(r"\d+\.\d+", cap) else None


def sm_arch(cap):
    major, _, minor = str(cap).partition(".")
    return f"sm_{int(major)}{int(minor)}"


def cuda_pinned_cap(hub):
    raw = cuda_cfg(hub).get("arch", "")
    if raw is None or isinstance(raw, (bool, dict, list)):
        raise BtError(f"cuda.arch must be a compute capability like 8.6, got: {raw!r}")
    pinned = str(raw).strip()
    if pinned and not re.fullmatch(r"\d+(\.\d+)?", pinned):
        raise BtError(f"cuda.arch must be a compute capability like 8.6, got: {pinned}")
    return pinned


def cuda_arch(hub, cap):
    return sm_arch(cuda_pinned_cap(hub) or cap)


def cuda_lib_dir(nvcc):
    return Path(nvcc).resolve().parent.parent / "lib" / "x64"


def cuda_plan_payload(cuda):
    parts = [str(cuda.get(key)) for key in ("nvcc", "ccbin", "arch", "lib_dir")]
    return "\n".join([*parts, *CUDA_IEEE_FLAGS, CUDA_LIB_NAME, CUDA_DEFINE])


def vswhere_installs():
    vswhere = Path(os.environ.get("ProgramFiles(x86)", "")) / VSWHERE_RELPATH
    if not vswhere.is_file():
        return []
    try:
        proc = run(
            [vswhere, "-all", "-products", "*", "-property", "installationPath"],
            "probe vswhere",
            check=False,
        )
    except (OSError, subprocess.SubprocessError):
        return []
    if proc.returncode != 0:
        return []
    return sorted(
        Path(line.strip()) for line in (proc.stdout or "").splitlines() if line.strip()
    )


def cl_ccbin_candidates(hub):
    candidates = []
    override = build_cfg(hub)["tools"].get("cl", "")
    if override:
        candidates.append(Path(override))
    on_path = shutil.which("cl")
    if on_path:
        candidates.append(Path(on_path).parent)
    for install in vswhere_installs():
        toolroots = install.glob("VC/Tools/MSVC/*/bin/Hostx64/x64")
        candidates.extend(sorted(toolroots, reverse=True))
    seen = set()
    unique = []
    for candidate in candidates:
        key = str(candidate).lower()
        if key not in seen:
            seen.add(key)
            unique.append(candidate)
    return unique


def link_command(compiler, flags, objects, libs, exe, lib_dirs=()):
    argv = [*compiler, *flags, LINKER_FLAG]
    for flag in link_flags_of(load_hub()):
        argv += ["-Xlinker", flag]
    argv += [str(obj) for obj in objects]
    for lib_dir in lib_dirs:
        argv += ["-L", str(lib_dir)]
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


def prune_stale_obj_roots(obj_base, current, logger, keep=()):
    if not obj_base.is_dir():
        return
    for child in obj_base.iterdir():
        if child.is_dir() and child != current and child.name not in keep:
            shutil.rmtree(child, ignore_errors=True)
            logger.line("clean", f"removed stale objects {child.name}")


def discover_compile_units(hub, root, cuda=False):
    includes = [root / d for d in INCLUDE_DIRS]
    obj_base = root / dirs_cfg(hub)["obj"]
    obj_root = obj_base / objects_dirname(hub, cuda=cuda)
    if not cuda:
        note_plain_obj_root(hub, root, obj_root)
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


def needs_compile_plain(source, obj, extra_inputs=()):
    if not obj.is_file():
        return True
    obj_time = obj.stat().st_mtime
    for path in (source, *extra_inputs):
        if path.is_file() and path.stat().st_mtime >= obj_time:
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


def compile_stage(hub, root, compiler, logger, extra=(), test_area=None, only=None, cuda=None):
    includes, obj_root, units = discover_compile_units(hub, root, cuda=cuda is not None)
    prune_stale_obj_roots(
        root / dirs_cfg(hub)["obj"],
        obj_root,
        logger,
        keep={objects_dirname(hub, cuda=cuda is None)},
    )
    flags = cuda_compile_flags(hub, cuda=cuda is not None)
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
    cu_stamp = None
    cu_payload = ""
    if cuda is not None:
        cu_source = root / CUDA_UNIT
        cu_obj = object_path(obj_root, root, cu_source)
        cu_obj.parent.mkdir(parents=True, exist_ok=True)
        cu_stamp = root / dirs_cfg(hub)["gen"] / "cu_stamp_cuda.txt"
        cu_payload = cuda_plan_payload(cuda)
        stamp_fresh = cu_stamp.is_file() and cu_stamp.read_text(encoding="utf-8") == cu_payload
        if not stamp_fresh or needs_compile_plain(cu_source, cu_obj, [cu_source.with_suffix(".h")]):
            commands.append(
                (
                    f"compile {CUDA_UNIT}",
                    nvcc_compile_command(cuda["nvcc"], cuda["ccbin"], cuda["arch"],
                                         cu_source, cu_obj, [root / "src"]),
                )
            )
        else:
            logger.line("skip", f"compile {CUDA_UNIT} up to date")
        entries.append((cu_source, cu_obj))
    if commands:
        results = run_units(commands, jobs=jobs_of(hub))
        failed = [r for r in results if r.returncode != 0]
        for result in failed:
            logger.fail(result.label, result.output[-2000:] or "no output")
        if failed:
            raise BtError(f"{len(failed)} of {len(commands)} compiles failed")
        if cu_stamp is not None:
            cu_stamp.parent.mkdir(parents=True, exist_ok=True)
            cu_stamp.write_text(cu_payload, encoding="utf-8")
        patched = 0
        for _, obj in entries:
            if obj.is_file() and patch_coff_timestamp(obj):
                patched += 1
        logger.line("repro", f"{patched} object timestamps zeroed")
        logger.line(
            "compile", f"{len(commands)} compiled, {len(commands) - len(failed)} ok"
        )
    return obj_root, entries


def generate_stage(hub, root, cuda=False):
    gen_dir = root / dirs_cfg(hub)["gen"]
    obj_root = root / dirs_cfg(hub)["obj"] / objects_dirname(hub, cuda=cuda)
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


def link_stamp_path(hub, root, cuda=False):
    name = "link_stamp_cuda.txt" if cuda else "link_stamp.txt"
    return root / dirs_cfg(hub)["gen"] / name


def link_stamp_payload(hub, cuda=None):
    payload = [*link_flags_of(hub), "cuda" if cuda is not None else "plain"]
    if cuda is not None:
        payload.append(cuda_plan_payload(cuda))
    return "\n".join(payload)


def link_stamp_changed(hub, root, cuda=None):
    stamp = link_stamp_path(hub, root, cuda is not None)
    return not stamp.is_file() or stamp.read_text(
        encoding="utf-8"
    ) != link_stamp_payload(hub, cuda)


def write_link_stamp(hub, root, cuda=None):
    stamp = link_stamp_path(hub, root, cuda is not None)
    stamp.parent.mkdir(parents=True, exist_ok=True)
    stamp.write_text(link_stamp_payload(hub, cuda), encoding="utf-8")


def link_stage(hub, root, compiler, logger, obj_root, entries, only=None, cuda=None):
    cfg = build_cfg(hub)
    bin_dir = cuda_bin_dir(hub) if cuda is not None else root / dirs_cfg(hub)["bin"]
    flags = cuda_compile_flags(hub, cuda=cuda is not None)
    lib_dirs = [cuda["lib_dir"]] if cuda is not None else []
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
    stamp_changed = link_stamp_changed(hub, root, cuda)
    for label, (exe, objects, libs) in links:
        if stamp_changed or needs_link(objects, exe):
            exe.parent.mkdir(parents=True, exist_ok=True)
            all_libs = [*libs, CUDA_LIB_NAME] if cuda is not None else list(libs)
            commands.append(
                (
                    label,
                    link_command(compiler, flags, objects, all_libs, exe, lib_dirs=lib_dirs),
                )
            )
        else:
            logger.line("skip", f"{label} up to date")
    write_link_stamp(hub, root, cuda)
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


def cached_env(hub=None):
    path = REPO_ROOT / dirs_cfg(hub or load_hub())["env"]
    try:
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    except (OSError, json.JSONDecodeError):
        return {}


def cache_cuda_verdict(hub, verdict):
    env = cached_env(hub)
    env["cuda"] = verdict
    return write_env(hub, env)


def note_plain_obj_root(hub, root, obj_root):
    if root != REPO_ROOT:
        return
    try:
        rel = obj_root.relative_to(root).as_posix()
    except ValueError:
        return
    env = cached_env(hub)
    if env.get("obj_root") != rel:
        env["obj_root"] = rel
        write_env(hub, env)


def cuda_probe_base(hub):
    try:
        nvcc = resolve_tool("nvcc", hub)
    except BtError:
        nvcc = None
    return {
        "nvcc": str(nvcc) if nvcc else None,
        "ccbin": None,
        "arch": None,
        "lib_dir": str(cuda_lib_dir(nvcc)) if nvcc else None,
    }


def probe_cuda(hub, root):
    override = str(cuda_cfg(hub).get("doctor_override", "")).strip().lower()
    if override in ("pass", "fail"):
        return {**cuda_probe_base(hub), "ok": override == "pass",
                "detail": f"doctor override {override}"}
    base = cuda_probe_base(hub)
    if base["nvcc"] is None:
        return {**base, "ok": False, "detail": "nvcc missing"}
    cap = None
    smi = shutil.which("nvidia-smi")
    if smi:
        cap = gpu_compute_cap([smi])
    pinned = cuda_pinned_cap(hub)
    arch = sm_arch(pinned or cap) if (pinned or cap) else None
    if arch is None:
        return {**base, "ok": False,
                "detail": "no gpu compute capability from nvidia-smi and no cuda.arch pin"}
    gen_dir = root / dirs_cfg(hub)["gen"] / "cuda-probe"
    gen_dir.mkdir(parents=True, exist_ok=True)
    smoke = gen_dir / "smoke.cu"
    smoke.write_text(CUDA_SMOKE_SOURCE, encoding="utf-8")
    exe = gen_dir / "smoke.exe"
    for ccbin in cl_ccbin_candidates(hub):
        proc = run(
            nvcc_smoke_command(base["nvcc"], ccbin, arch, smoke, exe),
            "probe cuda compile",
            check=False,
        )
        if proc.returncode != 0 or not exe.is_file():
            continue
        try:
            runproc = run([exe], "probe cuda run", check=False, timeout=60)
        except (OSError, subprocess.SubprocessError) as e:
            return {**base, "ok": False, "detail": f"smoke run failed to start: {e}"}
        if runproc.returncode == 0:
            return {**base, "ccbin": str(ccbin), "arch": arch, "ok": True,
                    "detail": f"smoke exit 0 on {arch} with {ccbin}"}
    return {**base, "ok": False,
            "detail": "no host compiler produced a running cuda smoke"}


def cuda_verdict_usable(verdict):
    if not isinstance(verdict, dict) or not verdict.get("ok"):
        return False
    nvcc = verdict.get("nvcc")
    ccbin = verdict.get("ccbin")
    lib_dir = verdict.get("lib_dir")
    if not all((nvcc, ccbin, lib_dir, verdict.get("arch"))):
        return False
    return Path(nvcc).is_file() and Path(ccbin).is_dir() and Path(lib_dir).is_dir()


def cuda_toolchain(hub):
    override = str(cuda_cfg(hub).get("doctor_override", "")).strip().lower()
    if override == "fail":
        raise BtError("cuda toolchain unavailable: doctor override fail")
    if override == "pass":
        verdict = probe_cuda(hub, REPO_ROOT)
        cache_cuda_verdict(hub, verdict)
    else:
        cached = cached_env(hub).get("cuda")
        if cuda_verdict_usable(cached):
            verdict = dict(cached)
        else:
            verdict = probe_cuda(hub, REPO_ROOT)
            cache_cuda_verdict(hub, verdict)
    if not verdict.get("ok"):
        raise BtError(
            f"cuda toolchain unavailable: {verdict.get('detail')}; run python bt.py doctor"
        )
    pinned = cuda_pinned_cap(hub)
    if pinned:
        verdict["arch"] = sm_arch(pinned)
    missing = [k for k in ("nvcc", "ccbin", "arch", "lib_dir") if not verdict.get(k)]
    if missing:
        raise BtError(f"cuda verdict incomplete, missing {', '.join(missing)}")
    return verdict


def doctor_collect():
    hub = load_hub()
    env = {
        name: probe_tool(name, hub)
        for name in ("clang", "clang_format", "clang_tidy", "llvm_cov", "llvm_ar", "nvcc")
    }
    env["python"] = {"path": sys.executable, "version": platform.python_version()}
    env["cuda"] = probe_cuda(hub, REPO_ROOT)
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
    cuda = env["cuda"]
    logger.line("probe", f"cuda {'ok' if cuda['ok'] else 'unavailable'}: {cuda['detail']}")
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
    cuda = env["cuda"]
    if cuda["ok"]:
        logger.line("pass", f"cuda {cuda['detail']}")
    else:
        logger.line("info", f"cuda unavailable: {cuda['detail']}")
    logger.line("info", f"fuzz mode {env['fuzz']['mode']}")
    logger.exit("setup", 0)
    return 0


def tgt_build(args):
    """compile and link every c unit, test exe, and the hello smoke exe"""
    hub = load_hub()
    compiler = [resolve_tool("clang", hub)]
    logger = Logger()
    logger.entry("build" if "--cuda" not in args else "build --cuda")
    cuda = None
    try:
        if "--cuda" in args:
            cuda = cuda_toolchain(hub)
            logger.line("cuda", f"{cuda['arch']} via {cuda['nvcc']} ccbin {cuda['ccbin']}")
        verify_migrations_fresh(REPO_ROOT, logger)
        generated = generate_stage(hub, REPO_ROOT, cuda=cuda is not None)
        obj_root, entries = compile_stage(
            hub, REPO_ROOT, compiler, logger, extra=generated, cuda=cuda
        )
        exes = link_stage(hub, REPO_ROOT, compiler, logger, obj_root, entries, cuda=cuda)
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
    if area == "e2e" and rel.startswith("tests/e2e/"):
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


def mutants_union_argv(plans, root):
    argv = ["union-report"]
    for plan in plans:
        argv += ["--ledger", str(root / plan["ledger"])]
    argv += ["--out", str(root / "artifacts/mutants/outcomes.json")]
    return argv


def run_mutants_plans(mutate_argv, plans, env, root):
    for plan in plans:
        run(
            [
                *mutate_argv,
                "run",
                "--ledger",
                str(root / plan["ledger"]),
                "--exe",
                plan["suite"],
                *plan["sources"],
            ],
            f"mutate {plan['suite']}",
            env=env,
        )
    run([*mutate_argv, *mutants_union_argv(plans, root)], "union outcomes", env=env)


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


def kill_process_tree(pid):
    subprocess.run(["taskkill", "/PID", str(pid), "/T", "/F"], capture_output=True)


def run_policy_exe(exe, argv, label, logger, env=None, echo=False, timeout=300):
    unit_argv = [str(part) for part in [exe, *argv]]
    logger.entry(label, subprocess.list2cmdline(unit_argv))
    started = time.monotonic()
    proc = subprocess.Popen(
        unit_argv,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        env=env,
    )
    try:
        out, err = proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        kill_process_tree(proc.pid)
        try:
            proc.kill()
        except OSError:
            pass
        proc.communicate()
        logger.fail(label, f"timeout after {timeout}s: {unit_argv}")
        logger.exit(label, 2)
        return 2
    code = proc.returncode
    output = ((err or "") + (out or ""))
    if code != 0:
        tail = output[-3000:].strip()
        if tail:
            print(tail, flush=True)
    elif echo and out:
        print(out, flush=True)
    logger.exit(label, code, time.monotonic() - started)
    return code


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
        mutate = ensure_tool_exes(hub, logger, "mutants-build", ("mutate.exe",))[0]
        plans = mutants_plan(REPO_ROOT, areas)
        run_mutants_plans([mutate], plans, child_env(hub), REPO_ROOT)
        logger.line(
            "union",
            f"{len(plans)} suite ledgers unioned into artifacts/mutants/outcomes.json",
        )
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


def visual_pair_dirs(hub):
    shots = REPO_ROOT / "artifacts" / "e2e"
    bases = REPO_ROOT / hub.get("paths", {}).get("baselines", "tests/visual/baselines")
    return shots, bases


def visual_report_path(hub):
    artifacts = hub.get("paths", {}).get("artifacts", "artifacts")
    return REPO_ROOT / artifacts / "visual" / "report.md"


def visual_retry_outcome(rerender_code, second_code):
    if rerender_code != 0:
        return 4, "infra error, re-render failed"
    if second_code == 0:
        return 3, "infra flake, second run passed"
    if second_code != 1:
        return 4, f"infra error, second diff exited {second_code}"
    return 2, "regression, both runs mismatched"


def visual_diff_gate(code):
    if code == 0:
        return 0, None
    if code == 1:
        return 1, None
    return 4, f"infra error, diff exited {code}"


def visual_append_verdict(report_path, verdict):
    report_path.parent.mkdir(parents=True, exist_ok=True)
    with open(report_path, "a", encoding="utf-8") as handle:
        handle.write(f"\nverdict: {verdict}\n")


def png_hashes(directory):
    if not directory.is_dir():
        return {}
    return {
        png.name: hashlib.sha256(png.read_bytes()).hexdigest()
        for png in sorted(directory.glob("*.png"))
    }


def capture_diffs(first, second):
    drift = []
    for name in sorted(set(first) | set(second)):
        if name not in first:
            drift.append(f"{name} missing from first capture")
        elif name not in second:
            drift.append(f"{name} missing from second capture")
        elif first[name] != second[name]:
            drift.append(f"{name} hash differs between captures")
    return drift


def expected_shot_names(hub):
    return [f"{slug}.png" for slug in hub.get("e2e", {}).get("shots", [])]


def shot_set_drift(expected, found):
    drift = []
    if not expected:
        drift.append("config e2e.shots lists no shots")
    if not found:
        drift.append("no pngs found")
    for name in sorted(set(expected) - set(found)):
        drift.append(f"{name} missing from the set")
    for name in sorted(set(found) - set(expected)):
        drift.append(f"{name} unexpected")
    return drift


def reset_e2e_sandbox(user_dir):
    if user_dir.is_dir():
        shutil.rmtree(user_dir)
    return user_dir


def _ensure_visual_diff(hub, logger):
    build_scope(hub, logger, "tools", only="visual_diff")
    exe = REPO_ROOT / dirs_cfg(hub)["bin"] / "visual-diff.exe"
    if not exe.is_file():
        raise BtError(f"visual-diff.exe missing after build: {exe}")
    return exe


def tgt_visual_diff(args):
    """diff e2e shots against the committed baselines"""
    hub = load_hub()
    shots, bases = visual_pair_dirs(hub)
    return tool_run(
        hub, "visual-diff-build", "visual-diff.exe", [shots, bases], "visual-diff",
        env=child_env(hub)
    )


def tgt_visual_selftest(args):
    """prove the diff passes identical and catches drift"""
    hub = load_hub()
    workdir = REPO_ROOT / "artifacts" / "visual-selftest"
    return tool_run(
        hub,
        "visual-diff-build",
        "visual-diff.exe",
        ["--selftest", workdir],
        "visual-selftest",
        env=child_env(hub),
    )


def tgt_visual(args):
    """visual selftest then the diff with one re-render retry"""
    hub = load_hub()
    logger = Logger()
    logger.entry("visual")
    try:
        exe = _ensure_visual_diff(hub, logger)
        shots, bases = visual_pair_dirs(hub)
        report = visual_report_path(hub)
        workdir = REPO_ROOT / "artifacts" / "visual-selftest"
        drift = shot_set_drift(
            expected_shot_names(hub), set(png_hashes(shots)) | set(png_hashes(bases))
        )
        if drift:
            for line in drift:
                logger.fail("visual", line)
            logger.exit("visual", 4)
            return 4
    except (BtError, OSError, KeyError, ValueError) as e:
        logger.fail("visual", f"{type(e).__name__}: {e}")
        return 1
    try:
        code = run_policy_exe(
            exe, ["--selftest", str(workdir)], "visual-selftest", logger,
            env=child_env(hub)
        )
        if code != 0:
            logger.exit("visual", code)
            return code
        code = run_policy_exe(
            exe, [str(shots), str(bases)], "visual-diff", logger, echo=True,
            env=child_env(hub)
        )
        gate, verdict = visual_diff_gate(code)
        if gate != 1:
            if verdict is not None:
                logger.fail("visual", verdict)
                visual_append_verdict(report, verdict)
            logger.exit("visual", gate)
            return gate
        print("visual: mismatch, re-rendering and re-diffing once", flush=True)
        _, _, _, user_dir, _ = _e2e_smoke_plan(hub)
        reset_e2e_sandbox(user_dir)
        rerender = tgt_e2e_smoke(args)
        second = (
            run_policy_exe(
                exe, [str(shots), str(bases)], "visual-diff", logger, echo=True,
                env=child_env(hub)
            )
            if rerender == 0
            else None
        )
        code, verdict = visual_retry_outcome(rerender, second)
        visual_append_verdict(report, verdict)
        logger.exit("visual", code)
        return code
    except OSError as e:
        verdict = f"infra error, spawn failed: {e}"
        logger.fail("visual", verdict)
        visual_append_verdict(report, verdict)
        logger.exit("visual", 4)
        return 4


def tgt_baselines(args):
    """recapture the committed baselines from two byte-identical smoke captures"""
    hub = load_hub()
    logger = Logger()
    logger.entry("baselines")
    try:
        user_dir = REPO_ROOT / "artifacts" / "e2e-user"
        reset_e2e_sandbox(user_dir)
        build_scope(hub, logger, "app")
        build_scope(hub, logger, "e2e")
        driver, app, out_dir, user_dir, env = _e2e_smoke_plan(hub)
        for path in (out_dir, user_dir):
            path.mkdir(parents=True, exist_ok=True)
        exe = _ensure_visual_diff(hub, logger)
        _, bases = visual_pair_dirs(hub)
    except (BtError, OSError, KeyError, ValueError) as e:
        logger.fail("baselines", f"{type(e).__name__}: {e}")
        return 1
    captures = []
    for pass_at in (1, 2):
        label = f"baselines-capture-{pass_at}"
        code = run_policy_exe(
            driver, [str(app), str(out_dir)], label, logger, env=env, echo=True
        )
        if code != 0:
            logger.exit("baselines", code)
            return code
        captures.append(png_hashes(out_dir))
        logger.line("hash", f"capture {pass_at} hashed {len(captures[-1])} pngs")
    drift = capture_diffs(captures[0], captures[1])
    drift += shot_set_drift(expected_shot_names(hub), set(captures[1]))
    if drift:
        for line in drift:
            logger.fail("baselines", line)
        logger.exit("baselines", 1)
        return 1
    code = run_policy_exe(
        exe, ["--update", str(out_dir), str(bases)], "baselines-update", logger,
        echo=True, env=child_env(hub)
    )
    logger.exit("baselines", code)
    return code


def tgt_gen_data(args):
    """build data/vendor from ref_ground and synthesize uncovered pairs"""
    return tool_run(load_hub(), "gendata-build", "gendata.exe", [], "gen-data")


def tgt_memofreq(args):
    """memo pair frequency monte carlo"""
    hub = load_hub()
    return tool_run(hub, "memofreq-build", "memofreq.exe", [], "memofreq",
                    env=child_env(hub))


def tgt_ga(args):
    """run the syllable optimizer into data/ga/"""
    hub = load_hub()
    return tool_run(hub, "ga-run-build", "ga-run.exe", [], "ga", env=child_env(hub))


def parity_timeout_secs(hub):
    raw = cuda_cfg(hub).get("parity_timeout_secs", 3600)
    if isinstance(raw, bool) or not isinstance(raw, int) or raw <= 0:
        raise BtError(f"cuda.parity_timeout_secs must be a positive integer, got: {raw!r}")
    return raw


def parity_composed_timeout(hub):
    suite_timeout = int(build_cfg(hub)["test_timeout_secs"])
    suites = len(cuda_cfg(hub).get("parity_suites", []))
    return 2 * GA_PARITY_BUILD_SECS + 2 * 4 * suite_timeout + suites * suite_timeout


def tgt_ga_parity(args):
    """run the cpu vs cuda parity comparison"""
    hub = load_hub()
    logger = Logger()
    try:
        timeout = max(parity_timeout_secs(hub), parity_composed_timeout(hub))
    except (BtError, KeyError, ValueError, TypeError) as e:
        logger.fail("ga-parity", f"{type(e).__name__}: {e}")
        return 1
    try:
        proc = run(
            [sys.executable, "-I", REPO_ROOT / GA_PARITY_SCRIPT],
            "ga-parity",
            check=False,
            cwd=REPO_ROOT,
            timeout=timeout,
            env=child_env(hub),
        )
    except subprocess.TimeoutExpired as e:
        logger.fail("ga-parity", f"timeout after {e.timeout}s, the next run wipes the scratch lanes")
        return 1
    output = ((proc.stdout or "") + (proc.stderr or "")).strip()
    if output:
        print(output[-3000:], flush=True)
    return proc.returncode


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


def _e2e_smoke_plan(hub):
    bin_dir = REPO_ROOT / dirs_cfg(hub)["bin"]
    out_dir = REPO_ROOT / "artifacts" / "e2e"
    user_dir = REPO_ROOT / "artifacts" / "e2e-user"
    env = {**child_env(hub), "APPDATA": str(user_dir)}
    driver = bin_dir / build_cfg(hub)["e2e_exe"]
    app = bin_dir / build_cfg(hub).get("app_exe", "bt-app.exe")
    return driver, app, out_dir, user_dir, env


def tgt_e2e_smoke(args):
    """sandboxed smoke tour over the release exe"""
    hub = load_hub()
    logger = Logger()
    logger.entry("e2e-smoke")
    try:
        build_scope(hub, logger, "app")
        build_scope(hub, logger, "e2e")
        driver, app, out_dir, user_dir, env = _e2e_smoke_plan(hub)
        for path in (out_dir, user_dir):
            path.mkdir(parents=True, exist_ok=True)
    except (BtError, OSError, KeyError, ValueError) as e:
        logger.fail("e2e-smoke", f"{type(e).__name__}: {e}")
        return 1
    return run_policy_exe(driver, [str(app), str(out_dir)], "e2e-smoke", logger, env=env,
                          echo=True)


def tgt_e2e(args):
    """headless flow suite then the smoke tour"""
    hub = load_hub()
    logger = Logger()
    logger.entry("e2e")
    try:
        build_scope(hub, logger, "app", only="e2e")
        build_scope(hub, logger, "tools", only="jobwrap")
        bin_dir = REPO_ROOT / dirs_cfg(hub)["bin"]
        jobwrap = bin_dir / "jobwrap.exe"
        suite = bin_dir / "app_e2e_test.exe"
    except (BtError, OSError, KeyError, ValueError) as e:
        logger.fail("e2e", f"{type(e).__name__}: {e}")
        return 1
    for exe in (jobwrap, suite):
        if not exe.is_file():
            logger.fail("e2e", f"missing exe after build: {exe}")
            return 1
    code = run_policy_exe(jobwrap, ["--anchor", "e2e", "--", str(suite)], "e2e", logger,
                          echo=True)
    if code != 0:
        return code
    return tgt_e2e_smoke(args)


def release_version_arg(args):
    return args[0][2:] if args and args[0].startswith("v=") else ""


def release_dist_paths(hub):
    artifacts = REPO_ROOT / hub.get("paths", {}).get("artifacts", "artifacts")
    dist_root = artifacts / "dist"
    return dist_root, dist_root / hub.get("release", {}).get("staging_dir", "blind-trainer")


def release_zip_path(dist_root, version):
    import scripts.dist as dist

    return dist.zip_path(dist_root, version)


def release_verify_plan(hub, version):
    dist_root, staging = release_dist_paths(hub)
    zip_path = release_zip_path(dist_root, version)
    return staging, zip_path, [str(staging), str(zip_path), "--run"]


def release_verify_timeout(hub):
    return RELEASE_OVERHEAD_SECS + RELEASE_SCENARIOS * hub["release"]["run_timeout_secs"]


def release_verify_invoke(exe, argv, hub, logger):
    return run_policy_exe(exe, argv, "release-verify", logger, env=child_env(hub), echo=True,
                          timeout=release_verify_timeout(hub))


def release_stage_exe(bin_dir, app_name, release_name):
    app = bin_dir / app_name
    if not app.is_file():
        raise BtError(f"{app} missing, run python bt.py build first")
    target = bin_dir / release_name
    shutil.copy2(app, target)
    return target


def _ensure_release_verify(hub, logger):
    build_scope(hub, logger, "tools", only="release_verify")
    exe = REPO_ROOT / dirs_cfg(hub)["bin"] / "release-verify.exe"
    if not exe.is_file():
        raise BtError(f"release-verify.exe missing after build: {exe}")
    return exe


def tgt_release_verify(args):
    """byte-check the dist zip and run the packaged exe: python bt.py release-verify v=<tag>"""
    hub = load_hub()
    logger = Logger()
    version = release_version_arg(args)
    if not version:
        print("usage: python bt.py release-verify v=<tag>")
        return 2
    logger.entry("release-verify", f"v={version}")
    try:
        exe = _ensure_release_verify(hub, logger)
        _, _, argv = release_verify_plan(hub, version)
    except (BtError, OSError, KeyError, ValueError) as e:
        logger.fail("release-verify-build", f"{type(e).__name__}: {e}")
        return 1
    return release_verify_invoke(exe, argv, hub, logger)


def tgt_release_verify_selftest(args):
    """the release-verify unit suite"""
    hub = load_hub()
    logger = Logger()
    logger.entry("release-verify-selftest")
    try:
        build_scope(hub, logger, "tools", only="release_verify")
    except (BtError, OSError, KeyError, ValueError) as e:
        logger.fail("release-verify-selftest-build", f"{type(e).__name__}: {e}")
        return 1
    suites = [s for s in discover_test_suites(REPO_ROOT) if "release_verify" in s["stem"]]
    results = run_suite_exes(hub, suites)
    code, line = aggregate_results(results)
    logger.line("summary", line)
    logger.exit("release-verify-selftest", code)
    return code


def tgt_dist(args):
    """build the app, stage and verify the dist zip: python bt.py dist v=<tag>"""
    hub = load_hub()
    logger = Logger()
    version = release_version_arg(args)
    if not version:
        print("usage: python bt.py dist v=<tag>")
        return 2
    logger.entry("dist", f"v={version}")
    try:
        build_scope(hub, logger, "app")
        bin_dir = REPO_ROOT / dirs_cfg(hub)["bin"]
        staged_exe = release_stage_exe(
            bin_dir,
            build_cfg(hub).get("app_exe", "bt-app.exe"),
            hub.get("release", {}).get("exe", "blind-trainer.exe"),
        )
        logger.line("stage", f"{staged_exe.name} staged for dist")
        python = build_cfg(hub)["tools"].get("python", "") or sys.executable
        run([python, REPO_ROOT / "scripts" / "dist.py", "-Version", version], "dist-stage",
            env=child_env(hub))
    except (BtError, OSError, KeyError, ValueError) as e:
        logger.fail("dist", f"{type(e).__name__}: {e}")
        return 1
    logger.exit("dist", 0)
    return tgt_release_verify([f"v={version}"])


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
