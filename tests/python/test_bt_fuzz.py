import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

import bt


class FuzzConfigTests(unittest.TestCase):
    def test_hub_carries_the_fuzz_seed(self):
        with open(bt.REPO_ROOT / "config" / "config.json", encoding="utf-8") as f:
            hub = json.load(f)
        self.assertEqual(hub["fuzz"]["seed"], 20261012)
        self.assertGreater(hub["fuzz"]["runs"], 0)
        self.assertGreater(hub["fuzz"]["secs"], 0)


class FuzzSuiteTests(unittest.TestCase):
    def test_fuzz_suites_select_the_plainloop_runners_only(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            core = root / "tests" / "unit" / "core"
            core.mkdir(parents=True)
            (core / "fuzz_notation_test.c").write_text(
                "BT_TEST(fuzz_notation, plainloop)\n", encoding="utf-8"
            )
            (core / "alg_test.c").write_text("BT_TEST(alg, parse)\n", encoding="utf-8")
            (core / "fuzzkill_analysis_test.c").write_text(
                "BT_TEST(fuzzkill, pins)\n", encoding="utf-8"
            )
            self.assertEqual(
                [s["exe_name"] for s in bt.fuzz_suites(root)], ["core_fuzz_notation_test"]
            )

    def test_fuzz_suites_empty_when_tree_has_none(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.assertEqual(bt.fuzz_suites(Path(tmp)), [])


if __name__ == "__main__":
    unittest.main()
