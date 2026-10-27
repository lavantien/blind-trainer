import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

import bt


def plant_area(root, area, suite_stems):
    (root / "src" / area).mkdir(parents=True, exist_ok=True)
    (root / "src" / area / "alg.c").write_text("int f(void) { return 1; }\n", encoding="utf-8")
    unit = root / "tests" / "unit" / area
    unit.mkdir(parents=True)
    for stem in suite_stems:
        (unit / f"{stem}_test.c").write_text(f"BT_TEST({stem}, one)\n", encoding="utf-8")


class MutantsPlanTests(unittest.TestCase):
    def test_plan_pairs_each_suite_with_a_ledger_and_outcome(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            plant_area(root, "core", ["alg", "fuzz_nota"])
            plans = bt.mutants_plan(root, ["core"])
            self.assertEqual([p["suite"] for p in plans], ["core_alg_test"])
            self.assertEqual(plans[0]["ledger"], "artifacts/mutants/ledger-core_alg_test.tsv")
            self.assertEqual(plans[0]["outcomes"], "artifacts/mutants/outcomes-core_alg_test.json")
            self.assertEqual(plans[0]["area"], "core")
            self.assertEqual(plans[0]["sources"], ["src/core/alg.c"])

    def test_plan_sources_reach_area_subdirectories(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            plant_area(root, "app", ["menu"])
            views = root / "src" / "app" / "views"
            views.mkdir(parents=True)
            (views / "drill.c").write_text("int g(void) { return 2; }\n", encoding="utf-8")
            plan = bt.mutants_plan(root, ["app"])[0]
            self.assertEqual(plan["sources"], ["src/app/alg.c", "src/app/views/drill.c"])

    def test_plan_rejects_unknown_area(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(bt.BtError):
                bt.mutants_plan(Path(tmp), ["common"])

    def test_plan_rejects_area_without_sources_or_suites(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "src" / "ga").mkdir(parents=True)
            with self.assertRaises(bt.BtError):
                bt.mutants_plan(root, ["ga"])
            plant_area(root, "ga", ["ga_run"])
            (root / "src" / "ga" / "alg.c").unlink()
            with self.assertRaises(bt.BtError):
                bt.mutants_plan(root, ["ga"])

    def test_mutant_areas_cover_the_domain_areas(self):
        self.assertEqual(
            bt.MUTANT_AREAS, ("core", "scheme", "data", "puzzles", "ga", "app")
        )


if __name__ == "__main__":
    unittest.main()
