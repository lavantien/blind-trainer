import ctypes
import sys
from ctypes import wintypes

PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
PROCESS_VM_READ = 0x10
STILL_ACTIVE = 259
DOTNET_FILETIME_OFFSET = 504911232000000000
PATH_BUFFER_WCHARS = 1024


class PROCESS_BASIC_INFORMATION(ctypes.Structure):
    _fields_ = [
        ("Reserved1", ctypes.c_void_p * 2),
        ("PebBaseAddress", ctypes.c_void_p),
        ("Reserved2", ctypes.c_void_p * 2),
        ("UniqueProcessId", ctypes.c_void_p),
        ("InheritedFromUniqueProcessId", ctypes.c_void_p),
        ("Reserved3", ctypes.c_void_p),
    ]


class FILETIME(ctypes.Structure):
    _fields_ = [
        ("dwLowDateTime", wintypes.DWORD),
        ("dwHighDateTime", wintypes.DWORD),
    ]


class PROCESS_MEMORY_COUNTERS(ctypes.Structure):
    _fields_ = [
        ("cb", wintypes.DWORD),
        ("PageFaultCount", wintypes.DWORD),
        ("PeakWorkingSetSize", ctypes.c_size_t),
        ("WorkingSetSize", ctypes.c_size_t),
        ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
        ("QuotaPagedPoolUsage", ctypes.c_size_t),
        ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
        ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
        ("PagefileUsage", ctypes.c_size_t),
        ("PeakPagefileUsage", ctypes.c_size_t),
    ]


def _kernel32():
    if sys.platform != "win32":
        raise OSError("winproc helpers are windows only")
    return ctypes.WinDLL("kernel32", use_last_error=True)


def _psapi():
    if sys.platform != "win32":
        raise OSError("winproc helpers are windows only")
    try:
        return ctypes.WinDLL("psapi", use_last_error=True)
    except OSError:
        return _kernel32()


def _ntdll():
    if sys.platform != "win32":
        raise OSError("winproc helpers are windows only")
    return ctypes.WinDLL("ntdll")


def _pid_ppid(pid):
    k32 = _kernel32()
    ntdll = _ntdll()
    handle = _open(pid, PROCESS_QUERY_LIMITED_INFORMATION)
    if not handle:
        return 0
    try:
        info = PROCESS_BASIC_INFORMATION()
        retlen = wintypes.ULONG(0)
        status = ntdll.NtQueryInformationProcess(
            handle, 0, ctypes.byref(info), ctypes.sizeof(info), ctypes.byref(retlen)
        )
        if status != 0 or info.InheritedFromUniqueProcessId is None:
            return 0
        return int(info.InheritedFromUniqueProcessId)
    finally:
        k32.CloseHandle(handle)


def snapshot_processes():
    """returns a list of dicts: pid, ppid, exe (basename) for every enumerable process."""
    psapi = _psapi()
    psapi.EnumProcesses.argtypes = [
        ctypes.POINTER(wintypes.DWORD),
        wintypes.DWORD,
        ctypes.POINTER(wintypes.DWORD),
    ]
    capacity = 1024
    while True:
        pids = (wintypes.DWORD * capacity)()
        needed = wintypes.DWORD(0)
        if not psapi.EnumProcesses(pids, ctypes.sizeof(pids), ctypes.byref(needed)):
            raise ctypes.WinError(ctypes.get_last_error())
        count = needed.value // ctypes.sizeof(wintypes.DWORD)
        if count < capacity:
            break
        capacity *= 2
    rows = []
    for index in range(count):
        pid = int(pids[index])
        if pid == 0:
            continue
        path = pid_exe_path(pid)
        rows.append(
            {
                "pid": pid,
                "ppid": _pid_ppid(pid),
                "exe": path.rsplit("\\", 1)[-1].rsplit("/", 1)[-1] if path else "",
            }
        )
    return rows


def _open(pid, access):
    k32 = _kernel32()
    k32.OpenProcess.restype = wintypes.HANDLE
    k32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    return k32.OpenProcess(access, False, pid)


def pid_exe_path(pid):
    """full image path or None; expands 8.3 short segments like the pwsh Get-Item route."""
    k32 = _kernel32()
    handle = _open(pid, PROCESS_QUERY_LIMITED_INFORMATION)
    if not handle:
        return None
    try:
        size = wintypes.DWORD(PATH_BUFFER_WCHARS)
        buf = ctypes.create_unicode_buffer(size.value)
        k32.QueryFullProcessImageNameW.argtypes = [
            wintypes.HANDLE,
            wintypes.DWORD,
            wintypes.LPWSTR,
            ctypes.POINTER(wintypes.DWORD),
        ]
        if not k32.QueryFullProcessImageNameW(handle, 0, buf, ctypes.byref(size)):
            return None
        path = buf.value
        if "~" in path:
            long_buf = ctypes.create_unicode_buffer(PATH_BUFFER_WCHARS)
            k32.GetLongPathNameW.argtypes = [wintypes.LPWSTR, wintypes.LPWSTR, wintypes.DWORD]
            if k32.GetLongPathNameW(path, long_buf, PATH_BUFFER_WCHARS):
                path = long_buf.value
        return path
    finally:
        k32.CloseHandle(handle)


def pid_working_set(pid):
    psapi = _psapi()
    handle = _open(pid, PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ)
    if not handle:
        return None
    try:
        counters = PROCESS_MEMORY_COUNTERS()
        counters.cb = ctypes.sizeof(PROCESS_MEMORY_COUNTERS)
        fn = getattr(psapi, "K32GetProcessMemoryInfo", None) or psapi.GetProcessMemoryInfo
        fn.argtypes = [wintypes.HANDLE, ctypes.POINTER(PROCESS_MEMORY_COUNTERS), wintypes.DWORD]
        if not fn(handle, ctypes.byref(counters), counters.cb):
            return None
        return int(counters.WorkingSetSize)
    finally:
        _kernel32().CloseHandle(handle)


def filetime_to_dotnet_ticks(low, high):
    return ((int(high) << 32) | int(low)) + DOTNET_FILETIME_OFFSET


def pid_start_ticks(pid):
    """process creation time in utc .NET ticks (100ns since 0001-01-01) or None."""
    k32 = _kernel32()
    handle = _open(pid, PROCESS_QUERY_LIMITED_INFORMATION)
    if not handle:
        return None
    try:
        creation = FILETIME()
        exit_ft = FILETIME()
        kernel = FILETIME()
        user = FILETIME()
        k32.GetProcessTimes.argtypes = [
            wintypes.HANDLE,
            ctypes.POINTER(FILETIME),
            ctypes.POINTER(FILETIME),
            ctypes.POINTER(FILETIME),
            ctypes.POINTER(FILETIME),
        ]
        if not k32.GetProcessTimes(
            handle, ctypes.byref(creation), ctypes.byref(exit_ft), ctypes.byref(kernel), ctypes.byref(user)
        ):
            return None
        return filetime_to_dotnet_ticks(creation.dwLowDateTime, creation.dwHighDateTime)
    finally:
        k32.CloseHandle(handle)


def pid_alive(pid):
    k32 = _kernel32()
    handle = _open(pid, PROCESS_QUERY_LIMITED_INFORMATION)
    if not handle:
        return False
    try:
        code = wintypes.DWORD(0)
        k32.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
        if not k32.GetExitCodeProcess(handle, ctypes.byref(code)):
            return False
        return code.value == STILL_ACTIVE
    finally:
        k32.CloseHandle(handle)


def pid_name(pid):
    path = pid_exe_path(pid)
    if path is None:
        return None
    stem = path.rsplit("\\", 1)[-1].rsplit("/", 1)[-1]
    return stem.rsplit(".", 1)[0] if "." in stem else stem


def process_row(pid):
    """{id, name, ws} for a live pid or None."""
    ws = pid_working_set(pid)
    name = pid_name(pid)
    if ws is None or name is None:
        return None
    return {"id": pid, "name": name, "ws": ws}


def short_path(path):
    """8.3 short form when available, else the input unchanged."""
    k32 = _kernel32()
    buf = ctypes.create_unicode_buffer(PATH_BUFFER_WCHARS)
    k32.GetShortPathNameW.argtypes = [wintypes.LPWSTR, wintypes.LPWSTR, wintypes.DWORD]
    if k32.GetShortPathNameW(path, buf, PATH_BUFFER_WCHARS):
        return buf.value
    return path
