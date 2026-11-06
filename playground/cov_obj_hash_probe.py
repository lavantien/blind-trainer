import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO))
import bt

lane = REPO / "artifacts" / "lane" / "lane-cov-verify"
cov = json.loads((lane / "config-cov.json").read_text(encoding="utf-8"))
print("cflags:", cov["build"]["cflags"])
print("hash plain:", bt.objects_dirname(cov, cuda=False, coverage=False))
print("hash coverage:", bt.objects_dirname(cov, cuda=False, coverage=True))
obj_root = Path(cov["build"]["dirs"]["obj_cov"]) / bt.objects_dirname(
    cov, cuda=False, coverage=False
)
print("obj root:", obj_root, obj_root.is_dir())
exe = Path(cov["build"]["dirs"]["bin_cov"]) / "visual-diff.exe"
print("exe:", exe, exe.stat().st_mtime if exe.is_file() else "missing")
src = REPO / "tools" / "visual-diff" / "fs.c"
obj = bt.object_path(obj_root, REPO, src)
print("fs.c obj:", obj, obj.stat().st_mtime if obj.is_file() else "missing")
