import ctypes
import subprocess
import sys
import time
from ctypes import wintypes
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "scripts"))

import winproc


class PBI_PHNT(ctypes.Structure):
    _fields_ = [
        ("ExitStatus", ctypes.c_long),
        ("PebBaseAddress", ctypes.c_void_p),
        ("AffinityMask", ctypes.c_size_t),
        ("BasePriority", ctypes.c_long),
        ("UniqueProcessId", ctypes.c_void_p),
        ("InheritedFromUniqueProcessId", ctypes.c_void_p),
    ]


def raw_pbi(pid):
    ntdll = ctypes.WinDLL("ntdll")
    handle = winproc._open(pid, 0x1000)
    info = PBI_PHNT()
    retlen = wintypes.ULONG(0)
    status = ntdll.NtQueryInformationProcess(
        handle, 0, ctypes.byref(info), ctypes.sizeof(info), ctypes.byref(retlen)
    )
    winproc._kernel32().CloseHandle(handle)
    return status, retlen.value, info


child = subprocess.Popen(
    [sys.executable, "-c", "import time; time.sleep(25)"],
    creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
)
time.sleep(1.5)
try:
    status, retlen, pbi = raw_pbi(child.pid)
    print(f"probe1 raw phnt layout child pid={child.pid} status={status:#x} retlen={retlen} sizeof={ctypes.sizeof(PBI_PHNT)}")
    print(f"probe1 raw UniqueProcessId={pbi.UniqueProcessId} InheritedFromUniqueProcessId={pbi.InheritedFromUniqueProcessId} (expect pid={child.pid} ppid={None})")
    own = ctypes.windll.kernel32.GetCurrentProcessId()
    status2, retlen2, pbi2 = raw_pbi(own)
    print(f"probe1 raw self UniqueProcessId={pbi2.UniqueProcessId} InheritedFromUniqueProcessId={pbi2.InheritedFromUniqueProcessId} (expect self pid {own})")

    rows = winproc.snapshot_processes()
    child_row = next((r for r in rows if r["pid"] == child.pid), None)
    self_row = next((r for r in rows if r["pid"] == own), None)
    print(f"probe2 snapshot rows={len(rows)} empty_exe_rows={sum(1 for r in rows if not r['exe'])}")
    print(f"probe2 child row={child_row} own_pid={own} self_row_ppid={self_row['ppid'] if self_row else 'missing'}")
    ppid_ok = child_row is not None and child_row["ppid"] == own
    print(f"probe2 child ppid correct: {ppid_ok}")

    ticks = winproc.pid_start_ticks(child.pid)
    back = datetime.fromtimestamp(ticks / 10_000_000 - 62135596800, tz=timezone.utc)
    age = (datetime.now(timezone.utc) - back).total_seconds()
    print(f"probe3 child start ticks={ticks} back_to_utc={back.isoformat()} age_secs={age:.1f} plausible={0 <= age <= 120}")
finally:
    child.kill()
    child.wait()

zombie = subprocess.Popen(
    [sys.executable, "-c", "import sys; sys.exit(259)"],
    creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
)
zombie.wait()
print(f"probe4 zombie exit code={zombie.returncode} pid_alive reports={winproc.pid_alive(zombie.pid)} (True means dead process counted alive)")
gone = winproc.wait_process_gone(zombie.pid, 0.5)
print(f"probe4 wait_process_gone on dead-259 pid returned={gone} (False means kill verification would lie)")
zombie.__del__()

import dist

import tempfile

with tempfile.TemporaryDirectory() as tmp:
    repo = Path(tmp)
    exe = repo / "build" / "bin" / "blind-trainer.exe"
    exe.parent.mkdir(parents=True)
    exe.write_text("exe")
    release = {"staging_dir": "s", "exe": "blind-trainer.exe", "config_file": "config/config.json", "fonts_dir": "assets/fonts"}
    plan = dist.staging_plan(repo, repo / "out", release)
    print(f"probe5 dist plan with NO config and NO fonts: {[(str(s.name), str(d.name)) for s, d in plan]} len={len(plan)} (ps1 would fail hard)")
