import os
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
EXE = Path(os.environ.get("RV_EXE", REPO / "artifacts/lane/lane-cov-verify/build/bin/release-verify.exe"))


def main():
    if not EXE.is_file():
        sys.exit(f"probe: missing exe {EXE}")
    config_override = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else None
    with tempfile.TemporaryDirectory() as tmp:
        config = config_override or Path(tmp) / "config.json"
        if config_override is None:
            config.write_text("{}", encoding="utf-8")
        for k in range(0, 10):
            env = os.environ.copy()
            env["BT_CONFIG"] = str(config)
            env["BT_RV_ALLOC_FAIL_OK"] = str(k)
            proc = subprocess.run(
                [str(EXE), "staging", "app.zip"],
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="backslashreplace",
                timeout=60,
                env=env,
                cwd=tmp,
            )
            first = (proc.stderr.strip().splitlines() or [""])[0]
            print(f"k={k} code={proc.returncode} stderr={first[:90]}")


if __name__ == "__main__":
    main()
