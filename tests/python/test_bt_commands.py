import json
import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

import bt


def sample_hub():
    with open(bt.REPO_ROOT / "config" / "config.json", encoding="utf-8") as f:
        hub = json.load(f)
    return hub


class CompileCommandTests(unittest.TestCase):
    def test_compile_command_carries_target_std_cflags_release_and_includes(self):
        hub = sample_hub()
        argv = bt.compile_command(
            ["clang"],
            bt.compile_flags(hub),
            [bt.REPO_ROOT / "src"],
            bt.REPO_ROOT / "src/common/rng.c",
            bt.REPO_ROOT / "build/obj/src/common/rng.obj",
        )
        self.assertEqual(argv[0], "clang")
        self.assertIn("-target", argv)
        self.assertIn(hub["build"]["target"], argv)
        self.assertIn(f"-std={hub['build']['std']}", argv)
        for flag in hub["build"]["cflags"] + hub["build"]["release_flags"]:
            self.assertIn(flag, argv)
        self.assertIn("-ffp-contract=off", argv)
        self.assertIn("-g0", argv)
        self.assertEqual(argv.count("-I"), 1)
        self.assertEqual(
            argv[argv.index("-c") + 1], str(bt.REPO_ROOT / "src/common/rng.c")
        )
        self.assertEqual(
            argv[argv.index("-o") + 1],
            str(bt.REPO_ROOT / "build/obj/src/common/rng.obj"),
        )

    def test_link_command_routes_link_flags_through_xlinker(self):
        hub = sample_hub()
        argv = bt.link_command(
            ["clang"],
            bt.compile_flags(hub),
            [bt.REPO_ROOT / "build/obj/a.obj", bt.REPO_ROOT / "build/obj/b.obj"],
            ["kernel32"],
            bt.REPO_ROOT / "build/bin/app.exe",
        )
        self.assertIn("-fuse-ld=lld-link", argv)
        for flag in hub["build"]["link_flags"]:
            self.assertEqual(argv[argv.index("-Xlinker") + 1], flag)
            argv.remove("-Xlinker")
            argv.remove(flag)
        self.assertIn(str(bt.REPO_ROOT / "build/obj/a.obj"), argv)
        self.assertIn("-lkernel32", argv)
        self.assertEqual(argv[-1], str(bt.REPO_ROOT / "build/bin/app.exe"))


class ObjectPathTests(unittest.TestCase):
    def test_object_path_mirrors_source_tree_under_obj_dir(self):
        obj = bt.object_path(
            bt.REPO_ROOT / "build/obj",
            bt.REPO_ROOT,
            bt.REPO_ROOT / "src/common/json.c",
        )
        self.assertEqual(obj, bt.REPO_ROOT / "build/obj/src/common/json.obj")

    def test_exe_name_joins_area_parts_with_underscores(self):
        suite = {"area": "common", "stem": "rng_test"}
        self.assertEqual(bt.suite_exe_name(suite), "common_rng_test")

    def test_objects_dir_changes_when_flags_change(self):
        hub = sample_hub()
        first = bt.objects_dirname(hub)
        self.assertEqual(first, bt.objects_dirname(hub))
        hub["build"]["cflags"] = [*hub["build"]["cflags"], "-DEXTRA"]
        self.assertNotEqual(first, bt.objects_dirname(hub))


class StubCompilerTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.stub = self.root / "stub.py"
        self.stub.write_text(
            "import sys\n"
            "from pathlib import Path\n"
            "argv = sys.argv[1:]\n"
            "if '-Z' in argv:\n"
            "    sys.exit(3)\n"
            "if '-o' in argv:\n"
            "    out = Path(argv[argv.index('-o') + 1])\n"
            "    out.parent.mkdir(parents=True, exist_ok=True)\n"
            "    out.write_bytes(b'stub')\n",
            encoding="utf-8",
        )

    def tearDown(self):
        self.tmp.cleanup()

    def test_run_units_executes_stubs_and_collects_returncodes(self):
        objs = [self.root / "o1.obj", self.root / "o2.obj"]
        units = [
            (
                f"compile {obj.name}",
                [sys.executable, "-I", str(self.stub), "-c", "x.c", "-o", str(obj)],
            )
            for obj in objs
        ]
        results = bt.run_units(units, jobs=1)
        self.assertEqual([r.returncode for r in results], [0, 0])
        for obj in objs:
            self.assertTrue(obj.is_file())

    def test_run_units_surfaces_nonzero_exit_codes(self):
        units = [("boom", [sys.executable, "-I", str(self.stub), "-Z"])]
        results = bt.run_units(units, jobs=1)
        self.assertEqual(results[0].returncode, 3)


class AggregateTests(unittest.TestCase):
    def test_aggregate_passes_when_all_exes_pass(self):
        results = [
            bt.UnitResult("a", 0, ""),
            bt.UnitResult("b", 0, ""),
        ]
        code, line = bt.aggregate_results(results)
        self.assertEqual(code, 0)
        self.assertIn("2 passed", line)
        self.assertIn("0 failed", line)

    def test_aggregate_fails_nonzero_on_any_failure(self):
        results = [
            bt.UnitResult("a", 0, ""),
            bt.UnitResult("b", 1, "check failed"),
        ]
        code, line = bt.aggregate_results(results)
        self.assertEqual(code, 1)
        self.assertIn("1 failed", line)


class HubTests(unittest.TestCase):
    def test_hub_exposes_build_and_trace_sections(self):
        hub = sample_hub()
        self.assertEqual(hub["build"]["target"], "x86_64-pc-windows-msvc")
        self.assertEqual(hub["build"]["std"], "c23")
        self.assertGreater(hub["trace"]["ring_bytes"], 0)

    def test_bt_config_env_override_selects_the_hub(self):
        with tempfile.TemporaryDirectory() as tmp:
            alt = Path(tmp) / "alt.json"
            alt.write_text(json.dumps({"marker": 17}), encoding="utf-8")
            old = os.environ.get("BT_CONFIG")
            os.environ["BT_CONFIG"] = str(alt)
            try:
                hub = bt.load_hub()
                self.assertEqual(hub["marker"], 17)
            finally:
                if old is None:
                    os.environ.pop("BT_CONFIG", None)
                else:
                    os.environ["BT_CONFIG"] = old


if __name__ == "__main__":
    unittest.main()
