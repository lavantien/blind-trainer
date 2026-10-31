import ctypes
import time
from ctypes import wintypes as wt

user32 = ctypes.WinDLL("user32", use_last_error=True)
gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

INPUT_MOUSE = 0
INPUT_KEYBOARD = 1
KEYEVENTF_KEYUP = 0x0002
KEYEVENTF_UNICODE = 0x0004
MOUSEEVENTF_LEFTDOWN = 0x0002
MOUSEEVENTF_LEFTUP = 0x0004
PW_RENDERFULLCONTENT = 2
SM_CXSCREEN = 0
SM_CYSCREEN = 1

VK_MAP = {
    "ESC": 0x1B,
    "ENTER": 0x0D,
    "SPACE": 0x20,
    "UP": 0x26,
    "DOWN": 0x28,
    "1": ord("1"),
    "2": ord("2"),
    "3": ord("3"),
    "4": ord("4"),
    "5": ord("5"),
    "6": ord("6"),
    "9": ord("9"),
    "V": ord("V"),
}


class MOUSEINPUT(ctypes.Structure):
    _fields_ = [("dx", wt.LONG), ("dy", wt.LONG), ("mouseData", wt.DWORD),
                ("dwFlags", wt.DWORD), ("time", wt.DWORD), ("dwExtraInfo", ctypes.POINTER(wt.ULONG))]


class KEYBDINPUT(ctypes.Structure):
    _fields_ = [("wVk", wt.WORD), ("wScan", wt.WORD), ("dwFlags", wt.DWORD), ("time", wt.DWORD),
                ("dwExtraInfo", ctypes.POINTER(wt.ULONG))]


class INPUT(ctypes.Structure):
    class _U(ctypes.Union):
        _fields_ = [("mi", MOUSEINPUT), ("ki", KEYBDINPUT)]

    _anonymous_ = ("u",)
    _fields_ = [("type", wt.DWORD), ("u", _U)]


def send_input(*inputs):
    array = (INPUT * len(inputs))(*inputs)
    sent = user32.SendInput(len(inputs), array, ctypes.sizeof(INPUT))
    if sent != len(inputs):
        raise OSError(f"SendInput delivered {sent} of {len(inputs)}")


def key_input(vk, flags=0, scan=0):
    item = INPUT(type=INPUT_KEYBOARD)
    item.ki = KEYBDINPUT(vk, scan, flags, 0, None)
    return item


def mouse_input(flags):
    item = INPUT(type=INPUT_MOUSE)
    item.mi = MOUSEINPUT(0, 0, 0, flags, 0, None)
    return item


def hwnd_for_title(title, timeout_secs=30.0):
    deadline = time.monotonic() + timeout_secs
    while time.monotonic() < deadline:
        hwnd = user32.FindWindowW(None, title)
        if hwnd and user32.IsWindowVisible(hwnd):
            return hwnd
        time.sleep(0.2)
    return 0


def client_origin(hwnd):
    pt = wt.POINT(0, 0)
    if not user32.ClientToScreen(hwnd, ctypes.byref(pt)):
        raise OSError("ClientToScreen failed")
    return pt.x, pt.y


def client_size(hwnd):
    rect = wt.RECT()
    if not user32.GetClientRect(hwnd, ctypes.byref(rect)):
        raise OSError("GetClientRect failed")
    return rect.right - rect.left, rect.bottom - rect.top


def move_mouse(x, y):
    user32.SetCursorPos(int(x), int(y))


def click(x, y, hold=0.05):
    move_mouse(x, y)
    time.sleep(0.03)
    send_input(mouse_input(MOUSEEVENTF_LEFTDOWN))
    time.sleep(hold)
    send_input(mouse_input(MOUSEEVENTF_LEFTUP))


def press(vk_name, hold=0.05):
    vk = VK_MAP[vk_name]
    send_input(key_input(vk))
    time.sleep(hold)
    send_input(key_input(vk, KEYEVENTF_KEYUP))


def type_chars(text, per=0.06):
    for ch in text:
        code = ord(ch)
        send_input(key_input(0, KEYEVENTF_UNICODE, code))
        time.sleep(per)
        send_input(key_input(0, KEYEVENTF_UNICODE | KEYEVENTF_KEYUP, code))


def wait_frames(secs):
    time.sleep(secs)


def capture(hwnd, path):
    w, h = client_size(hwnd)
    hdc_window = user32.GetDC(hwnd)
    hdc_mem = gdi32.CreateCompatibleDC(hdc_window)
    bmi = (ctypes.c_ubyte * 40)()
    ctypes.memset(bmi, 0, 40)
    struct_bmi = ctypes.cast(bmi, ctypes.POINTER(ctypes.c_ubyte))
    import struct

    header = struct.pack("<IiiHHIIiiII", 40, w, -h, 1, 32, 0, 0, 0, 0, 0, 0)
    ctypes.memmove(bmi, header.encode("latin1") if isinstance(header, str) else header, 40)
    pixels = ctypes.create_string_buffer(w * h * 4)
    bitmap = gdi32.CreateCompatibleBitmap(hdc_window, w, h)
    gdi32.SelectObject(hdc_mem, bitmap)
    got = user32.PrintWindow(hwnd, hdc_mem, PW_RENDERFULLCONTENT)
    if got != 1:
        got_bits = 0
    else:
        got_bits = gdi32.GetDIBits(hdc_mem, bitmap, 0, h, pixels, ctypes.byref(bmi), 0)
    gdi32.DeleteObject(bitmap)
    gdi32.DeleteDC(hdc_mem)
    user32.ReleaseDC(hwnd, hdc_window)
    if got_bits != h:
        raise OSError(f"capture failed: PrintWindow {got}, GetDIBits {got_bits}")
    from PIL import Image

    image = Image.frombytes("RGBA", (w, h), pixels.raw, "raw", "BGRA", 0, 1)
    image.convert("RGB").save(path)
    return path


def close_window(hwnd):
    user32.PostMessageW(hwnd, 0x0010, 0, 0)


def focus_window(hwnd):
    send_input(key_input(0x12), key_input(0x12, KEYEVENTF_KEYUP))
    user32.ShowWindow(hwnd, 9)
    user32.SetForegroundWindow(hwnd)
    user32.BringWindowToTop(hwnd)


def wait_gone(pid, timeout_secs=20.0):
    handle = kernel32.OpenProcess(0x00100000, False, pid)
    if handle == 0:
        return True
    deadline = time.monotonic() + timeout_secs
    while time.monotonic() < deadline:
        if kernel32.WaitForSingleObject(handle, 500) == 0:
            kernel32.CloseHandle(handle)
            return True
    kernel32.CloseHandle(handle)
    return False
