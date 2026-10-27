import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

import bt


class ScreenshotsTargetTests(unittest.TestCase):
    def test_target_is_registered(self):
        names = dict(bt.target_functions(sys.modules["bt"]))
        self.assertIn("screenshots", names)

    def test_steps_run_the_usage_tour_then_the_fatal_shot(self):
        steps = bt._screenshots_steps(bt.load_hub())
        self.assertEqual(len(steps), 2)
        tour_argv, tour_env, tour_wipe = steps[0]
        fatal_argv, fatal_env, fatal_wipe = steps[1]
        self.assertEqual(tour_argv, ["--usage", str(bt.REPO_ROOT / "docs" / "screenshots")])
        self.assertEqual(fatal_argv, ["--fatal-shot", str(bt.REPO_ROOT / "docs" / "screenshots")])
        self.assertTrue(tour_env["APPDATA"].endswith("usage-user"))
        self.assertEqual(tour_env["BT_CONFIG"], bt.child_env(bt.load_hub())["BT_CONFIG"])
        self.assertTrue(fatal_env["BT_CONFIG"].endswith("usage-absent-config.json"))
        self.assertEqual(fatal_env["APPDATA"], bt.child_env(bt.load_hub())["APPDATA"])
        self.assertEqual(tour_wipe.name, "usage-user")
        self.assertIsNone(fatal_wipe)


if __name__ == "__main__":
    unittest.main()
