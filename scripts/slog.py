import sys
import time


class Tool:
    """shared stderr tracing shim: entry, exit, fail lines with timestamps, user output stays on stdout."""

    def __init__(self, name):
        self.name = name

    def line(self, tag, text):
        stamp = time.strftime("%Y-%m-%dT%H:%M:%S")
        print(f"{stamp} [{tag}] {self.name}: {text}", file=sys.stderr, flush=True)
        return text

    def entry(self, unit, detail=""):
        return self.line("entry", f"{unit} {detail}".rstrip())

    def exit(self, unit, code, seconds=None):
        detail = f"{unit} code={code}"
        if seconds is not None:
            detail += f" dt={seconds:.3f}s"
        return self.line("exit", detail)

    def fail(self, unit, message):
        return self.line("fail", f"{unit} {message}")

    def out(self, text):
        print(text, flush=True)
        return text
