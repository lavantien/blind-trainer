import contextlib
import io
import json
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
    '{"conclusion":"success","databaseId":41217224663,'
    '"headSha":"1d763327e019233da29f0e8b17b839be02bec9a0","status":"completed"}'
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
        for conclusion in (
            "failure",
            "cancelled",
            "startup_failure",
            "timed_out",
            "action_required",
        ):
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
            {"status": "completed", "conclusion": None},
            {"status": "completed", "conclusion": "neutral"},
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


class BadgeSvgEscapeTests(unittest.TestCase):
    def test_xml_special_characters_are_escaped(self):
        svg = badge_svg("A&B", "<x>", color=GREY)
        self.assertIn('aria-label="A&amp;B: &lt;x&gt;"', svg)
        self.assertIn("<title>A&amp;B: &lt;x&gt;</title>", svg)
        self.assertIn(">A&amp;B</text>", svg)
        self.assertIn(">&lt;x&gt;</text>", svg)
        self.assertNotIn("A&B", svg)
        self.assertNotIn("<x>", svg)

    def test_quote_in_label_or_value_is_escaped(self):
        svg = badge_svg('say "hi"', '"ok"', color=GREY)
        self.assertNotIn('"hi"', svg)
        self.assertIn("&quot;hi&quot;", svg)

    def test_committed_alg_coverage_render_is_unchanged(self):
        badge_dir = Path(__file__).resolve().parents[2] / "docs" / "badges"
        self.assertEqual(
            (badge_dir / "alg-coverage.svg").read_bytes(),
            badge_svg("alg coverage", "99.9%", 99.9).encode("utf-8"),
        )

    def test_committed_coverage_badge_renders_its_recorded_counts(self):
        badge_dir = Path(__file__).resolve().parents[2] / "docs" / "badges"
        counts = json.loads((badge_dir / "coverage.json").read_text(encoding="utf-8"))
        self.assertEqual(set(counts), {"covered", "count"})
        self.assertGreater(counts["count"], 0)
        self.assertLessEqual(counts["covered"], counts["count"])
        percent = 100.0 * counts["covered"] / counts["count"]
        self.assertEqual(
            (badge_dir / "coverage.svg").read_bytes(),
            badge_svg("coverage", f"{percent:.1f}%", percent).encode("utf-8"),
        )


class CiBadgeEndToEndTests(unittest.TestCase):
    def run_with_stub(self, tmp, body, extra_argv=(), out=None):
        (Path(tmp) / "gh.bat").write_text(body, encoding="utf-8")
        target = out if out is not None else Path(tmp) / "ci.svg"
        old = os.environ["PATH"]
        os.environ["PATH"] = str(tmp) + os.pathsep + old
        try:
            code = ci_badge.main(["--out", str(target), *extra_argv])
        finally:
            os.environ["PATH"] = old
        return code, target

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

    def test_valid_json_with_nonzero_exit_fails_closed(self):
        with tempfile.TemporaryDirectory() as tmp:
            code, out = self.run_with_stub(tmp, f"@echo [{PASSED_RUN}]\n@exit /b 1\n")
            self.assertEqual(code, 1)
            self.assertFalse(out.exists())

    def test_gh_argv_is_pinned_with_the_branch_filter(self):
        with tempfile.TemporaryDirectory() as tmp:
            stub = (
                "@echo off\n"
                'echo %*>>"%~dp0gh_args.txt"\n'
                f"@echo [{PASSED_RUN}]\n"
            )
            code, out = self.run_with_stub(tmp, stub)
            self.assertEqual(code, 0)
            self.assert_badge_bytes(out, "passed")
            recorded = (Path(tmp) / "gh_args.txt").read_text(encoding="utf-8").strip()
            self.assertEqual(
                recorded.split(),
                [
                    "run",
                    "list",
                    "--workflow",
                    "ci.yml",
                    "--branch",
                    "main",
                    "--limit",
                    "1",
                    "--json",
                    "status,conclusion,headSha,databaseId",
                ],
            )

    def test_gh_auth_failure_with_offline_renders_unknown_and_exits_zero(self):
        with tempfile.TemporaryDirectory() as tmp:
            code, out = self.run_with_stub(
                tmp, "@echo gh: run gh auth login first\n@exit /b 1\n", ["--offline"]
            )
            self.assertEqual(code, 0)
            self.assert_badge_bytes(out, "unknown")

    def test_whitespace_only_stderr_fails_closed_without_a_traceback(self):
        with tempfile.TemporaryDirectory() as tmp:
            buffer = io.StringIO()
            with contextlib.redirect_stderr(buffer):
                code, out = self.run_with_stub(tmp, "@echo. 1>&2\n@exit /b 1\n")
            self.assertEqual(code, 1)
            self.assertFalse(out.exists())
            self.assertIn(
                "[fail] ci-badge: gh exited 1: empty output", buffer.getvalue()
            )
            self.assertNotIn("Traceback", buffer.getvalue())

    def test_whitespace_only_stderr_with_offline_renders_unknown(self):
        with tempfile.TemporaryDirectory() as tmp:
            code, out = self.run_with_stub(
                tmp, "@echo. 1>&2\n@exit /b 1\n", ["--offline"]
            )
            self.assertEqual(code, 0)
            self.assert_badge_bytes(out, "unknown")

    def test_out_under_a_regular_file_parent_fails_with_a_trace(self):
        with tempfile.TemporaryDirectory() as tmp:
            blocker = Path(tmp) / "blocker"
            blocker.write_text("x", encoding="utf-8")
            buffer = io.StringIO()
            with contextlib.redirect_stderr(buffer):
                code, out = self.run_with_stub(
                    tmp,
                    f"@echo [{PASSED_RUN}]\n",
                    out=blocker / "ci.svg",
                )
            self.assertEqual(code, 1)
            self.assertFalse(out.exists())
            self.assertIn("[fail] ci-badge:", buffer.getvalue())
            self.assertNotIn("Traceback", buffer.getvalue())

    def test_out_under_a_regular_file_parent_fails_even_offline(self):
        with tempfile.TemporaryDirectory() as tmp:
            blocker = Path(tmp) / "blocker"
            blocker.write_text("x", encoding="utf-8")
            code, out = self.run_with_stub(
                tmp,
                "@echo gh: run gh auth login first\n@exit /b 1\n",
                ["--offline"],
                out=blocker / "ci.svg",
            )
            self.assertEqual(code, 1)
            self.assertFalse(out.exists())

    def test_offline_stdout_carries_the_real_reason(self):
        with tempfile.TemporaryDirectory() as tmp:
            stdout = io.StringIO()
            with contextlib.redirect_stdout(stdout):
                code, out = self.run_with_stub(
                    tmp,
                    "@echo gh: run gh auth login first\n@exit /b 1\n",
                    ["--offline"],
                )
            self.assertEqual(code, 0)
            self.assert_badge_bytes(out, "unknown")
            self.assertIn(
                "ci: unknown (gh exited 1: gh: run gh auth login first)",
                stdout.getvalue(),
            )
            self.assertNotIn("no matching run", stdout.getvalue())

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

    def test_trace_lines_carry_the_unit_name_once(self):
        with tempfile.TemporaryDirectory() as tmp:
            buffer = io.StringIO()
            with contextlib.redirect_stderr(buffer):
                code, out = self.run_with_stub(tmp, f"@echo [{PASSED_RUN}]\n")
            self.assertEqual(code, 0)
            self.assertTrue(out.exists())
            self.assertIn("[entry] ci-badge: out=", buffer.getvalue())
            self.assertIn("[exit] ci-badge: code=0", buffer.getvalue())
            self.assertNotIn("ci-badge: ci-badge", buffer.getvalue())


class ConclusionModeTests(unittest.TestCase):
    def run_conclusion(self, tmp, conclusion, extra_argv=()):
        out = Path(tmp) / "ci.svg"
        old = os.environ["PATH"]
        os.environ["PATH"] = str(tmp)
        try:
            code = ci_badge.main(
                ["--out", str(out), "--conclusion", conclusion, *extra_argv]
            )
        finally:
            os.environ["PATH"] = old
        return code, out

    def test_conclusion_success_renders_passed_without_gh(self):
        with tempfile.TemporaryDirectory() as tmp:
            code, out = self.run_conclusion(tmp, "success")
            self.assertEqual(code, 0)
            self.assert_badge_written(out, "passed")

    def test_conclusion_failure_renders_failed(self):
        with tempfile.TemporaryDirectory() as tmp:
            code, out = self.run_conclusion(tmp, "failure")
            self.assertEqual(code, 0)
            self.assert_badge_written(out, "failed")

    def test_conclusion_timed_out_renders_failed(self):
        with tempfile.TemporaryDirectory() as tmp:
            code, out = self.run_conclusion(tmp, "timed_out")
            self.assertEqual(code, 0)
            self.assert_badge_written(out, "failed")

    def test_conclusion_action_required_renders_failed(self):
        with tempfile.TemporaryDirectory() as tmp:
            code, out = self.run_conclusion(tmp, "action_required")
            self.assertEqual(code, 0)
            self.assert_badge_written(out, "failed")

    def test_conclusion_neutral_renders_unknown(self):
        with tempfile.TemporaryDirectory() as tmp:
            code, out = self.run_conclusion(tmp, "neutral")
            self.assertEqual(code, 0)
            self.assert_badge_written(out, "unknown")

    def test_unknown_conclusion_value_fails_closed(self):
        with tempfile.TemporaryDirectory() as tmp:
            buffer = io.StringIO()
            with contextlib.redirect_stderr(buffer):
                code, out = self.run_conclusion(tmp, "bogus")
            self.assertEqual(code, 2)
            self.assertFalse(out.exists())
            self.assertIn("[fail] ci-badge: unknown conclusion: bogus", buffer.getvalue())

    def test_conclusion_mode_is_idempotent(self):
        with tempfile.TemporaryDirectory() as tmp:
            code, out = self.run_conclusion(tmp, "success")
            first = out.read_bytes()
            code, out = self.run_conclusion(tmp, "success")
            self.assertEqual(code, 0)
            self.assertEqual(out.read_bytes(), first)

    def test_conclusion_stdout_names_the_conclusion(self):
        with tempfile.TemporaryDirectory() as tmp:
            stdout = io.StringIO()
            with contextlib.redirect_stdout(stdout):
                code, out = self.run_conclusion(tmp, "success")
            self.assertEqual(code, 0)
            self.assertIn("ci: passed (conclusion=success)", stdout.getvalue())

    def assert_badge_written(self, out, verdict):
        self.assertEqual(
            out.read_bytes(),
            badge_svg("CI", verdict, color=ci_badge.VERDICT_COLORS[verdict]).encode(
                "utf-8"
            ),
        )


class CiWorkflowTests(unittest.TestCase):
    def setUp(self):
        self.text = (
            Path(__file__).resolve().parents[2] / ".github" / "workflows" / "ci.yml"
        ).read_text(encoding="utf-8")

    def badge_block(self):
        return self.text.split("  badge:\n", 1)[1]

    def test_badge_job_runs_after_ci_on_every_push(self):
        block = self.badge_block()
        self.assertIn("    needs: ci\n", block)
        self.assertIn("    if: always() && github.event_name == 'push'\n", block)

    def test_badge_job_writes_contents_on_windows_python(self):
        block = self.badge_block()
        self.assertIn("    permissions:\n      contents: write\n", block)
        self.assertIn("    runs-on: windows-latest\n", block)
        self.assertIn('          python-version: "3.14"\n', block)

    def test_badge_job_renders_the_conclusion_from_the_ci_result(self):
        block = self.badge_block()
        self.assertIn("scripts/ci_badge.py --conclusion", block)
        self.assertIn("needs.ci.result", block)

    def test_badge_job_commits_only_when_bytes_changed(self):
        block = self.badge_block()
        self.assertIn("git status --porcelain -- docs/badges/ci.svg", block)
        guard = block.find("if ($changed)")
        commit = block.find("git commit")
        push = block.find("git push")
        self.assertLess(guard, commit)
        self.assertLess(commit, push)
        self.assertIn("[skip ci]", block.split("git commit", 1)[1].split("\n", 1)[0])


class CommittedBadgeTests(unittest.TestCase):
    def test_committed_ci_svg_is_a_renderer_product_for_its_verdict(self):
        committed = (
            Path(__file__).resolve().parents[2] / "docs" / "badges" / "ci.svg"
        ).read_bytes()
        matches = [
            verdict
            for verdict in ci_badge.VERDICT_COLORS
            if committed
            == badge_svg(
                "CI", verdict, color=ci_badge.VERDICT_COLORS[verdict]
            ).encode("utf-8")
        ]
        self.assertEqual(
            len(matches),
            1,
            f"committed ci.svg matches {len(matches)} renderer verdicts, it must be "
            f"the byte-exact badge_svg product of exactly one verdict in "
            f"{sorted(ci_badge.VERDICT_COLORS)}",
        )


if __name__ == "__main__":
    unittest.main()
