import os
import sys
import tempfile
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

import bt

STUB = """import sys
from pathlib import Path
argv = sys.argv[1:]
with open(r'{log}', 'a', encoding='utf-8') as handle:
    handle.write(repr(argv) + '\\n')
if '-o' in argv:
    out = Path(argv[argv.index('-o') + 1])
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(b'stub')
"""


def set_mtime(path, when):
    os.utime(path, (when, when))


def past(path):
    set_mtime(path, time.time() - 120)


def newer_than_obj(path, obj):
    set_mtime(path, obj.stat().st_mtime + 2)


def main():
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        (root / "src" / "common").mkdir(parents=True)
        (root / "src" / "common" / "x.c").write_text("int x;\n", encoding="utf-8")
        (root / "vendor" / "sqlite").mkdir(parents=True)
        (root / "vendor" / "sqlite" / "sqlite3.c").write_text("int sq;\n", encoding="utf-8")
        cu_dir = root / "src" / "ga" / "cuda"
        cu_dir.mkdir(parents=True)
        cu = cu_dir / "ga_eval.cu"
        cu.write_text("int cu;\n", encoding="utf-8")
        cu_header = cu_dir / "ga_eval.h"
        cu_header.write_text("int cu_h;\n", encoding="utf-8")
        log = root / "argv.log"
        stub_clang = root / "clang_stub.py"
        stub_nvcc = root / "nvcc_stub.py"
        stub_clang.write_text(STUB.format(log=log), encoding="utf-8")
        stub_nvcc.write_text(STUB.format(log=log), encoding="utf-8")
        hub = bt.load_hub()
        plan = {
            "ok": True,
            "nvcc": [sys.executable, "-I", str(stub_nvcc)],
            "ccbin": str(root / "hostx64"),
            "arch": "sm_86",
            "lib_dir": str(root / "lib" / "x64"),
            "detail": "stub",
        }
        compiler = [sys.executable, "-I", str(stub_clang)]
        obj_root = root / "build" / "obj" / bt.objects_dirname(hub, cuda=True)
        cu_obj = bt.object_path(obj_root, root, cu)

        def stage():
            return bt.compile_stage(hub, root, compiler, bt.Logger(), cuda=plan)

        def nvcc_count():
            lines = [line for line in log.read_text(encoding="utf-8").splitlines() if line]
            return len([line for line in lines if "-ccbin" in line])

        stage()
        past(cu)
        past(cu_header)
        n_fresh = nvcc_count()
        stage()
        n_rerun = nvcc_count()
        newer_than_obj(cu, cu_obj)
        stage()
        n_cu_touch = nvcc_count()
        past(cu)
        stage()
        n_settle = nvcc_count()
        newer_than_obj(cu_header, cu_obj)
        stage()
        n_h_touch = nvcc_count()
        past(cu_header)
        stage()
        n_settle2 = nvcc_count()
        newer_than_obj(root / "src" / "common" / "x.c", cu_obj)
        stage()
        n_unrelated = nvcc_count()
        last = log.read_text(encoding="utf-8").splitlines()[-1]
        print(f"nvcc_invocations fresh={n_fresh} rerun={n_rerun} after_cu_touch={n_cu_touch} "
              f"settle={n_settle} after_h_touch={n_h_touch} settle2={n_settle2} "
              f"after_unrelated_touch={n_unrelated}")
        print(f"cu_obj={cu_obj} exists={cu_obj.is_file()}")
        print(f"last_nvcc_argv={last}")
        checks = [
            ("fresh build compiles the cu once", n_fresh == 1),
            ("immediate rerun skips nvcc", n_rerun == 1),
            ("cu source edit forces nvcc", n_cu_touch == 2),
            ("settled rerun skips nvcc again", n_settle == 2),
            ("cu header edit forces nvcc", n_h_touch == 3),
            ("second settled rerun skips nvcc", n_settle2 == 3),
            ("unrelated c edit leaves nvcc skipped", n_unrelated == 3),
        ]
        for name, ok in checks:
            print(f"check {name}: {'pass' if ok else 'FAIL'}")
        return 0 if all(ok for _, ok in checks) else 1


if __name__ == "__main__":
    sys.exit(main())
