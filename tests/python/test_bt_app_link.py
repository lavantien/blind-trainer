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
