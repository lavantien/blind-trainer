import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

import bt


def sample_hub():
    with open(bt.REPO_ROOT / "config" / "config.json", encoding="utf-8") as f:
        return json.load(f)


def entries_from(root, rels):
    return [
        (root / rel, root / "build" / "obj" / (rel.replace("/", "_") + ".obj"))
        for rel in rels
    ]


class SrcObjectExclusionTests(unittest.TestCase):
    def test_src_main_is_excluded_from_shared_link_objects(self):
        root = bt.REPO_ROOT
        entries = entries_from(root, ["src/core/alg.c", "src/app/main.c"])
        objects = [
            obj
            for source, obj in entries
            if source.is_relative_to(root / "src") and source.name != "main.c"
        ]
        self.assertEqual(len(objects), 1)
        self.assertNotIn("main.c", objects[0].name)


class RaylibSrcPairTests(unittest.TestCase):
    def test_app_area_source_yields_the_pair(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            lib = root / "build" / "vendor" / "raylib.lib"
            lib.parent.mkdir(parents=True)
            lib.write_bytes(b"")
            pair = bt.raylib_src_pair(
                sample_hub(), root, [root / "src" / "app" / "ui.c"]
            )
            self.assertEqual(pair[0], lib)
            self.assertEqual(pair[1], ["opengl32", "gdi32", "user32", "shell32", "winmm"])

    def test_other_area_source_yields_none(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.assertIsNone(
                bt.raylib_src_pair(sample_hub(), root, [root / "src" / "core" / "alg.c"])
            )

    def test_app_source_without_the_lib_raises(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            with self.assertRaises(bt.BtError):
                bt.raylib_src_pair(
                    sample_hub(), root, [root / "src" / "app" / "ui.c"]
                )

    def test_empty_src_areas_config_yields_none(self):
        hub = sample_hub()
        hub["build"]["raylib_build"]["src_areas"] = []
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.assertIsNone(
                bt.raylib_src_pair(hub, root, [root / "src" / "app" / "ui.c"])
            )


class AppExeLinkTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.bin_dir = self.root / "build" / "bin"
        self.entries = entries_from(
            self.root, ["src/core/alg.c", "src/app/ui.c", "src/app/main.c"]
        )
        self.objs_by_source = dict(self.entries)
        self.src_objects = [
            obj
            for source, obj in self.entries
            if source.is_relative_to(self.root / "src") and source.name != "main.c"
        ]

    def tearDown(self):
        self.tmp.cleanup()

    def test_app_exe_links_shared_objects_plus_its_main_and_raylib(self):
        lib = self.root / "build" / "vendor" / "raylib.lib"
        lib.parent.mkdir(parents=True)
        lib.write_bytes(b"")
        src_lib = bt.raylib_src_pair(
            sample_hub(),
            self.root,
            [s for s, _ in self.entries],
        )
        exe, objects, libs = bt.app_exe_link(
            sample_hub(),
            self.root,
            self.bin_dir,
            self.objs_by_source,
            self.src_objects,
            [],
            src_lib,
        )
        self.assertEqual(exe, self.bin_dir / "bt-app.exe")
        self.assertEqual(objects[-1], lib)
        self.assertIn(self.objs_by_source[self.root / "src" / "app" / "main.c"], objects)
        self.assertIn(self.objs_by_source[self.root / "src" / "app" / "ui.c"], objects)
        self.assertEqual(len(objects), 4)
        self.assertEqual(libs, ("opengl32", "gdi32", "user32", "shell32", "winmm"))

    def test_missing_app_main_yields_none(self):
        self.assertIsNone(
            bt.app_exe_link(
                sample_hub(),
                self.root,
                self.bin_dir,
                {},
                self.src_objects,
                [],
                None,
            )
        )


class ToolLinkFilterTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        for rel in (
            "src/common/x.c",
            "vendor/sqlite/sqlite3.c",
            "tools/release-verify/main.c",
            "tools/verify/main.c",
        ):
            target = self.root / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text("int t;\n", encoding="utf-8")
        self.stub = self.root / "clang_stub.py"
        self.stub.write_text(
            "import sys\n"
            "from pathlib import Path\n"
            "argv = sys.argv[1:]\n"
            "if '-o' in argv:\n"
            "    out = Path(argv[argv.index('-o') + 1])\n"
            "    out.parent.mkdir(parents=True, exist_ok=True)\n"
            "    out.write_bytes(b'stub')\n",
            encoding="utf-8",
        )

    def tearDown(self):
        self.tmp.cleanup()

    def compiler(self):
        return [sys.executable, "-I", str(self.stub)]

    def linked(self, only):
        hub = sample_hub()
        generated = bt.generate_stage(hub, self.root)
        obj_root, entries = bt.compile_stage(
            hub, self.root, self.compiler(), bt.Logger(), extra=generated
        )
        bt.link_stage(
            hub, self.root, self.compiler(), bt.Logger(), obj_root, entries, only=only
        )
        bin_dir = self.root / hub["build"]["dirs"]["bin"]
        return sorted(path.name for path in bin_dir.glob("*.exe"))

    def test_short_release_filter_relinks_release_verify(self):
        self.assertIn("release-verify.exe", self.linked("release"))

    def test_underscore_filter_relinks_release_verify(self):
        self.assertIn("release-verify.exe", self.linked("release_verify"))

    def test_verify_filter_links_verify_without_release_verify(self):
        names = self.linked("verify")
        self.assertIn("verify.exe", names)
        self.assertNotIn("release-verify.exe", names)
