import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

import bt


def sample_hub():
    with open(bt.REPO_ROOT / "config" / "config.json", encoding="utf-8") as f:
        return json.load(f)


class AreaClosureTests(unittest.TestCase):
    def test_scheme_closure_includes_core(self):
        hub = sample_hub()
        self.assertEqual(bt.src_area_closure(hub, ["scheme"]), {"scheme", "core"})

    def test_app_closure_covers_every_src_area(self):
        hub = sample_hub()
        self.assertEqual(
            bt.src_area_closure(hub, ["app"]),
            {"app", "ga", "puzzles", "data", "scheme", "core"},
        )

    def test_core_closure_is_itself(self):
        hub = sample_hub()
        self.assertEqual(bt.src_area_closure(hub, ["core"]), {"core"})

    def test_unknown_area_closes_to_itself(self):
        hub = sample_hub()
        self.assertEqual(bt.src_area_closure(hub, ["nope"]), {"nope"})

    def test_tool_closure_of_gendata_spans_data_scheme_puzzles_and_core(self):
        hub = sample_hub()
        self.assertEqual(
            bt.tool_area_closure(hub, ["gendata"]),
            {"data", "scheme", "core", "puzzles"},
        )

    def test_tool_closure_of_dep_free_tools_is_empty(self):
        hub = sample_hub()
        self.assertEqual(bt.tool_area_closure(hub, ["policy", "mutate"]), set())

    def test_every_src_area_names_only_known_deps(self):
        hub = sample_hub()
        areas = set(hub["build"]["src_deps"])
        for deps in hub["build"]["src_deps"].values():
            self.assertTrue(set(deps) <= areas)
        for deps in hub["build"]["tool_deps"].values():
            self.assertTrue(set(deps) <= areas)
