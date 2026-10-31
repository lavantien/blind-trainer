import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

import bt


class TargetTableTests(unittest.TestCase):
    def test_registered_targets_have_docstrings_and_unique_names(self):
        pairs = bt.target_functions(bt)
        self.assertGreater(len(pairs), 0)
        names = [name for name, _ in pairs]
        self.assertEqual(len(names), len(set(names)))
        for name, fn in pairs:
            self.assertTrue(bt.target_doc(fn), f"target {name} lacks a docstring")

    def test_target_name_maps_underscores_to_dashes(self):
        def tgt_fmt_fix():
            """rustfmt in place"""

        pairs = bt.target_functions_from([tgt_fmt_fix])
        self.assertEqual(pairs, [("fmt-fix", tgt_fmt_fix)])

    def test_missing_docstring_raises(self):
        def tgt_broken():
            pass

        with self.assertRaises(bt.BtError):
            bt.target_functions_from([tgt_broken])

    def test_help_output_lists_every_registered_target(self):
        pairs = bt.target_functions(bt)
        text = bt.format_help(bt.target_table(pairs))
        for name, _ in pairs:
            self.assertIn(f"{name:<24}", text)
            self.assertIn(bt.target_doc(dict(pairs)[name]), text)

    def test_help_names_carry_no_function_prefix(self):
        text = bt.format_help(bt.target_table(bt.target_functions(bt)))
        self.assertNotIn("tgt_", text)

    def test_postwave_targets_are_registered(self):
        names = dict(bt.target_functions(bt))
        for name in ("fuzz", "mutants", "mutants-gate", "mutants-gate-selftest"):
            self.assertIn(name, names)
            self.assertTrue(bt.target_doc(names[name]))


class TestUnitScopeTests(unittest.TestCase):
    def test_scope_accepts_area_and_only_pairs(self):
        self.assertEqual(bt.test_unit_scope(["tools"]), ("tools", None, None))
        self.assertEqual(
            bt.test_unit_scope(["tools", "--only", "visual_diff"]),
            ("tools", "visual_diff", None),
        )
        self.assertEqual(bt.test_unit_scope(["common"]), ("common", None, None))

    def test_scope_accepts_area_and_without_pairs(self):
        self.assertEqual(
            bt.test_unit_scope(["tools", "--without", "ga_run_lib"]),
            ("tools", None, "ga_run_lib"),
        )

    def test_scope_rejects_malformed_only_and_without_forms(self):
        for args in (
            ["--only"],
            ["tools", "--only"],
            ["tools", "--only", ""],
            ["tools", "--only=visual_diff"],
            ["tools", "--only", "visual_diff", "extra"],
            ["tools", "--without"],
            ["tools", "--without", ""],
            ["tools", "--without=ga_run_lib"],
            ["tools", "--without", "ga_run_lib", "extra"],
            ["tools", "--only", "a", "--without", "b"],
            ["tools", "extra"],
            ["--only", "visual_diff"],
        ):
            self.assertIsNone(bt.test_unit_scope(args), args)

    def test_scoped_tools_needed_narrows_to_matching_suites(self):
        suites = [
            {"area": "tools", "stem": "policy_lib_test"},
            {"area": "tools", "stem": "visual_diff_lib_test"},
        ]
        wide = bt.tools_needed_scoped(bt.REPO_ROOT, suites)
        self.assertEqual(wide, {"policy", "visual-diff"})
        narrow = bt.tools_needed_scoped(bt.REPO_ROOT, suites, "visual_diff")
        self.assertEqual(narrow, {"visual-diff"})


if __name__ == "__main__":
    unittest.main()
