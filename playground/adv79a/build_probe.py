import subprocess
import shutil
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
OBJ = REPO / "build/obj/70dcf3245464"
OUT = REPO / "build/bin/adv79a_review_probe.exe"
CLANG = shutil.which("clang")

FLAGS = [
    "-target", "x86_64-pc-windows-msvc", "-std=c23", "-Wall", "-Wextra", "-Werror",
    "-ffp-contract=off", "-g0", "-D_CRT_SECURE_NO_WARNINGS", "-O2", "-DNDEBUG",
]
INCLUDES = [
    "-I", str(REPO / "src"),
    "-isystem", str(REPO / "vendor/sqlite"),
    "-isystem", str(REPO / "vendor/raylib/src"),
    "-isystem", str(REPO / "vendor/raylib/src/external/glfw/include"),
]
LINK_FLAGS = ["-fuse-ld=lld-link"]
for flag in ["/SUBSYSTEM:CONSOLE", "/OPT:REF", "/OPT:ICF", "/DEBUG:NONE", "/brepro"]:
    LINK_FLAGS += ["-Xlinker", flag]
LIBS = ["-lopengl32", "-lgdi32", "-luser32", "-lshell32", "-lwinmm"]


def main():
    src_objects = [
        p for p in sorted((OBJ / "src").rglob("*.obj"))
        if p.name != "main.obj" and "ga" not in p.parts
    ]
    vendor_objects = sorted((OBJ / "vendor").rglob("*.obj"))
    probe_obj = REPO / "build/adv79a_probe_review.obj"
    compile_cmd = [
        CLANG, *FLAGS, *INCLUDES, "-c",
        str(REPO / "playground/adv79a/probe_review.c"),
        "-o", str(probe_obj),
    ]
    subprocess.run(compile_cmd, check=True)
    objects = [str(probe_obj), *[str(p) for p in src_objects],
               *[str(p) for p in vendor_objects],
               str(REPO / "build/vendor/raylib.lib")]
    link_cmd = [CLANG, *LINK_FLAGS, *objects, *LIBS, "-o", str(OUT)]
    subprocess.run(link_cmd, check=True)
    print(OUT)


if __name__ == "__main__":
    sys.exit(main())
