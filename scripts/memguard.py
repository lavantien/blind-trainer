import argparse
import json
import os
import re
import subprocess
import sys
import time
import uuid
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import slog
import winproc

MB = 1024 * 1024
TOOL = slog.Tool("memguard")
REPO = Path(__file__).resolve().parent.parent
WRAP_NAME = re.compile(r"^[A-Za-z0-9._-]+$")


def fail(message):
    return TOOL.line("fail", message)


def fmt_mb(value):
    return f"{value / MB:.1f}"


def norm_repo_prefix(repo):
    return str(repo).replace("\\", "/").lower().rstrip("/") + "/"


def is_repo_path(exe, repo_norm):
    return exe.replace("\\", "/").lower().startswith(repo_norm)


def parse_pid_file(text, own_pid, live_ticks):
    """valid live foreign pid from a '<pid> <ticks>' stamp, else None."""
    parts = text.strip().split()
    if len(parts) != 2:
        return None
    try:
        pid = int(parts[0])
        ticks = int(parts[1])
    except ValueError:
        return None
    if pid <= 0 or pid > 2147483647:
        return None
    if pid == own_pid:
        return None
    live = live_ticks(pid)
    if live is None or live != ticks:
        return None
    return pid


def stamp_line(pid, ticks):
    return f"{pid} {ticks}"


def pid_stamp(pid):
    ticks = winproc.pid_start_ticks(pid)
    if ticks is None:
        return None
    return stamp_line(pid, ticks)


def repo_rows(repo):
    prefix = norm_repo_prefix(repo)
    rows = {}
    own = os.getpid()
    for proc in winproc.snapshot_processes():
        pid = proc["pid"]
        if pid == own:
            continue
        exe = winproc.pid_exe_path(pid)
        if exe is None or not is_repo_path(exe, prefix):
            continue
        row = winproc.process_row(pid)
        if row is None:
            continue
        rows[pid] = row
    return list(rows.values())


def config_path():
    env = os.environ.get("BT_CONFIG", "")
    if env:
        return Path(env) if Path(env).is_absolute() else REPO / env
    return REPO / "config" / "config.json"


def memguard_value(key):
    path = config_path()
    if not path.is_file():
        fail(f"config not found: {path}, set -CapBytes or add memguard.cap_bytes")
        raise SystemExit(2)
    try:
        doc = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        fail(f"config unreadable: {path} : {exc}")
        raise SystemExit(2)
    value = None
    if isinstance(doc, dict):
        section = doc.get("memguard")
        if isinstance(section, dict):
            value = section.get(key)
    if value is None:
        fail(f"config section missing: memguard.{key} in {path}")
        raise SystemExit(2)
    return value


def resolve_cap(override, config_cap):
    if isinstance(override, int) and override > 0:
        return override
    if isinstance(override, int) and override < 0:
        return "-CapBytes must be a positive byte count"
    cap = config_cap
    if isinstance(cap, str):
        try:
            cap = int(cap)
        except ValueError:
            cap = 0
    if not isinstance(cap, int) or cap <= 0:
        return "config value invalid: memguard.cap_bytes"
    return cap


def resolve_pid_dir():
    pid_dir = str(memguard_value("pid_dir")).strip()
    if not pid_dir:
        fail(f"config value invalid: memguard.pid_dir in {config_path()}")
        raise SystemExit(2)
    path = Path(pid_dir)
    return path if path.is_absolute() else REPO / path


def pid_tree_roots(pid_dir):
    if not Path(pid_dir).is_dir():
        return []
    own = os.getpid()
    roots = []
    for file in sorted(Path(pid_dir).glob("*.pid")):
        try:
            text = file.read_text(encoding="utf-8")
        except OSError:
            continue
        pid = parse_pid_file(text, own, winproc.pid_start_ticks)
        if pid is not None:
            roots.append(pid)
    return roots


def tree_ids(roots):
    index = {}
    for proc in winproc.snapshot_processes():
        index.setdefault(proc["ppid"], []).append(proc["pid"])
    seen = set()
    queue = list(dict.fromkeys(roots))
    while queue:
        current = queue.pop(0)
        if current in seen:
            continue
        seen.add(current)
        queue.extend(index.get(current, []))
    return seen


def tracked_rows(pid_dir):
    rows = {row["id"]: row for row in repo_rows(REPO)}
    for pid in tree_ids(pid_tree_roots(pid_dir)):
        if pid == os.getpid():
            continue
        row = winproc.process_row(pid)
        if row is not None:
            rows.setdefault(pid, row)
    return [rows[pid] for pid in sorted(rows)]


def taskkill_tree(pid):
    try:
        done = subprocess.run(
            ["taskkill", "/PID", str(pid), "/T", "/F"], capture_output=True, text=True
        )
        return done.returncode == 0
    except OSError:
        return False


def wait_process_gone(pid, seconds, poll=0.2):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if not winproc.pid_alive(pid):
            return True
        time.sleep(poll)
    return not winproc.pid_alive(pid)


def monitor(rows, cap, kill=False, killer=None, waiter=None):
    total = sum(row["ws"] for row in rows)
    if total <= cap:
        return {
            "code": 0,
            "lines": [f"memguard: {len(rows)} procs {fmt_mb(total)} MB / {fmt_mb(cap)} MB cap ok"],
            "stderr": False,
        }
    largest = max(rows, key=lambda row: row["ws"])
    breach = (
        f"memguard: {len(rows)} procs {fmt_mb(total)} MB / {fmt_mb(cap)} MB cap breach,"
        f" largest pid {largest['id']} {largest['name']} {fmt_mb(largest['ws'])} MB"
    )
    if not kill:
        return {"code": 3, "lines": [breach], "stderr": True}
    lines = [breach]
    (killer or taskkill_tree)(largest["id"])
    wait = waiter if waiter is not None else lambda pid, seconds: wait_process_gone(pid, seconds)
    gone = bool(wait(largest["id"], 5))
    if gone:
        lines.append(f"memguard: killed pid {largest['id']} {largest['name']}, verified gone")
        return {"code": 3, "lines": lines, "stderr": False}
    lines.append(f"memguard: kill failed for pid {largest['id']} {largest['name']}, still alive")
    return {"code": 5, "lines": lines, "stderr": True}


def split_wrap_command(command):
    tokens = []
    for match in re.finditer(r'"([^"]*)"|(\S+)', command.strip()):
        tokens.append(match.group(1) if match.group(1) is not None else match.group(2))
    return tokens


def wrap_payload_argv(tokens):
    lowered = tokens[0].strip().lower()
    if lowered.endswith(".bat") or lowered.endswith(".cmd"):
        return ["cmd.exe", "/c"] + list(tokens)
    return list(tokens)


def spawn_hidden(argv, stderr_file=None):
    kwargs = {"stdout": subprocess.DEVNULL, "stderr": subprocess.DEVNULL}
    stream = None
    if stderr_file:
        stream = open(stderr_file, "wb")
        kwargs["stderr"] = stream
    creationflags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    try:
        return subprocess.Popen(argv, creationflags=creationflags, **kwargs)
    finally:
        if stream is not None:
            stream.close()


def selftest(sleeper_path):
    if not sleeper_path or not Path(sleeper_path).is_file():
        fail(f"selftest sleeper binary not found: {sleeper_path}")
        return 4
    sleeper_full = str(Path(sleeper_path).resolve())
    if not is_repo_path(sleeper_full, norm_repo_prefix(REPO)):
        fail(f"selftest sleeper binary is outside the repo: {sleeper_full}")
        return 4
    temp_dir = REPO / f".memguard-selftest-{uuid.uuid4().hex}"
    temp_dir.mkdir()
    code = 5
    sleeper = None
    decoy = None
    tree_sleeper = None
    tree_root = None
    tree_child_id = 0
    outside_dir = None
    wrap_proc = None
    wrap_child2_id = 0
    wrap_pid_file = None
    try:
        copy_exe = temp_dir / "bt-sleeper.exe"
        copy_exe.write_bytes(Path(sleeper_full).read_bytes())
        launch_exe = winproc.short_path(str(copy_exe)) or str(copy_exe)
        sleeper = spawn_hidden([launch_exe, "120"])
        decoy_script = f"print({str(REPO)!r})\nimport time\ntime.sleep(90)"
        decoy = spawn_hidden([sys.executable, "-c", decoy_script])
        TOOL.out(
            f"memguard selftest ok: decoy pid {decoy.pid} command line carries the repo path, exe is {sys.executable}"
        )
        if not _await(lambda: any(row["id"] == sleeper.pid for row in repo_rows(REPO))):
            fail(f"selftest sleeper pid {sleeper.pid} not matched by exe path, launched as {launch_exe}")
            return 5
        TOOL.out(f"memguard selftest ok: sleeper pid {sleeper.pid} matched via exe path, launched as {launch_exe}")
        if any(row["id"] == decoy.pid for row in repo_rows(REPO)):
            fail(f"selftest decoy pid {decoy.pid} matched, command line matching leaked into the matcher")
            return 5
        TOOL.out(f"memguard selftest ok: decoy pid {decoy.pid} not matched")
        pid_dir = temp_dir / "pids"
        pid_dir.mkdir()
        resolved_pid_dir = resolve_pid_dir()
        if not str(resolved_pid_dir).strip():
            fail("selftest resolved an empty memguard.pid_dir")
            return 5
        TOOL.out(f"memguard selftest ok: memguard.pid_dir resolved from config, {resolved_pid_dir}")
        outside_dir = Path(tempfile_root()) / f"bt-memguard-selftest-{uuid.uuid4().hex}"
        outside_dir.mkdir()
        outside_exe = outside_dir / "bt-sleeper.exe"
        outside_exe.write_bytes(Path(sleeper_full).read_bytes())
        tree_sleeper = spawn_hidden([str(outside_exe), "120"])
        dead = spawn_hidden([sys.executable, "-c", "pass"])
        tree_root_script = f"import subprocess\nsubprocess.run([{str(outside_exe)!r}, '120'])"
        tree_root = spawn_hidden([sys.executable, "-c", tree_root_script])
        if not wait_process_gone(dead.pid, 10):
            fail(f"selftest stale pid {dead.pid} still alive")
            return 5
        _write(pid_dir / "selftest-stale.pid", f"{dead.pid} 0")
        _write(pid_dir / "selftest-garbage.pid", "abc")
        _write(pid_dir / "selftest-negative.pid", "-5 0")
        _write(pid_dir / "selftest-zero.pid", "0 0")
        _write(pid_dir / "selftest-overflow.pid", "99999999999999999999 0")
        _write(pid_dir / "selftest-unstamped.pid", str(decoy.pid))
        sleeper_stamp = pid_stamp(sleeper.pid)
        if sleeper_stamp is None:
            fail(f"selftest cannot stamp repo sleeper pid {sleeper.pid}")
            return 5
        _write(pid_dir / "selftest-dup.pid", sleeper_stamp)
        tree_stamp = pid_stamp(tree_sleeper.pid)
        if tree_stamp is None:
            fail(f"selftest cannot stamp pidfile sleeper pid {tree_sleeper.pid}")
            return 5
        _write(pid_dir / "selftest-sleeper.pid", tree_stamp)
        def child_of(root):
            for _ in range(40):
                for proc in winproc.snapshot_processes():
                    if proc["ppid"] == root and proc["exe"].startswith("bt-sleeper"):
                        return proc["pid"]
                time.sleep(0.25)
            return 0
        tree_child_id = child_of(tree_root.pid)
        if tree_child_id == 0:
            fail(f"selftest tree root pid {tree_root.pid} never spawned the child sleeper")
            return 5
        root_stamp = pid_stamp(tree_root.pid)
        if root_stamp is None:
            fail(f"selftest cannot stamp tree root pid {tree_root.pid}")
            return 5
        _write(pid_dir / "selftest-tree.pid", root_stamp)
        time.sleep(0.5)
        rows = tracked_rows(str(pid_dir))
        for expected in (tree_sleeper.pid, tree_root.pid, tree_child_id):
            if expected not in [row["id"] for row in rows]:
                fail(f"selftest pid {expected} not matched via pidfile tree, exe outside repo at {outside_exe}")
                return 5
        TOOL.out(
            f"memguard selftest ok: pidfile tree matched pids {tree_sleeper.pid}, {tree_root.pid}, {tree_child_id}"
        )
        if dead.pid in [row["id"] for row in rows]:
            fail(f"selftest stale pid {dead.pid} matched from a dead pidfile")
            return 5
        TOOL.out(f"memguard selftest ok: stale pidfile with dead pid {dead.pid} ignored")
        if decoy.pid in [row["id"] for row in rows]:
            fail("selftest decoy pid matched, command line matching or unstamped pidfiles leaked into the matcher")
            return 5
        TOOL.out(f"memguard selftest ok: decoy pid {decoy.pid} not matched by pidtree, malformed and unstamped pidfiles ignored")
        if [row["id"] for row in rows].count(sleeper.pid) != 1:
            fail(f"selftest repo sleeper pid {sleeper.pid} dedupe broken")
            return 5
        TOOL.out(f"memguard selftest ok: pid {sleeper.pid} counted once across exe path and pidfile tree")
        live = winproc.process_row(tree_sleeper.pid)
        if live is None:
            fail(f"selftest pidfile sleeper pid {tree_sleeper.pid} died before the kill fixture")
            return 5
        no_kill = monitor([live], 1, kill=False)
        if no_kill["code"] != 3:
            fail(f"selftest breach without -Kill returned {no_kill['code']}")
            return 5
        if not winproc.pid_alive(tree_sleeper.pid):
            fail(f"selftest breach without -Kill touched pid {tree_sleeper.pid}")
            return 5
        TOOL.out(f"memguard selftest ok: breach without -Kill left pid {tree_sleeper.pid} alive")
        live = winproc.process_row(tree_sleeper.pid)
        with_kill = monitor([live], 1, kill=True)
        if with_kill["code"] != 3 or not wait_process_gone(tree_sleeper.pid, 5):
            fail(f"selftest -Kill returned {with_kill['code']} and left pid {tree_sleeper.pid} unverified")
            return 5
        TOOL.out(f"memguard selftest ok: -Kill killed pidfile pid {tree_sleeper.pid}, verified gone")
        outside_exe2 = outside_dir / "bt-sleeper-2.exe"
        outside_exe2.write_bytes(Path(sleeper_full).read_bytes())
        wrap_err = temp_dir / "wrap-stderr.txt"
        wrap_argv = [
            sys.executable,
            str(Path(__file__).resolve()),
            "-Wrap",
            "selftest-wrap",
            "-WrapCommand",
            f"{str(outside_exe2).replace(chr(92), '/')} 8",
        ]
        wrap_proc = spawn_hidden(wrap_argv, stderr_file=str(wrap_err))
        wrap_pid_file = Path(resolved_pid_dir) / f"selftest-wrap-{wrap_proc.pid}.pid"
        if not _await(lambda: wrap_pid_file.is_file(), tries=40):
            err = wrap_err.read_text(encoding="utf-8", errors="replace").strip() if wrap_err.exists() else ""
            fail(f"selftest wrap mode never wrote {wrap_pid_file}, wrapper stderr: {err}")
            return 5
        wrap_pid = int(wrap_pid_file.read_text(encoding="utf-8").split()[0])
        if wrap_pid != wrap_proc.pid:
            fail(f"selftest wrap pidfile holds pid {wrap_pid}, expected wrapper pid {wrap_proc.pid}")
            return 5
        def sleeper2_pid():
            for proc in winproc.snapshot_processes():
                if proc["exe"] == "bt-sleeper-2.exe":
                    return proc["pid"]
            return 0
        wrap_child2_id = _await(sleeper2_pid, tries=40)
        if wrap_child2_id == 0:
            fail("selftest wrap command never spawned bt-sleeper-2.exe")
            return 5
        rows = tracked_rows(str(resolved_pid_dir))
        ids = [row["id"] for row in rows]
        if wrap_proc.pid not in ids or wrap_child2_id not in ids:
            fail(f"selftest wrap tree pids {wrap_proc.pid} or {wrap_child2_id} not matched")
            return 5
        TOOL.out(f"memguard selftest ok: wrap mode pidfile matched wrapper pid {wrap_proc.pid} and sleeper pid {wrap_child2_id}")
        try:
            wrap_proc.wait(timeout=20)
        except subprocess.TimeoutExpired:
            pass
        time.sleep(0.5)
        if wrap_pid_file.exists():
            fail(f"selftest wrap mode left {wrap_pid_file} behind")
            return 5
        if wrap_proc.returncode != 0:
            fail(f"selftest wrap mode exited with {wrap_proc.returncode}")
            return 5
        TOOL.out("memguard selftest ok: wrap mode removed the pidfile and exited 0")
        code = 0
    finally:
        code = _selftest_cleanup(
            code,
            sleeper,
            decoy,
            tree_root,
            tree_child_id,
            tree_sleeper,
            wrap_proc,
            wrap_child2_id,
            wrap_pid_file,
            outside_dir,
            temp_dir,
        )
    return code


def tempfile_root():
    return os.environ.get("TEMP") or os.environ.get("TMP") or "."


def _write(path, text):
    path.write_text(text, encoding="utf-8")


def _await(probe, tries=20, delay=0.25):
    for _ in range(tries):
        value = probe()
        if value:
            return value
        time.sleep(delay)
    return probe()


def _selftest_cleanup(code, sleeper, decoy, tree_root, tree_child_id, tree_sleeper, wrap_proc, wrap_child2_id, wrap_pid_file, outside_dir, temp_dir):
    ok = True
    if sleeper is not None:
        taskkill_tree(sleeper.pid)
        ok = ok and wait_process_gone(sleeper.pid, 5)
        if ok:
            TOOL.out(f"memguard selftest ok: sleeper pid {sleeper.pid} killed and gone")
    if decoy is not None:
        taskkill_tree(decoy.pid)
        ok = ok and wait_process_gone(decoy.pid, 5)
        if ok:
            TOOL.out(f"memguard selftest ok: decoy pid {decoy.pid} killed and gone")
    if tree_root is not None:
        taskkill_tree(tree_root.pid)
        gone = wait_process_gone(tree_root.pid, 5)
        if tree_child_id:
            gone = gone and wait_process_gone(tree_child_id, 5)
        ok = ok and gone
        if gone:
            TOOL.out(f"memguard selftest ok: tree root pid {tree_root.pid} and child killed and gone")
    if tree_sleeper is not None:
        taskkill_tree(tree_sleeper.pid)
        ok = ok and wait_process_gone(tree_sleeper.pid, 5)
    if wrap_proc is not None:
        taskkill_tree(wrap_proc.pid)
        gone = wait_process_gone(wrap_proc.pid, 5)
        if wrap_child2_id:
            gone = gone and wait_process_gone(wrap_child2_id, 5)
        ok = ok and gone
        if gone:
            TOOL.out(f"memguard selftest ok: wrap wrapper pid {wrap_proc.pid} tree killed and gone")
    if wrap_pid_file is not None and Path(wrap_pid_file).exists():
        try:
            Path(wrap_pid_file).unlink()
        except OSError:
            pass
        ok = ok and not Path(wrap_pid_file).exists()
    if outside_dir is not None and outside_dir.exists():
        _rmtree(outside_dir)
        ok = ok and not outside_dir.exists()
    if temp_dir.exists():
        _rmtree(temp_dir)
        ok = ok and not temp_dir.exists()
    if not ok:
        fail("selftest cleanup failed")
        return 5
    return code


def _rmtree(path):
    for child in sorted(Path(path).rglob("*"), reverse=True):
        try:
            if child.is_dir():
                child.rmdir()
            else:
                child.unlink()
        except OSError:
            pass
    try:
        Path(path).rmdir()
    except OSError:
        pass


def run_wrap(name, command):
    if not WRAP_NAME.match(name or ""):
        fail(f"-Wrap must be a plain name part: {name}")
        raise SystemExit(2)
    if not (command or "").strip():
        fail("-WrapCommand must not be empty")
        raise SystemExit(2)
    pid_dir = resolve_pid_dir()
    pid_dir.mkdir(parents=True, exist_ok=True)
    own = os.getpid()
    stamp = pid_stamp(own)
    if stamp is None:
        fail(f"cannot stamp wrapper pid {own} for wrap {name}")
        raise SystemExit(2)
    payload = split_wrap_command(command)
    if not payload or not payload[0].strip():
        fail(f"-WrapCommand must name an executable: {command}")
        raise SystemExit(2)
    pid_file = pid_dir / f"{name}-{own}.pid"
    _write(pid_file, stamp)
    try:
        done = subprocess.run(wrap_payload_argv(payload))
        raise SystemExit(done.returncode)
    except OSError as exc:
        fail(f"wrap payload failed to launch: {payload[0]} : {exc}")
        raise SystemExit(2)
    finally:
        try:
            pid_file.unlink()
        except OSError:
            pass


def main(argv=None):
    parser = argparse.ArgumentParser(
        prog="memguard.py",
        description="report repo process memory against the cap, selftest the census, or wrap a command with a pid stamp",
        allow_abbrev=False,
    )
    parser.add_argument("-CapBytes", "--cap-bytes", dest="cap_bytes", type=int, default=0)
    parser.add_argument("-Kill", "-kill", "--kill", dest="kill", action="store_true")
    parser.add_argument("-SelfTest", "-self-test", "--self-test", dest="self_test", action="store_true")
    parser.add_argument("-SleeperPath", "--sleeper-path", dest="sleeper_path", default="")
    parser.add_argument("-Wrap", "--wrap", dest="wrap", default="")
    parser.add_argument("-WrapCommand", "--wrap-command", dest="wrap_command", default="")
    args = parser.parse_args(argv)
    TOOL.entry("memguard", f"selftest={args.self_test} wrap={args.wrap or '-'}")
    if args.self_test:
        code = selftest(args.sleeper_path)
        TOOL.exit("selftest", code)
        return code
    if args.wrap:
        run_wrap(args.wrap, args.wrap_command)
        return 0
    cap = resolve_cap(args.cap_bytes, memguard_value("cap_bytes"))
    if isinstance(cap, str):
        fail(cap)
        return 2
    result = monitor(tracked_rows(resolve_pid_dir()), cap, kill=args.kill)
    for line in result["lines"]:
        if result["stderr"]:
            fail(line.removeprefix("memguard: "))
        else:
            TOOL.out(line)
    TOOL.exit("memguard", result["code"])
    return result["code"]


if __name__ == "__main__":
    sys.exit(main())
