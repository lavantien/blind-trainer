import json
import os
import re
import sys
import tempfile
import types
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

import bt


def sample_hub():
    with open(bt.REPO_ROOT / "config" / "config.json", encoding="utf-8") as f:
        return json.load(f)


class CoverageFlagTests(unittest.TestCase):
    def test_hub_carries_the_coverage_section(self):
        hub = sample_hub()
        cov = hub["coverage"]
        self.assertEqual(
            cov["flags"], ["-fprofile-instr-generate", "-fcoverage-mapping"]
        )
        for key in ("profiles_dir", "profdata", "export", "include_regex"):
            self.assertIsInstance(cov[key], str)
            self.assertTrue(cov[key])

    def test_coverage_compile_flags_extend_the_base_with_both_flags(self):
        hub = sample_hub()
        base = bt.compile_flags(hub)
        cov = bt.coverage_compile_flags(hub)
        for flag in base:
            self.assertIn(flag, cov)
        for flag in hub["coverage"]["flags"]:
            self.assertIn(flag, cov)
            self.assertNotIn(flag, base)

    def test_coverage_dirs_are_separate_from_the_plain_build(self):
        hub = sample_hub()
        dirs = hub["build"]["dirs"]
        self.assertNotEqual(dirs["obj_cov"], dirs["obj"])
        self.assertNotEqual(dirs["bin_cov"], dirs["bin"])

    def test_objects_dirname_splits_between_plain_and_coverage(self):
        hub = sample_hub()
        plain = bt.objects_dirname(hub)
        self.assertEqual(plain, bt.objects_dirname(hub, coverage=False))
        self.assertNotEqual(plain, bt.objects_dirname(hub, coverage=True))
        hub["coverage"]["flags"] = [*hub["coverage"]["flags"], "-DEXTRA"]
        self.assertNotEqual(
            bt.objects_dirname(hub, coverage=True), plain
        )

    def test_coverage_paths_live_under_artifacts_coverage(self):
        hub = sample_hub()
        paths = bt.coverage_paths(hub)
        expected_root = (
            bt.REPO_ROOT / hub["paths"]["artifacts"] / "coverage"
        )
        self.assertEqual(set(paths), {"root", "profiles", "profdata", "export"})
        self.assertEqual(paths["root"], expected_root)
        self.assertEqual(
            paths["profiles"], expected_root / hub["coverage"]["profiles_dir"]
        )
        self.assertEqual(paths["profdata"], expected_root / hub["coverage"]["profdata"])
        self.assertEqual(paths["export"], expected_root / hub["coverage"]["export"])
        self.assertNotEqual(paths["profdata"], paths["export"])


class CoverageScopeTests(unittest.TestCase):
    def repo_sources(self):
        found = []
        for base in ("src", "tools", "tests/fuzz", "tests/unit"):
            for path in sorted((bt.REPO_ROOT / base).rglob("*")):
                if path.is_file() and path.suffix in (".c", ".h"):
                    found.append(path)
        return found

    def in_scope(self, path):
        rel = path.relative_to(bt.REPO_ROOT).as_posix()
        return rel.startswith(("src/", "tools/", "tests/fuzz/"))

    def test_include_regex_template_anchors_on_the_repo_root(self):
        self.assertIn("{root}", sample_hub()["coverage"]["include_regex"])

    def test_include_regex_admits_exactly_the_measured_sources(self):
        pattern = bt.coverage_include_regex(sample_hub())
        for path in self.repo_sources():
            rel = path.relative_to(bt.REPO_ROOT).as_posix()
            with self.subTest(path=rel):
                self.assertEqual(bool(re.search(pattern, str(path))), self.in_scope(path))

    def test_include_regex_rejects_foreign_roots_and_unit_paths(self):
        pattern = bt.coverage_include_regex(sample_hub())
        foreign = bt.REPO_ROOT.parent / "other" / "tools" / "main.c"
        self.assertIsNone(re.search(pattern, str(foreign)))
        unit_tool = bt.REPO_ROOT / "tests" / "unit" / "tools" / "x_test.c"
        self.assertIsNone(re.search(pattern, str(unit_tool)))
        forward = str(bt.REPO_ROOT / "tools" / "x.c").replace("\\", "/")
        self.assertIsNotNone(re.search(pattern, forward))


class CoverageCommandTests(unittest.TestCase):
    def test_profdata_merge_argv_uses_output_and_positions_profraws(self):
        raws = [Path("a.profraw"), Path("b.profraw")]
        out = Path("merged.profdata")
        argv = bt.profdata_merge_argv(Path("llvm-profdata.exe"), raws, out)
        self.assertEqual(argv[0], "llvm-profdata.exe")
        self.assertEqual(argv[1], "merge")
        self.assertIn(f"--output={out}", argv)
        self.assertEqual(argv[-2:], [str(raws[0]), str(raws[1])])

    def test_cov_export_argv_leads_with_primary_and_objects_after(self):
        hub = sample_hub()
        exes = [Path("bin/one.exe"), Path("bin/two.exe"), Path("bin/three.exe")]
        prof = Path("merged.profdata")
        argv = bt.cov_export_argv(hub, Path("llvm-cov.exe"), exes, prof)
        self.assertEqual(argv[0], "llvm-cov.exe")
        self.assertEqual(argv[1], "export")
        self.assertIn(f"--instr-profile={prof}", argv)
        self.assertIn(
            f"--include-filename-regex={bt.coverage_include_regex(hub)}", argv
        )
        self.assertEqual(
            argv[argv.index(f"--instr-profile={prof}") + 2], str(exes[0])
        )
        for exe in exes[1:]:
            self.assertIn(f"--object={exe}", argv)
        self.assertNotIn(f"--object={exes[0]}", argv)

    def test_coverage_export_exes_feed_every_instrumented_exe_to_llvm_cov(self):
        hub = sample_hub()
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            bin_cov = root / hub["build"]["dirs"]["bin_cov"]
            bin_cov.mkdir(parents=True)
            suites = [bin_cov / "core_cube_test.exe", bin_cov / "tools_sleeper_test.exe"]
            extras = [
                bin_cov / "mutants-gate.exe",
                bin_cov / "mutate.exe",
                bin_cov / hub["build"]["app_exe"],
            ]
            for exe in [*suites, *extras, bin_cov / "notes.txt"]:
                exe.write_bytes(b"x")
            objects = bt.coverage_export_exes(hub, root, suites)
            self.assertEqual(objects[: len(suites)], suites)
            self.assertEqual(objects[len(suites) :], sorted(extras))
            self.assertEqual(len(objects), len(set(objects)))
            for exe in extras:
                self.assertIn(f"--object={exe}", bt.cov_export_argv(hub, Path("llvm-cov.exe"), objects, Path("m.profdata")))

    def test_coverage_badge_argv_runs_the_script_isolated_with_the_export(self):
        hub = sample_hub()
        export = Path("artifacts/coverage/coverage.json")
        argv = bt.coverage_badge_argv(hub, export)
        self.assertEqual(argv[1], "-I")
        self.assertTrue(
            str(argv[2]).replace("\\", "/").endswith("scripts/coverage_badge.py")
        )
        self.assertEqual(argv[3], str(export))
        self.assertEqual(
            argv[0],
            hub["build"]["tools"].get("python", "") or sys.executable,
        )


class CoverageStageTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        (self.root / "src" / "common").mkdir(parents=True)
        (self.root / "src" / "common" / "x.c").write_text("int x;\n", encoding="utf-8")
        (self.root / "vendor" / "sqlite").mkdir(parents=True)
        (self.root / "vendor" / "sqlite" / "sqlite3.c").write_text(
            "int sq;\n", encoding="utf-8"
        )
        suite = self.root / "tests" / "unit" / "common" / "x_test.c"
        suite.parent.mkdir(parents=True)
        suite.write_text(
            "#include \"common/bt_harness.h\"\n"
            "BT_TEST(group, one)\n"
            "BT_TEST(group, two)\n",
            encoding="utf-8",
        )
        lib = self.root / "build" / "vendor" / "raylib.lib"
        lib.parent.mkdir(parents=True)
        lib.write_bytes(b"")
        self.log = self.root / "argv.log"
        self.stub = self.root / "clang_stub.py"
        self.stub.write_text(
            "import sys\n"
            "from pathlib import Path\n"
            "argv = sys.argv[1:]\n"
            "with open(r'%s', 'a', encoding='utf-8') as log:\n"
            "    log.write(repr(argv) + '\\n')\n"
            "if '-o' in argv:\n"
            "    out = Path(argv[argv.index('-o') + 1])\n"
            "    out.parent.mkdir(parents=True, exist_ok=True)\n"
            "    out.write_bytes(b'stub')\n" % str(self.log),
            encoding="utf-8",
        )

    def tearDown(self):
        self.tmp.cleanup()

    def compiler(self):
        return [sys.executable, "-I", str(self.stub)]

    def argv_lines(self):
        if not self.log.is_file():
            return []
        return [line for line in self.log.read_text(encoding="utf-8").splitlines() if line]

    def test_compile_stage_coverage_carries_flags_and_isolates_the_obj_base(self):
        hub = sample_hub()
        plain_marker = self.root / "build" / "obj" / "plainkeep"
        plain_marker.mkdir(parents=True)
        (plain_marker / "keep.obj").write_bytes(b"keep")
        obj_root, entries = bt.compile_stage(
            hub, self.root, self.compiler(), bt.Logger(), coverage=True
        )
        lines = self.argv_lines()
        self.assertTrue(lines)
        for flag in hub["coverage"]["flags"]:
            self.assertTrue(any(flag in line for line in lines), flag)
        self.assertIn("obj-cov", str(obj_root))
        self.assertTrue(
            (obj_root / "src" / "common" / "x.obj").is_file()
        )
        self.assertTrue((plain_marker / "keep.obj").is_file())
        plain_root, _ = bt.compile_stage(hub, self.root, self.compiler(), bt.Logger())
        self.assertIn("obj-cov", str(obj_root))
        self.assertNotEqual(plain_root, obj_root)
        self.assertTrue(
            (obj_root / "src" / "common" / "x.obj").is_file()
        )

    def test_link_stage_coverage_uses_bin_cov_and_its_own_stamp(self):
        hub = sample_hub()
        generated = bt.generate_stage(hub, self.root, coverage=True)
        obj_root, entries = bt.compile_stage(
            hub,
            self.root,
            self.compiler(),
            bt.Logger(),
            extra=generated,
            coverage=True,
        )
        bt.link_stage(
            hub, self.root, self.compiler(), bt.Logger(), obj_root, entries,
            coverage=True
        )
        bin_cov = self.root / hub["build"]["dirs"]["bin_cov"]
        self.assertTrue((bin_cov / "common_x_test.exe").is_file())
        link_lines = [
            line
            for line in self.argv_lines()
            if "common_x_test.exe" in line and "-fuse-ld=lld-link" in line
        ]
        self.assertEqual(len(link_lines), 1)
        for flag in hub["coverage"]["flags"]:
            self.assertIn(flag, link_lines[0])
        gen = self.root / hub["build"]["dirs"]["gen"]
        self.assertTrue((gen / "link_stamp_cov.txt").is_file())
        self.assertFalse((gen / "link_stamp.txt").is_file())


class CoverageRunTests(unittest.TestCase):
    def fake_suites(self):
        return [
            {"exe_name": "common_rng_test"},
            {"exe_name": "core_cube_test"},
        ]

    def test_coverage_suite_units_point_each_exe_at_its_own_profile(self):
        hub = sample_hub()
        profile_dir = bt.REPO_ROOT / "scratch" / "profiles"
        units = bt.coverage_suite_units(hub, self.fake_suites(), profile_dir)
        self.assertEqual(len(units), 2)
        seen = set()
        for label, argv, env in units:
            exe = Path(argv[0])
            self.assertEqual(label, exe.stem)
            bin_cov = bt.REPO_ROOT / hub["build"]["dirs"]["bin_cov"]
            self.assertEqual(exe.parent, bin_cov)
            raw = Path(env["LLVM_PROFILE_FILE"])
            self.assertEqual(raw.parent, profile_dir)
            self.assertEqual(raw.name, f"{exe.stem}.%p.profraw")
            self.assertNotIn(str(raw), seen)
            seen.add(str(raw))
            self.assertEqual(env["BT_BIN_DIR"], str(bin_cov))
            self.assertIn("BT_CONFIG", env)

    def test_run_units_env_runs_each_stub_with_its_own_env(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            stub = root / "suite_stub.py"
            stub.write_text(
                "import os\n"
                "from pathlib import Path\n"
                "raw = os.environ.get('LLVM_PROFILE_FILE')\n"
                "if not raw:\n"
                "    raise SystemExit(9)\n"
                "out = Path(raw)\n"
                "out.parent.mkdir(parents=True, exist_ok=True)\n"
                "out.write_bytes(b'raw')\n",
                encoding="utf-8",
            )
            units = []
            for name in ("first", "second"):
                raw = root / name / f"{name}.profraw"
                env = {**os.environ, "LLVM_PROFILE_FILE": str(raw)}
                units.append((name, [sys.executable, "-I", str(stub)], env))
            results = bt.run_units_env(units, jobs=2)
            self.assertEqual([r.returncode for r in results], [0, 0])
            for name in ("first", "second"):
                self.assertEqual((root / name / f"{name}.profraw").read_bytes(), b"raw")

    def test_run_units_env_surfaces_nonzero_exit_codes(self):
        with tempfile.TemporaryDirectory() as tmp:
            stub = Path(tmp) / "dead.py"
            stub.write_text("raise SystemExit(7)\n", encoding="utf-8")
            results = bt.run_units_env(
                [("dead", [sys.executable, "-I", str(stub)], None)], jobs=1
            )
            self.assertEqual(results[0].label, "dead")
            self.assertEqual(results[0].returncode, 7)


class CoverageArtifactTests(unittest.TestCase):
    def test_write_coverage_export_requires_payload(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "cov.json"
            for empty in ("", "   \n"):
                with self.assertRaises(bt.BtError) as caught:
                    bt.write_coverage_export(empty, out)
                self.assertIn(str(out), str(caught.exception))
            bt.write_coverage_export('{"data": []}  \n', out)
            self.assertEqual(out.read_text(encoding="utf-8"), '{"data": []}\n')

    def test_coverage_percent_parses_the_badge_line_and_fails_loud(self):
        noisy = "tool out\ncoverage regions: 87.3%\ndone"
        self.assertEqual(bt.coverage_percent(noisy), 87.3)
        self.assertEqual(bt.coverage_percent("coverage regions: 100%"), 100.0)
        with self.assertRaises(bt.BtError):
            bt.coverage_percent("no line here")

    def test_coverage_scrub_removes_the_profiles_dir_and_profdata(self):
        with tempfile.TemporaryDirectory() as tmp:
            profiles = Path(tmp) / "profiles"
            profiles.mkdir()
            (profiles / "a.profraw").write_bytes(b"raw")
            profdata = Path(tmp) / "merged.profdata"
            profdata.write_bytes(b"prof")
            bt.coverage_scrub(profiles, profdata)
            self.assertFalse(profiles.exists())
            self.assertFalse(profdata.exists())

    def test_coverage_target_is_registered_with_a_docstring(self):
        names = dict(bt.target_functions(bt))
        self.assertIn("coverage", names)
        self.assertTrue(bt.target_doc(names["coverage"]))


class CoverageTreeGuardTests(unittest.TestCase):
    def hub_with_guard(self):
        hub = sample_hub()
        hub["coverage"]["guard_paths"] = ["src", "tools"]
        return hub

    def git(self, argv, label, env=None):
        bt.run(argv, label, env=env)

    def commit_env(self):
        return {
            **os.environ,
            "GIT_AUTHOR_NAME": "t86",
            "GIT_AUTHOR_EMAIL": "t86@example.com",
            "GIT_COMMITTER_NAME": "t86",
            "GIT_COMMITTER_EMAIL": "t86@example.com",
        }

    def test_guard_paths_cover_every_measured_area(self):
        self.assertEqual(
            set(sample_hub()["coverage"]["guard_paths"]), {"src", "tools", "tests/fuzz"}
        )

    def test_measured_dirty_paths_names_files_under_the_guard_paths(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.git(["git", "init", "-q", str(root)], "guard init")
            (root / "src").mkdir()
            (root / "src" / "a.c").write_text("int a;\n", encoding="utf-8")
            (root / "elsewhere").mkdir()
            (root / "elsewhere" / "b.c").write_text("int b;\n", encoding="utf-8")
            self.git(["git", "-C", str(root), "add", "."], "guard add")
            self.git(
                ["git", "-C", str(root), "commit", "-q", "-m", "chore: guard probe"],
                "guard commit",
                env=self.commit_env(),
            )
            self.assertEqual(bt.measured_dirty_paths(self.hub_with_guard(), root), [])
            (root / "src" / "a.c").write_text("int b;\n", encoding="utf-8")
            (root / "tools").mkdir()
            (root / "tools" / "new.c").write_text("int c;\n", encoding="utf-8")
            (root / "elsewhere" / "b.c").write_text("int z;\n", encoding="utf-8")
            dirty = bt.measured_dirty_paths(self.hub_with_guard(), root)
            self.assertIn("src/a.c", dirty)
            self.assertIn("tools/new.c", dirty)
            self.assertNotIn("elsewhere/b.c", dirty)

    def test_failed_git_status_fails_closed(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(bt.BtError):
                bt.measured_dirty_paths(self.hub_with_guard(), Path(tmp))


class BadgeFailureTests(unittest.TestCase):
    def test_proc_tail_joins_stderr_then_stdout_clamped_and_stripped(self):
        proc = types.SimpleNamespace(stderr="  err \n", stdout="out\n")
        self.assertEqual(bt.proc_tail(proc), "err \nout")
        noisy = types.SimpleNamespace(stderr="e" * 4000, stdout="o" * 10)
        self.assertEqual(bt.proc_tail(noisy), ("e" * 2990) + ("o" * 10))

    def test_badge_failure_carries_the_code_and_the_captured_tail(self):
        proc = types.SimpleNamespace(
            returncode=3,
            stderr="[fail] coverage-badge: llvm-cov export invalid: data array is empty\n",
            stdout="",
        )
        error = bt.badge_failure(proc)
        self.assertIsInstance(error, bt.BtError)
        self.assertIn("coverage badge failed with code 3", str(error))
        self.assertIn("data array is empty", str(error))


if __name__ == "__main__":
    unittest.main()
