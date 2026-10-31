import os
import re
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))

import ci_badge
from badgesvg import GREY, GREEN, RED, YELLOW, badge_svg

PASSED_RUN = (
    '{"conclusion":"success","createdAt":"2026-10-31T15:56:23Z",'
    '"databaseId":41217224663,"headSha":"1d763327e019233da29f0e8b17b839be02bec9a0",'
    '"status":"completed"}'
)
FAILED_RUN = PASSED_RUN.replace('"success"', '"failure"')
RUNNING_RUN = PASSED_RUN.replace('"completed"', '"in_progress"').replace(
    '"success"', 'null'
)


class CiBadgeVerdictTests(unittest.TestCase):
    def test_completed_success_maps_to_passed(self):
        self.assertEqual(
            ci_badge.verdict_of({"status": "completed", "conclusion": "success"}),
            "passed",
        )

    def test_each_failed_conclusion_maps_to_failed(self):
        for conclusion in ("failure", "cancelled", "startup_failure"):
            self.assertEqual(
                ci_badge.verdict_of({"status": "completed", "conclusion": conclusion}),
                "failed",
            )

    def test_live_statuses_map_to_running(self):
        for status in ("in_progress", "queued"):
            self.assertEqual(
                ci_badge.verdict_of({"status": status, "conclusion": None}), "running"
            )

    def test_unmapped_conclusion_or_status_maps_to_unknown(self):
        for run in (
            {"status": "completed", "conclusion": "timed_out"},
            {"status": "completed", "conclusion": None},
            {"status": "waiting", "conclusion": "success"},
            {},
        ):
            self.assertEqual(ci_badge.verdict_of(run), "unknown")

    def test_every_verdict_has_a_color(self):
        self.assertEqual(
            ci_badge.VERDICT_COLORS,
            {"passed": GREEN, "failed": RED, "running": YELLOW, "unknown": GREY},
        )


class BadgeSvgColorTests(unittest.TestCase):
    def test_explicit_color_drives_the_value_fill(self):
        svg = badge_svg("CI", "unknown", color=GREY)
        self.assertIn(f'fill="{GREY}"', svg)
        self.assertIn("CI: unknown", svg)

    def test_explicit_color_matching_the_derived_one_stays_byte_identical(self):
        self.assertEqual(
            badge_svg("coverage", "10.0%", 10.0, color=RED),
            badge_svg("coverage", "10.0%", 10.0),
        )


class CiBadgeEndToEndTests(unittest.TestCase):
    def run_with_stub(self, tmp, body, extra_argv=()):
        (Path(tmp) / "gh.bat").write_text(body, encoding="utf-8")
        out = Path(tmp) / "ci.svg"
        old = os.environ["PATH"]
        os.environ["PATH"] = str(tmp) + os.pathsep + old
        try:
            code = ci_badge.main(["--out", str(out), *extra_argv])
        finally:
            os.environ["PATH"] = old
        return code, out

    def run_with_path(self, tmp, extra_argv=()):
        out = Path(tmp) / "ci.svg"
        old = os.environ["PATH"]
        os.environ["PATH"] = str(tmp)
        try:
            code = ci_badge.main(["--out", str(out), *extra_argv])
        finally:
            os.environ["PATH"] = old
        return code, out

    def assert_badge_bytes(self, out, verdict):
        self.assertEqual(
            out.read_bytes(),
            badge_svg("CI", verdict, color=ci_badge.VERDICT_COLORS[verdict]).encode(
                "utf-8"
            ),
        )

    def test_success_run_renders_the_passed_badge(self):
        with tempfile.TemporaryDirectory() as tmp:
            code, out = self.run_with_stub(tmp, f"@echo [{PASSED_RUN}]\n")
            self.assertEqual(code, 0)
            self.assert_badge_bytes(out, "passed")

    def test_failure_run_renders_the_failed_badge(self):
        with tempfile.TemporaryDirectory() as tmp:
            code, out = self.run_with_stub(tmp, f"@echo [{FAILED_RUN}]\n")
            self.assertEqual(code, 0)
            self.assert_badge_bytes(out, "failed")

    def test_in_progress_run_renders_the_running_badge(self):
        with tempfile.TemporaryDirectory() as tmp:
            code, out = self.run_with_stub(tmp, f"@echo [{RUNNING_RUN}]\n")
            self.assertEqual(code, 0)
            self.assert_badge_bytes(out, "running")

    def test_empty_answer_renders_the_unknown_badge_and_exits_zero(self):
        with tempfile.TemporaryDirectory() as tmp:
            code, out = self.run_with_stub(tmp, "@echo []\n")
            self.assertEqual(code, 0)
            self.assert_badge_bytes(out, "unknown")

    def test_malformed_json_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            code, out = self.run_with_stub(tmp, "@echo not json\n")
            self.assertEqual(code, 1)
            self.assertFalse(out.exists())

    def test_non_list_json_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            code, out = self.run_with_stub(tmp, '@echo {"status": "completed"}\n')
            self.assertEqual(code, 1)
            self.assertFalse(out.exists())

    def test_gh_auth_failure_exits_nonzero_without_a_badge(self):
        with tempfile.TemporaryDirectory() as tmp:
            code, out = self.run_with_stub(
                tmp, "@echo gh: run gh auth login first\n@exit /b 1\n"
            )
            self.assertEqual(code, 1)
            self.assertFalse(out.exists())

    def test_gh_auth_failure_with_offline_renders_unknown_and_exits_zero(self):
        with tempfile.TemporaryDirectory() as tmp:
            code, out = self.run_with_stub(
                tmp, "@echo gh: run gh auth login first\n@exit /b 1\n", ["--offline"]
            )
            self.assertEqual(code, 0)
            self.assert_badge_bytes(out, "unknown")

    def test_missing_gh_exits_nonzero_and_writes_nothing(self):
        with tempfile.TemporaryDirectory() as tmp:
            code, out = self.run_with_path(tmp)
            self.assertEqual(code, 1)
            self.assertFalse(out.exists())

    def test_missing_gh_with_offline_renders_unknown(self):
        with tempfile.TemporaryDirectory() as tmp:
            code, out = self.run_with_path(tmp, ["--offline"])
            self.assertEqual(code, 0)
            self.assert_badge_bytes(out, "unknown")


class CommittedBadgeTests(unittest.TestCase):
    def test_committed_ci_svg_matches_the_renderer(self):
        badge_dir = Path(__file__).resolve().parents[2] / "docs" / "badges"
        raw = (badge_dir / "ci.svg").read_bytes()
        match = re.search(rb'aria-label="CI: ([^"]+)"', raw)
        value = match.group(1).decode("utf-8")
        self.assertEqual(
            badge_svg("CI", value, color=ci_badge.VERDICT_COLORS[value]).encode(
                "utf-8"
            ),
            raw,
        )


if __name__ == "__main__":
    unittest.main()
