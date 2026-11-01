import os
import sys

mode = sys.argv[1]
count = int(sys.argv[2])
log = os.path.join(os.path.dirname(os.path.abspath(__file__)), sys.argv[3])
payload = ("x" * 180 + " line-end\n").encode("ascii")
for i in range(count):
    line = f"{os.getpid()} {i} ".encode("ascii") + payload
    if mode == "text":
        with open(log, "a", encoding="ascii") as handle:
            handle.write(line.decode("ascii"))
    else:
        fd = os.open(log, os.O_WRONLY | os.O_CREAT | os.O_APPEND)
        try:
            os.write(fd, line)
        finally:
            os.close(fd)
