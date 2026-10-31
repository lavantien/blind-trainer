import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

import bt


class ExtractTests(unittest.TestCase):
    def test_extracts_and_sorts_test_names(self):
        text = (
            "BT_TEST(rng, below_spreads)\nBT_TEST( rng , vectors )\nint not_a_test;\n"
        )
        self.assertEqual(
            bt.extract_bt_tests(text), [("rng", "below_spreads"), ("rng", "vectors")]
        )

    def test_generated_order_is_sorted_by_test_symbol(self):
        text = "BT_TEST(rng, zeta)\nBT_TEST(alpha, beta)\n"
        self.assertEqual(
            bt.extract_bt_tests(text), [("alpha", "beta"), ("rng", "zeta")]
        )

    def test_invalid_identifier_raises(self):
        with self.assertRaises(bt.BtError):
            bt.extract_bt_tests("BT_TEST(my-group, name)\n")

    def test_empty_file_yields_empty_list(self):
        self.assertEqual(bt.extract_bt_tests(""), [])


class DiscoverSuitesTests(unittest.TestCase):
    def test_suites_are_discovered_with_area_prefixed_exe_names(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            common = root / "tests" / "unit" / "common"
            data = root / "tests" / "unit" / "data"
            common.mkdir(parents=True)
            data.mkdir(parents=True)
            (common / "rng_test.c").write_text(
                "BT_TEST(rng, vectors)\nBT_TEST(rng, below)\n", encoding="utf-8"
            )
            (data / "db_x_test.c").write_text("BT_TEST(db, open)\n", encoding="utf-8")
            suites = bt.discover_test_suites(root)
            self.assertEqual(
                [s["exe_name"] for s in suites], ["common_rng_test", "data_db_x_test"]
            )
            self.assertEqual(suites[0]["tests"], [("rng", "below"), ("rng", "vectors")])
            self.assertEqual(suites[0]["source"], common / "rng_test.c")

    def test_non_test_c_files_are_ignored(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "tests" / "unit" / "common").mkdir(parents=True)
            (root / "tests" / "unit" / "common" / "helper.c").write_text(
                "int f(void) { return 1; }\n", encoding="utf-8"
            )
            self.assertEqual(bt.discover_test_suites(root), [])


class GeneratedMainTests(unittest.TestCase):
    def test_main_declares_externs_in_sorted_order_and_calls_harness(self):
        source = bt.generated_main_source(
            "common_rng_test", [("rng", "vectors"), ("rng", "below")]
        )
        self.assertIn("bt_test_rng_vectors", source)
        self.assertIn("bt_test_rng_below", source)
        self.assertLess(
            source.index("bt_test_rng_below"), source.index("bt_test_rng_vectors")
        )
        self.assertIn('bt_harness_run("common_rng_test"', source)
        self.assertIn("int main(void)", source)

    def test_main_carries_no_comment_marker(self):
        source = bt.generated_main_source("x", [("g", "n")])
        self.assertNotIn("//", source)


class HelloTests(unittest.TestCase):
    def test_hello_source_has_main_and_prints(self):
        source = bt.hello_source()
        self.assertIn("int main(void)", source)
        self.assertNotIn("//", source)


class FilterSuitesTests(unittest.TestCase):
    def test_filter_suites_scopes_area_and_substring(self):
        suites = [
            {"area": "tools", "stem": "visual_diff_lib_test"},
            {"area": "tools", "stem": "policy_lib_test"},
            {"area": "common", "stem": "rng_test"},
        ]
        picked = bt.filter_suites(suites, "tools")
        self.assertEqual([s["stem"] for s in picked], ["visual_diff_lib_test", "policy_lib_test"])
        picked = bt.filter_suites(suites, "tools", "visual_diff")
        self.assertEqual([s["stem"] for s in picked], ["visual_diff_lib_test"])
        picked = bt.filter_suites(suites, "tools", "absent")
        self.assertEqual(picked, [])

    def test_filter_suites_drops_stems_matching_without(self):
        suites = [
            {"area": "tools", "stem": "ga_run_lib_test"},
            {"area": "tools", "stem": "policy_lib_test"},
        ]
        picked = bt.filter_suites(suites, "tools", without="ga_run_lib")
        self.assertEqual([s["stem"] for s in picked], ["policy_lib_test"])
        picked = bt.filter_suites(suites, "tools", only="lib", without="ga_run")
        self.assertEqual([s["stem"] for s in picked], ["policy_lib_test"])
        picked = bt.filter_suites(suites, "tools", without="absent")
        self.assertEqual(len(picked), 2)


if __name__ == "__main__":
    unittest.main()
