import sys

t = open(sys.argv[1], encoding="utf-8", errors="replace").read()
gi = t.index('got "')
wi = t.index('want "', gi)
got = t[gi + 5 : wi - 1]
want = t[wi + 6 :].rstrip("\n").rstrip('"')
gl, wl = got.split("\n"), want.split("\n")
for i in range(max(len(gl), len(wl))):
    a = gl[i] if i < len(gl) else "<missing>"
    b = wl[i] if i < len(wl) else "<missing>"
    if a != b:
        print("first diff at line", i)
        print("got :", repr(a))
        print("want:", repr(b))
        break
print("line counts", len(gl), len(wl))
print("got tail :", repr(got[-80:]))
print("want tail:", repr(want[-80:]))
