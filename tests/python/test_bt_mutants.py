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


def write_mutate_stub(path, calls_path, fail_union=False):
    lines = [
        "import sys\n",
        "from pathlib import Path\n",
        "argv = sys.argv[1:]\n",
        f"with Path(r'{calls_path.as_posix()}').open('a', encoding='utf-8') as log:\n",
        "    log.write('\\t'.join(argv) + '\\n')\n",
    ]
    if fail_union:
        lines.append("if argv[0] == 'union-report':\n    sys.exit(4)\n")
    lines.append(
        "for i, arg in enumerate(argv):\n"
        "    if arg == '--out':\n"
        "        out = Path(argv[i + 1])\n"
        "        out.parent.mkdir(parents=True, exist_ok=True)\n"
        "        out.write_text('stub outcomes', encoding='utf-8')\n"
    )
    path.write_text("".join(lines), encoding="utf-8")


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


class MutantsUnionWiringTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        plant_area(self.root, "core", ["alg"])
        plant_area(self.root, "scheme", ["scram"])
        self.calls = self.root / "calls.txt"
        self.stub = self.root / "mutate_stub.py"
        write_mutate_stub(self.stub, self.calls)

    def tearDown(self):
        self.tmp.cleanup()

    def test_union_argv_carries_every_ledger_and_one_out(self):
        plans = bt.mutants_plan(self.root, ["core", "scheme"])
        argv = bt.mutants_union_argv(plans, self.root)
        self.assertEqual(argv[0], "union-report")
        ledgers = [argv[i + 1] for i, token in enumerate(argv) if token == "--ledger"]
        self.assertEqual(ledgers, [str(self.root / plan["ledger"]) for plan in plans])
        self.assertEqual(
            argv[-2:], ["--out", str(self.root / "artifacts/mutants/outcomes.json")]
        )
        self.assertEqual(len(argv), 1 + 2 * len(plans) + 2)

    def test_run_mutants_plans_runs_each_suite_then_one_union_report(self):
        plans = bt.mutants_plan(self.root, ["core", "scheme"])
        bt.run_mutants_plans([sys.executable, "-I", str(self.stub)], plans, None, self.root)
        calls = [
            line.split("\t")
            for line in self.calls.read_text(encoding="utf-8").splitlines()
        ]
        self.assertEqual([call[0] for call in calls], ["run", "run", "union-report"])
        for call, plan in zip(calls[:2], plans):
            self.assertEqual(call[call.index("--ledger") + 1], str(self.root / plan["ledger"]))
            self.assertEqual(call[call.index("--exe") + 1], plan["suite"])
            self.assertEqual(call[-len(plan["sources"]) :], plan["sources"])
        union = calls[2]
        self.assertEqual(union.count("--ledger"), len(plans))
        for plan in plans:
            self.assertIn(str(self.root / plan["ledger"]), union)
        self.assertEqual(union[-2], "--out")
        self.assertTrue((self.root / "artifacts/mutants/outcomes.json").is_file())

    def test_run_mutants_plans_surfaces_a_failing_union_report(self):
        write_mutate_stub(self.stub, self.calls, fail_union=True)
        plans = bt.mutants_plan(self.root, ["core", "scheme"])
        with self.assertRaises(bt.BtError) as caught:
            bt.run_mutants_plans(
                [sys.executable, "-I", str(self.stub)], plans, None, self.root
            )
        self.assertIn("union outcomes", str(caught.exception))
        calls = [
            line.split("\t")
            for line in self.calls.read_text(encoding="utf-8").splitlines()
        ]
        self.assertEqual([call[0] for call in calls], ["run", "run", "union-report"])


if __name__ == "__main__":
    unittest.main()
