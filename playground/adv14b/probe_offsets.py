import ctypes
import sys
from ctypes import wintypes
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "scripts"))
import winproc

for name, _t in winproc.PROCESS_BASIC_INFORMATION._fields_:
    field = getattr(winproc.PROCESS_BASIC_INFORMATION, name)
    print(f"field {name} offset={field.offset} size={field.size}")
print(f"sizeof={ctypes.sizeof(winproc.PROCESS_BASIC_INFORMATION)}")

own = ctypes.windll.kernel32.GetCurrentProcessId()
handle = winproc._open(own, 0x1000)
info = winproc.PROCESS_BASIC_INFORMATION()
retlen = wintypes.ULONG(0)
ntdll = ctypes.WinDLL("ntdll")
status = ntdll.NtQueryInformationProcess(
    handle, 0, ctypes.byref(info), ctypes.sizeof(info), ctypes.byref(retlen)
)
winproc._kernel32().CloseHandle(handle)
raw = ctypes.string_at(ctypes.byref(info), ctypes.sizeof(info))
print(f"status={status:#x} retlen={retlen.value} own_pid={own}")
print(f"raw64={raw.hex()}")
print(f"UniqueProcessId={int(info.UniqueProcessId or 0)} InheritedFromUniqueProcessId={int(info.InheritedFromUniqueProcessId or 0)} Reserved3={int(info.Reserved3 or 0)} PebBaseAddress={int(info.PebBaseAddress or 0)}")
