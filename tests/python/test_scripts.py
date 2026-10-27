import json
import os
import sys
import tempfile
import time
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))

import clean_rust
import coverage_badge
import alg_coverage_badge
import dist
import fetch_docs
import fetch_fonts
import memguard
import winproc

MB = 1024 * 1024
GB = 1024 * 1024 * 1024


class MemguardLogicTests(unittest.TestCase):
    def test_mb_format_matches_pwsh_f1(self):
        self.assertEqual(memguard.fmt_mb(1.5 * MB), "1.5")
        self.assertEqual(memguard.fmt_mb(0 * MB), "0.0")
        self.assertEqual(memguard.fmt_mb(2097152), "2.0")

    def test_repo_prefix_is_forward_slashed_lowercase_with_trailing_slash(self):
        self.assertEqual(
            memguard.norm_repo_prefix("C:\\Repo\\Blind.Trainer"),
            "c:/repo/blind.trainer/",
        )

    def test_repo_path_match_is_prefix_case_insensitive(self):
        repo = memguard.norm_repo_prefix("C:\\Users\\lav\\dev\\blind-trainer")
        self.assertTrue(memguard.is_repo_path("C:\\Users\\lav\\dev\\blind-trainer\\build\\bin\\x.exe", repo))
        self.assertTrue(memguard.is_repo_path("c:/users/lav/dev/blind-trainer/target/x", repo))
        self.assertFalse(memguard.is_repo_path("C:\\Users\\lav\\dev\\blind-trainer-2\\x.exe", repo))
        self.assertFalse(memguard.is_repo_path("D:\\elsewhere\\x.exe", repo))

    def test_pid_file_parse_accepts_only_live_stamped_foreign_pids(self):
        lookup = {11: 111, 22: 222}
        self.assertEqual(memguard.parse_pid_file("11 111", 5, lookup.get), 11)
        self.assertIsNone(memguard.parse_pid_file("11 999", 5, lookup.get))
        self.assertIsNone(memguard.parse_pid_file("11", 5, lookup.get))
        self.assertIsNone(memguard.parse_pid_file("abc", 5, lookup.get))
        self.assertIsNone(memguard.parse_pid_file("-5 0", 5, lookup.get))
        self.assertIsNone(memguard.parse_pid_file("0 0", 5, lookup.get))
        self.assertIsNone(memguard.parse_pid_file("99999999999999999999 0", 5, lookup.get))
        self.assertIsNone(memguard.parse_pid_file("33 333", 5, lookup.get))

    def test_pid_file_parse_rejects_own_pid(self):
        self.assertIsNone(memguard.parse_pid_file("5 55", 5, {5: 55}.get))

    def test_stamp_line_carries_pid_and_ticks(self):
        self.assertEqual(memguard.stamp_line(12, 345), "12 345")

    def test_filetime_to_dotnet_ticks_matches_net_epoch_math(self):
        self.assertEqual(winproc.filetime_to_dotnet_ticks(0, 0), 504911232000000000)
        self.assertEqual(
            winproc.filetime_to_dotnet_ticks(0x28000000, 0x01D00000),
            (0x01D0000002800000) + 504911232000000000,
        )

    def test_monitor_ok_line_and_code(self):
        rows = [{"id": 1, "name": "a", "ws": MB}]
        result = memguard.monitor(rows, 2 * MB)
        self.assertEqual(result["code"], 0)
        self.assertEqual(result["lines"], ["memguard: 1 procs 1.0 MB / 2.0 MB cap ok"])

    def test_monitor_breach_without_kill_reports_largest(self):
        rows = [
            {"id": 4, "name": "big", "ws": 9 * MB},
            {"id": 2, "name": "small", "ws": MB},
        ]
        result = memguard.monitor(rows, 2 * MB)
        self.assertEqual(result["code"], 3)
        self.assertEqual(
            result["lines"],
            ["memguard: 2 procs 10.0 MB / 2.0 MB cap breach, largest pid 4 big 9.0 MB"],
        )

    def test_monitor_kill_failure_lands_on_five(self):
        rows = [{"id": 7, "name": "z", "ws": 4 * MB}]
        result = memguard.monitor(rows, MB, kill=True, killer=lambda pid: True, waiter=lambda pid: False)
        self.assertEqual(result["code"], 5)
        self.assertTrue(any("kill failed" in line for line in result["lines"]))

    def test_monitor_kill_success_still_reports_breach(self):
        rows = [{"id": 7, "name": "z", "ws": 4 * MB}]
        result = memguard.monitor(rows, MB, kill=True, killer=lambda pid: True, waiter=lambda pid: True)
        self.assertEqual(result["code"], 3)
        self.assertTrue(any("killed pid 7 z, verified gone" in line for line in result["lines"]))

    def test_wrap_split_honors_quotes(self):
        self.assertEqual(memguard.split_wrap_command('a "b c" d'), ["a", "b c", "d"])
        self.assertEqual(memguard.split_wrap_command("  only  "), ["only"])

    def test_cap_resolution_prefers_positive_override(self):
        self.assertEqual(memguard.resolve_cap(5, 9), 5)
        self.assertEqual(memguard.resolve_cap(0, 9), 9)
        self.assertEqual(memguard.resolve_cap(-1, 9), "-CapBytes must be a positive byte count")


class CleanRustLogicTests(unittest.TestCase):
    def test_gb_format_matches_pwsh_f2(self):
        self.assertEqual(clean_rust.fmt_gb(2 * GB), "2.00")
        self.assertEqual(clean_rust.fmt_gb(0), "0.00")

    def test_stale_selection_picks_only_aged_scratch_names(self):
        with tempfile.TemporaryDirectory() as tmp:
            stale = Path(tmp) / "cargo-mutants-stale"
            fresh = Path(tmp) / "cargo-mutants-fresh"
            bt_stale = Path(tmp) / "bt-stale"
            other = Path(tmp) / "unrelated"
            for path in (stale, fresh, bt_stale, other):
                path.mkdir()
                (path / "a").write_text("x")
            old = time.time() - 8 * 3600
            os.utime(stale, (old, old))
            os.utime(bt_stale, (old, old))
            picked = clean_rust.stale_scratches(tmp, 6)
            self.assertEqual(sorted(picked), sorted([str(stale), str(bt_stale)]))

    def test_orphan_selection_uses_creation_time_vs_oldest_live_start(self):
        now = datetime.now()
        entries = [
            ("/t/bt-old", now - timedelta(hours=3)),
            ("/t/bt-new", now - timedelta(minutes=1)),
        ]
        picked = clean_rust.orphan_scratches(entries, [now - timedelta(hours=1)])
        self.assertEqual(picked, ["/t/bt-old"])
        self.assertEqual(clean_rust.orphan_scratches(entries, []), ["/t/bt-old", "/t/bt-new"])

    def test_dir_bytes_sums_files_recursively(self):
        with tempfile.TemporaryDirectory() as tmp:
            (Path(tmp) / "sub").mkdir()
            (Path(tmp) / "a").write_text("x" * 10)
            (Path(tmp) / "sub" / "b").write_text("y" * 5)
            self.assertEqual(clean_rust.dir_bytes(tmp), 15)
        self.assertEqual(clean_rust.dir_bytes("Z:\\definitely\\missing"), 0)

    def test_remove_tree_and_incremental(self):
        with tempfile.TemporaryDirectory() as tmp:
            inc = Path(tmp) / "target" / "debug" / "incremental"
            inc.mkdir(parents=True)
            (inc / "cache.bin").write_text("x")
            clean_rust.remove_incremental(str(inc))
            self.assertFalse(inc.exists())
            tree = Path(tmp) / "t"
            tree.mkdir()
            (tree / "a").write_text("x")
            self.assertTrue(clean_rust.remove_tree(str(tree)))
            self.assertFalse(tree.exists())
            self.assertFalse(clean_rust.remove_tree(str(Path(tmp) / "nope")))
            self.assertTrue(Path(tmp).exists())


class DistLogicTests(unittest.TestCase):
    def test_staging_plan_lists_exe_fonts_config(self):
        with tempfile.TemporaryDirectory() as tmp:
            repo = Path(tmp)
            exe = repo / "build" / "bin" / "blind-trainer.exe"
            exe.parent.mkdir(parents=True)
            exe.write_text("exe")
            config = repo / "config" / "config.json"
            config.parent.mkdir()
            config.write_text("{}")
            font = repo / "assets" / "fonts" / "NotoSans-Regular.ttf"
            font.parent.mkdir(parents=True)
            font.write_text("font")
            release = {
                "staging_dir": "blind-trainer",
                "exe": "blind-trainer.exe",
                "config_file": "config/config.json",
                "fonts_dir": "assets/fonts",
            }
            plan = dist.staging_plan(repo, repo / "artifacts" / "dist", release)
            destinations = [str(dst).replace("\\", "/") for _, dst in plan]
            self.assertTrue(any(d.endswith("/blind-trainer/blind-trainer.exe") for d in destinations))
            self.assertTrue(any(d.endswith("/blind-trainer/config/config.json") for d in destinations))
            self.assertTrue(any(d.endswith("/blind-trainer/assets/fonts/NotoSans-Regular.ttf") for d in destinations))
            zip_path = dist.zip_path(repo / "artifacts" / "dist", "1.2.3")
            self.assertTrue(str(zip_path).replace("\\", "/").endswith("blind-trainer-1.2.3-windows-x64.zip"))

    def test_staging_plan_fails_when_exe_missing(self):
        with tempfile.TemporaryDirectory() as tmp:
            repo = Path(tmp)
            release = {"staging_dir": "s", "exe": "e.exe", "config_file": "config/config.json", "fonts_dir": "fonts"}
            with self.assertRaises(FileNotFoundError):
                dist.staging_plan(repo, repo / "out", release)


COV_EXPORT = {
    "data": [
        {
            "files": [
                {"filename": "src/core/alg.c", "summary": {"regions": {"count": 10, "covered": 5, "percent": 50.0}}},
                {"filename": "src/core/cube.c", "summary": {"regions": {"count": 10, "covered": 9, "percent": 90.0}}},
                {"filename": "tools/policy/lib.c", "summary": {"regions": {"count": 4, "covered": 2, "percent": 50.0}}},
                {"filename": "tests/fuzz/notation.c", "summary": {"regions": {"count": 2, "covered": 1, "percent": 50.0}}},
                {"filename": "vendor/x.c", "summary": {"regions": {"count": 8, "covered": 0, "percent": 0.0}}},
                {"filename": "README.md", "summary": {"regions": {"count": 0, "covered": 0, "percent": 0.0}}},
            ],
            "functions": [
                {"name": "dead_long_function_name_" + "x" * 80, "count": 0, "filenames": ["src/core/alg.c"]},
                {"name": "live_one", "count": 3, "filenames": ["src/core/alg.c"]},
                {"name": "dead_two", "count": 0, "filenames": ["tools/policy/lib.c"]},
            ],
            "totals": {"regions": {"count": 34, "covered": 17, "percent": 50.0}},
        }
    ]
}


class CoverageBadgeLogicTests(unittest.TestCase):
    def test_area_rollup_sums_regions(self):
        rows = coverage_badge.area_rows(COV_EXPORT["data"][0]["files"])
        by_area = {row["area"]: row for row in rows}
        self.assertEqual(by_area["core"]["covered"], 14)
        self.assertEqual(by_area["core"]["total"], 20)
        self.assertEqual(by_area["policy"]["covered"], 2)
        self.assertEqual(rows[0]["area"], "policy")

    def test_worst_files_rank_and_filter(self):
        worst = coverage_badge.worst_files(COV_EXPORT["data"][0]["files"], 2)
        self.assertEqual([Path(f["filename"]).name for f in worst], ["alg.c", "lib.c"])

    def test_dead_by_file_truncates_at_sixty(self):
        dead = coverage_badge.dead_by_file(COV_EXPORT["data"][0]["functions"])
        names = dead["src/core/alg.c"]
        self.assertEqual(len(names), 1)
        self.assertLessEqual(len(names[0]), 60)
        self.assertEqual(dead["tools/policy/lib.c"], ["dead_two"])

    def test_badge_svg_geometry_and_colors(self):
        svg = coverage_badge.badge_svg("coverage", "50.0%", 50.0)
        self.assertIn('fill="#bf8700"', svg)
        self.assertIn('width="122"', svg)
        self.assertIn("coverage: 50.0%", svg)
        self.assertIn('fill="#2ea043"', coverage_badge.badge_svg("coverage", "80.0%", 80.0))
        self.assertIn('fill="#cf222e"', coverage_badge.badge_svg("coverage", "10.0%", 10.0))

    def test_report_lines_carry_table_and_totals(self):
        lines = coverage_badge.report_lines(COV_EXPORT["data"][0], "2026-10-27T00:00:00Z")
        joined = "\n".join(lines)
        self.assertIn("regions: 17 of 34 (50.0%)", joined)
        self.assertIn("| policy | 2/4 | 50.0% |", joined)
        self.assertIn("| core | 14/20 | 70.0% |", joined)
        self.assertIn("worst 25 files:", joined)
        self.assertIn("uncovered:", joined)


class AlgCoverageBadgeLogicTests(unittest.TestCase):
    def test_compute_clamps_percent(self):
        manifest = {"cases": {"entries": 10, "edge": {"covered": 4}, "corner": {"covered": 3}}}
        self.assertEqual(alg_coverage_badge.compute(manifest), (70.0, 7, 10))
        hot = {"cases": {"entries": 1, "edge": {"covered": 2}, "corner": {"covered": 2}}}
        self.assertEqual(alg_coverage_badge.compute(hot), (100.0, 4, 1))

    def test_missing_manifest_error_names_gen_data(self):
        message = alg_coverage_badge.missing_manifest_error("some/manifest.json")
        self.assertIn("manifest missing: some/manifest.json", message)
        self.assertIn("gen-data", message)

    def test_badge_matches_shared_geometry(self):
        svg = alg_coverage_badge.badge_svg("alg coverage", "70.0%", 70.0)
        self.assertIn('fill="#2ea043"', svg)
        self.assertIn("alg coverage: 70.0%", svg)


class FetchDocsLogicTests(unittest.TestCase):
    def test_entries_cover_raylib_c23_and_win32(self):
        paths = [entry["path"] for entry in fetch_docs.ENTRIES]
        self.assertTrue(any("cheatsheet" in p for p in paths))
        self.assertTrue(any("examples" in p for p in paths))
        self.assertTrue(any("6.2" in p for p in paths))
        self.assertTrue(any("c23" in p for p in paths))
        self.assertTrue(any("job-objects" in p for p in paths))

    def test_header_lines_carry_source_retrieved_license(self):
        entry = fetch_docs.ENTRIES[0]
        header = fetch_docs.header_text(entry, "2026-10-27")
        self.assertTrue(header.startswith("# source: "))
        self.assertIn("# retrieved: 2026-10-27\n", header)
        self.assertIn("# license: ", header)

    def test_manifest_line_is_sha_bytes_path_url(self):
        line = fetch_docs.manifest_line("ab" * 32, 12, "raylib/x.html", "https://example")
        self.assertEqual(line, "ab" * 32 + "  12  raylib/x.html  https://example")

    def test_preamble_states_raylib_line_and_format(self):
        preamble = fetch_docs.preamble_lines("2026-10-27")
        joined = "\n".join(preamble)
        self.assertIn("raylib 6.2", joined)
        self.assertIn("sha256  bytes  path  url", joined)
        self.assertIn("# ", joined)


class FetchFontsLogicTests(unittest.TestCase):
    def test_pins_carry_the_committed_shas(self):
        names = {f["name"]: f["sha256"] for f in fetch_fonts.FONTS}
        self.assertEqual(
            names["NotoSans-Regular.ttf"], "478c558ea716033cd60c03438f628dfa75694dcf6b5f6d505a2f05fd2b4f3823"
        )
        self.assertEqual(
            names["NotoSans-Bold.ttf"], "1df075a380fc7cb898acf64c1f7b3b4dd780de3caa860178bf929de35817a913"
        )
        self.assertEqual(
            names["OFL.txt"], "f2095b08bed08b23a6fe26112fcd679a2bee3f002eef077eb05d215ed1051bd8"
        )

    def test_needs_download_rules(self):
        self.assertTrue(fetch_fonts.needs_download(None, "x"))
        self.assertTrue(fetch_fonts.needs_download("mismatch", "x"))
        self.assertFalse(fetch_fonts.needs_download("x", "x"))


if __name__ == "__main__":
    unittest.main()
