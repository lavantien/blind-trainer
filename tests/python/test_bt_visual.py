import hashlib
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

import bt


class VisualTargetRegistrationTests(unittest.TestCase):
    def test_visual_targets_are_registered(self):
        names = dict(bt.target_functions(bt))
        for name in ("visual", "baselines"):
            self.assertIn(name, names)
            self.assertTrue(bt.target_doc(names[name]))


class VisualPairDirsTests(unittest.TestCase):
    def test_pair_dirs_follow_the_configured_paths(self):
        hub = bt.load_hub()
        shots, bases = bt.visual_pair_dirs(hub)
        self.assertEqual(shots, bt.REPO_ROOT / "artifacts" / "e2e")
        self.assertEqual(bases, bt.REPO_ROOT / hub["paths"]["baselines"])

    def test_report_path_lands_under_the_artifacts_root(self):
        hub = bt.load_hub()
        self.assertEqual(
            bt.visual_report_path(hub),
            bt.REPO_ROOT / hub["paths"]["artifacts"] / "visual" / "report.md",
        )


class VisualRetryOutcomeTests(unittest.TestCase):
    def test_failed_re_render_is_an_infra_error(self):
        self.assertEqual(
            bt.visual_retry_outcome(1, None), (4, "infra error, re-render failed")
        )
        self.assertEqual(
            bt.visual_retry_outcome(2, 0), (4, "infra error, re-render failed")
        )

    def test_second_pass_green_is_an_infra_flake(self):
        self.assertEqual(
            bt.visual_retry_outcome(0, 0), (3, "infra flake, second run passed")
        )

    def test_second_pass_mismatch_is_a_regression(self):
        self.assertEqual(
            bt.visual_retry_outcome(0, 1), (2, "regression, both runs mismatched")
        )
        self.assertEqual(
            bt.visual_retry_outcome(0, 2), (2, "regression, both runs mismatched")
        )


class VisualVerdictAppendTests(unittest.TestCase):
    def test_verdict_appends_after_the_report_body(self):
        with tempfile.TemporaryDirectory() as tmp:
            report = Path(tmp) / "visual" / "report.md"
            report.parent.mkdir(parents=True)
            report.write_text("# visual diff report\n", encoding="utf-8")
            bt.visual_append_verdict(report, "regression, both runs mismatched")
            lines = report.read_text(encoding="utf-8").splitlines()
            self.assertEqual(lines[-1], "verdict: regression, both runs mismatched")
            self.assertEqual(lines[-2], "")

    def test_verdict_creates_a_missing_report(self):
        with tempfile.TemporaryDirectory() as tmp:
            report = Path(tmp) / "visual" / "report.md"
            bt.visual_append_verdict(report, "infra error, re-render failed")
            self.assertIn(
                "verdict: infra error, re-render failed",
                report.read_text(encoding="utf-8"),
            )


class CaptureHashTests(unittest.TestCase):
    def test_png_hashes_covers_every_png_and_skips_other_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            shots = Path(tmp)
            (shots / "menu.png").write_bytes(b"one")
            (shots / "drill.png").write_bytes(b"two")
            (shots / "report.md").write_text("skip", encoding="utf-8")
            hashes = bt.png_hashes(shots)
            self.assertEqual(sorted(hashes), ["drill.png", "menu.png"])
            self.assertEqual(
                hashes["menu.png"], hashlib.sha256(b"one").hexdigest()
            )

    def test_png_hashes_is_empty_when_the_dir_is_absent(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.assertEqual(bt.png_hashes(Path(tmp) / "absent"), {})


class CaptureDiffTests(unittest.TestCase):
    def test_identical_captures_report_no_drift(self):
        captures = {"menu.png": "aa", "drill.png": "bb"}
        self.assertEqual(bt.capture_diffs(captures, dict(captures)), [])

    def test_a_changed_hash_is_named(self):
        drift = bt.capture_diffs({"menu.png": "aa"}, {"menu.png": "bb"})
        self.assertEqual(drift, ["menu.png hash differs between captures"])

    def test_a_png_missing_from_either_capture_is_named(self):
        self.assertEqual(
            bt.capture_diffs({"menu.png": "aa", "drill.png": "bb"}, {"drill.png": "bb"}),
            ["menu.png missing from second capture"],
        )
        self.assertEqual(
            bt.capture_diffs({"drill.png": "bb"}, {"menu.png": "aa", "drill.png": "bb"}),
            ["menu.png missing from first capture"],
        )

    def test_drift_lines_are_sorted_by_png_name(self):
        drift = bt.capture_diffs(
            {"z.png": "1", "a.png": "1"}, {"z.png": "2", "a.png": "3"}
        )
        self.assertEqual(
            drift,
            [
                "a.png hash differs between captures",
                "z.png hash differs between captures",
            ],
        )


class SmokePlanTests(unittest.TestCase):
    def test_plan_pins_the_driver_app_dirs_and_sandbox(self):
        hub = bt.load_hub()
        driver, app, out_dir, user_dir, env = bt._e2e_smoke_plan(hub)
        self.assertEqual(driver.name, hub["build"]["e2e_exe"])
        self.assertEqual(app.name, hub["build"]["app_exe"])
        self.assertEqual(out_dir, bt.REPO_ROOT / "artifacts" / "e2e")
        self.assertEqual(user_dir, bt.REPO_ROOT / "artifacts" / "e2e-user")
        self.assertEqual(env["APPDATA"], str(user_dir))


if __name__ == "__main__":
    unittest.main()
