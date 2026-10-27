import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

import bt

REPO_SCRIPT = bt.REPO_ROOT / bt.MIGRATIONS_GENERATOR


def plant_repo(root):
    (root / "scripts").mkdir(parents=True)
    (root / "db" / "migrations").mkdir(parents=True)
    (root / "src" / "data").mkdir(parents=True)
    shutil.copyfile(REPO_SCRIPT, root / bt.MIGRATIONS_GENERATOR)
    shutil.copyfile(bt.REPO_ROOT / ".clang-format", root / ".clang-format")
    (root / "db" / "migrations" / "001_init.sql").write_text(
        "create table t(a integer);\n", encoding="utf-8"
    )
    (root / "db" / "migrations" / "002_seed.sql").write_text(
        "insert into t(a, b, c) values(1, 'long literal far past the hundred column mark"
        " so the embedded line needs the repo format wrap');\n",
        encoding="utf-8",
    )
    subprocess.run(
        [sys.executable, "-I", str(root / bt.MIGRATIONS_GENERATOR)],
        check=True,
        capture_output=True,
    )
    fmt = bt.resolve_tool("clang_format")
    subprocess.run(
        [fmt, "-i", "--style=file", str(root / "src/data/migrations_embedded.c"),
         str(root / "src/data/migrations_embedded.h")],
        check=True,
        capture_output=True,
    )


class MigrationsFreshTests(unittest.TestCase):
    def test_matching_tree_reports_no_stale_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            plant_repo(root)
            self.assertIsNone(bt.migrations_stale(root))

    def test_tampered_embedded_body_is_reported(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            plant_repo(root)
            target = root / "src/data/migrations_embedded.c"
            target.write_bytes(target.read_bytes() + b"\n")
            self.assertEqual(bt.migrations_stale(root), "src/data/migrations_embedded.c")

    def test_missing_embedded_header_is_reported(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            plant_repo(root)
            (root / "src/data/migrations_embedded.h").unlink()
            self.assertEqual(bt.migrations_stale(root), "src/data/migrations_embedded.h")

    def test_verify_names_the_regeneration_command(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            plant_repo(root)
            (root / "src/data/migrations_embedded.c").unlink()
            with self.assertRaises(bt.BtError) as caught:
                bt.verify_migrations_fresh(root, bt.Logger())
            self.assertIn(bt.MIGRATIONS_GENERATOR, str(caught.exception))
            self.assertIn("fmt-fix", str(caught.exception))


if __name__ == "__main__":
    unittest.main()
