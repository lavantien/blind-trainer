import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
PRISTINE = {
    "scripts/ci_badge.py": (REPO / "scripts" / "ci_badge.py").read_text(encoding="utf-8"),
    "scripts/badgesvg.py": (REPO / "scripts" / "badgesvg.py").read_text(encoding="utf-8"),
    "scripts/slog.py": (REPO / "scripts" / "slog.py").read_text(encoding="utf-8"),
    "tests/python/test_ci_badge.py": (
        REPO / "tests" / "python" / "test_ci_badge.py"
    ).read_text(encoding="utf-8"),
    "docs/badges/ci.svg": (REPO / "docs" / "badges" / "ci.svg").read_text(encoding="utf-8"),
}

sys.path.insert(0, str(REPO / "scripts"))
import ci_badge
from badgesvg import RED, badge_svg

DOCTORED_SVG = badge_svg("CI", "failed", color=RED)

CASES = [
    ("baseline", [], True),
    (
        "m1_failure_maps_passed",
        [("scripts/ci_badge.py", 'if conclusion == "success":', 'if conclusion in ("success", "failure"):')],
        False,
    ),
    (
        "m2_drop_cancelled",
        [
            (
                "scripts/ci_badge.py",
                'FAILED_CONCLUSIONS = ("failure", "cancelled", "startup_failure")',
                'FAILED_CONCLUSIONS = ("failure", "startup_failure")',
            )
        ],
        False,
    ),
    (
        "m3_render_always_grey",
        [("scripts/ci_badge.py", "color=VERDICT_COLORS[verdict]", "color=GREY")],
        False,
    ),
    (
        "m4_offline_removed",
        [("scripts/ci_badge.py", "if not args.offline:", "if True:")],
        False,
    ),
    (
        "m5_committed_svg_drifts_to_failed",
        [("docs/badges/ci.svg", PRISTINE["docs/badges/ci.svg"], DOCTORED_SVG)],
        True,
    ),
    (
        "m6_drop_branch_filter",
        [("scripts/ci_badge.py", '    "--branch",\n    "main",\n', "")],
        True,
    ),
    (
        "m7_unknown_becomes_running",
        [("scripts/ci_badge.py", "    return \"unknown\"", "    return \"running\"")],
        False,
    ),
    (
        "m8_ignore_gh_exit_code",
        [("scripts/ci_badge.py", "if proc.returncode != 0:", "if False and proc.returncode != 0:")],
        True,
    ),
]


def build(root, patches):
    for rel, text in PRISTINE.items():
        target = root / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding="utf-8", newline="")
    for rel, old, new in patches:
        path = root / rel
        body = path.read_text(encoding="utf-8")
        if old not in body:
            raise RuntimeError(f"patch anchor missing in {rel}: {old[:40]}")
        path.write_text(body.replace(old, new, 1), encoding="utf-8", newline="")


def suite_rc(root):
    proc = subprocess.run(
        [sys.executable, "-m", "unittest", "discover", "-s", "tests/python", "-v"],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="backslashreplace",
        cwd=str(root),
        timeout=180,
    )
    return proc.returncode, proc


for name, patches, expect_green in CASES:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / "sandbox"
        build(root, patches)
        rc, proc = suite_rc(root)
        actual_green = rc == 0
        verdict = "expected" if actual_green == expect_green else "UNEXPECTED"
        gap = " (suite blind: gap confirmed)" if actual_green and not expect_green else ""
        print(f"{name}: rc={rc} suite-green={actual_green} expect-green={expect_green} -> {verdict}{gap}")
        if actual_green != expect_green:
            tail = [line for line in proc.stderr.splitlines() if line.strip()][-4:]
            for line in tail:
                print(f"    {line}")
