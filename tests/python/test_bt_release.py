import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

import bt


class ReleaseTargetRegistrationTests(unittest.TestCase):
    def test_release_targets_are_registered(self):
        names = dict(bt.target_functions(bt))
        for name in ("release-verify", "release-verify-selftest", "dist"):
            self.assertIn(name, names)
            self.assertTrue(bt.target_doc(names[name]))


class ReleaseVersionArgTests(unittest.TestCase):
    def test_v_equals_form_is_accepted(self):
        self.assertEqual(bt.release_version_arg(["v=1.2.3"]), "1.2.3")

    def test_missing_or_malformed_args_yield_empty(self):
        self.assertEqual(bt.release_version_arg([]), "")
        self.assertEqual(bt.release_version_arg(["1.2.3"]), "")
        self.assertEqual(bt.release_version_arg(["--only", "v=1.2.3"]), "")


class ReleaseDistPathsTests(unittest.TestCase):
    def test_paths_follow_the_configured_release_section(self):
        hub = bt.load_hub()
        dist_root, staging = bt.release_dist_paths(hub)
        self.assertEqual(dist_root, bt.REPO_ROOT / hub["paths"]["artifacts"] / "dist")
        self.assertEqual(staging, dist_root / hub["release"]["staging_dir"])


class ReleaseZipPathTests(unittest.TestCase):
    def test_zip_name_carries_the_version_and_platform_suffix(self):
        hub = bt.load_hub()
        dist_root, _ = bt.release_dist_paths(hub)
        zip_path = bt.release_zip_path(dist_root, "1.2.3")
        self.assertEqual(zip_path.name, "blind-trainer-1.2.3-windows-x64.zip")


class ReleaseVerifyPlanTests(unittest.TestCase):
    def test_plan_pins_staging_zip_and_the_run_flag(self):
        hub = bt.load_hub()
        staging, zip_path, argv = bt.release_verify_plan(hub, "1.2.3")
        dist_root, wanted_staging = bt.release_dist_paths(hub)
        self.assertEqual(staging, wanted_staging)
        self.assertEqual(zip_path, dist_root / "blind-trainer-1.2.3-windows-x64.zip")
        self.assertEqual(argv, [str(staging), str(zip_path), "--run"])

    def test_usage_is_reported_before_any_build_when_v_is_missing(self):
        self.assertEqual(bt.tgt_release_verify([]), 2)
        self.assertEqual(bt.tgt_dist([]), 2)


class ReleaseStageExeTests(unittest.TestCase):
    def test_stage_copies_the_app_exe_under_the_release_name(self):
        with tempfile.TemporaryDirectory() as tmp:
            bin_dir = Path(tmp) / "bin"
            bin_dir.mkdir()
            app = bin_dir / "bt-app.exe"
            app.write_bytes(b"exe")
            target = bt.release_stage_exe(bin_dir, "bt-app.exe", "blind-trainer.exe")
            self.assertEqual(target, bin_dir / "blind-trainer.exe")
            self.assertEqual(target.read_bytes(), b"exe")

    def test_stage_raises_when_the_app_exe_is_missing(self):
        with tempfile.TemporaryDirectory() as tmp:
            bin_dir = Path(tmp) / "bin"
            bin_dir.mkdir()
            with self.assertRaises(bt.BtError):
                bt.release_stage_exe(bin_dir, "bt-app.exe", "blind-trainer.exe")


if __name__ == "__main__":
    unittest.main()
