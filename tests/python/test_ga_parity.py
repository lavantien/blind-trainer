import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))

import bt
import ga_parity


def sample_hub():
    with open(bt.REPO_ROOT / "config" / "config.json", encoding="utf-8") as f:
        hub = json.load(f)
    return hub


class ImprovementGenerationTests(unittest.TestCase):
    def test_pins_the_generations_where_the_best_strictly_improved(self):
        self.assertEqual(ga_parity.improvement_generations([5.0, 4.0, 4.0, 3.0, 3.0, 3.0, 2.0]),
                         [0, 1, 3, 6])

    def test_flat_trajectory_improves_only_at_generation_zero(self):
        self.assertEqual(ga_parity.improvement_generations([1.5, 1.5, 1.5]), [0])


class ToleranceTests(unittest.TestCase):
    def test_within_tolerance_accepts_the_boundary(self):
        self.assertTrue(ga_parity.within_tolerance(1.0, 1.0 + 1e-06, 1e-06))
        self.assertFalse(ga_parity.within_tolerance(1.0, 1.0 + 1.1e-06, 1e-06))


def base_report():
    return {
        "best_fitness": 12.5,
        "best_parts": [1.0, 2.0, 3.0],
        "best_generation": 7,
        "generations_run": 500,
        "per_gen_best": [9.0, 8.0, 8.0, 7.0],
        "stagnation_tail": 4,
        "max_stagnation": 9,
        "evaluations": 100200,
        "freq_sha256": "a" * 64,
        "syllables_sha256": "b" * 64,
    }


class CompareReportsTests(unittest.TestCase):
    def test_identical_reports_do_not_drift(self):
        drift, improved = ga_parity.compare_reports(base_report(), base_report(), 1e-06)
        self.assertEqual(drift, [])
        self.assertEqual(improved, [0, 1, 3])

    def test_exact_pin_drift_names_the_field(self):
        cuda = base_report()
        cuda["best_generation"] = 8
        drift, _ = ga_parity.compare_reports(base_report(), cuda, 1e-06)
        self.assertTrue(any("best_generation" in line for line in drift))

    def test_tolerance_drift_names_the_index(self):
        cuda = base_report()
        cuda["per_gen_best"] = [9.0, 8.0, 8.0, 7.01]
        drift, _ = ga_parity.compare_reports(base_report(), cuda, 1e-06)
        self.assertTrue(any("per_gen_best[3]" in line for line in drift))

    def test_tolerance_covers_a_small_shift(self):
        cuda = base_report()
        cuda["best_fitness"] = 12.5 + 1e-07
        drift, _ = ga_parity.compare_reports(base_report(), cuda, 1e-06)
        self.assertEqual(drift, [])

    def test_improvement_pattern_drift_is_named(self):
        cuda = base_report()
        cuda["per_gen_best"] = [9.0, 8.0, 8.0 - 5e-07, 7.0]
        drift, _ = ga_parity.compare_reports(base_report(), cuda, 1e-06)
        self.assertEqual(
            [line for line in drift if "per_gen_best" in line],
            [],
        )
        self.assertTrue(any("improvement generations" in line for line in drift))


class ScratchConfigTests(unittest.TestCase):
    def test_overrides_the_cuda_flag_and_paths_only(self):
        hub = sample_hub()
        config = ga_parity.scratch_config(hub, True, "x/cpu", "x/cpu")
        self.assertTrue(config["cuda"]["enable"])
        self.assertEqual(config["paths"]["data_ga"], "x/cpu")
        self.assertEqual(config["paths"]["artifacts"], "x/cpu")
        self.assertEqual(config["ga"]["seed"], hub["ga"]["seed"])
        self.assertEqual(hub["cuda"]["enable"], False)


class CpuGoldenTests(unittest.TestCase):
    def test_golden_check_ignores_run_config_and_names_divergence(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            golden = root / "golden"
            lane = root / "lane"
            for path in (golden, lane):
                path.mkdir()
            for relative, _ in ga_parity.GOLDEN_OUTPUTS:
                produced = lane / relative
                produced.parent.mkdir(parents=True, exist_ok=True)
                produced.write_bytes(relative.encode())
                (golden / Path(relative).name).write_bytes(relative.encode())
            report = {"value": 1, "run_config": {"who": "golden"}}
            with open(golden / "report.json", "w", encoding="utf-8") as handle:
                json.dump(report, handle)
            lane_report = {"value": 1, "run_config": {"who": "lane"}}
            with open(lane / "report.json", "w", encoding="utf-8") as handle:
                json.dump(lane_report, handle)
            drift, hashes = ga_parity.check_cpu_golden(lane, golden)
            self.assertEqual(drift, [])
            self.assertEqual(sorted(hashes), ["report.md", "syllables.json"])
            (lane / "syllables.json").write_bytes(b"drifted")
            drift, _ = ga_parity.check_cpu_golden(lane, golden)
            self.assertTrue(any("syllables.json" in line for line in drift))


class ParityHubTests(unittest.TestCase):
    def test_hub_lists_the_parity_suites_and_tolerance(self):
        hub = sample_hub()
        self.assertIn("ga_eval", hub["cuda"]["parity_suites"])
        self.assertGreater(hub["cuda"]["parity_tolerance"], 0.0)
        self.assertGreater(hub["cuda"]["parity_timeout_secs"], 0)

    def test_ga_parity_target_is_registered(self):
        pairs = dict(bt.target_functions(sys.modules["bt"]))
        self.assertIn("ga-parity", pairs)
        self.assertTrue(bt.target_doc(pairs["ga-parity"]))


if __name__ == "__main__":
    unittest.main()
