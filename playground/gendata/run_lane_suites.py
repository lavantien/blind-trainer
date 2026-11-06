import argparse
import os
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]


def stamp():
    return time.strftime("%Y-%m-%dT%H:%M:%S")


def main():
    parser = argparse.ArgumentParser(
        prog="run_lane_suites.py",
        description="run bt.py unit suites sequentially inside a lane build clone",
        allow_abbrev=False,
    )
    parser.add_argument("--lane", required=True, help="lane name under artifacts/lane")
    parser.add_argument("--log", required=True, help="progress log path")
    parser.add_argument("--area", default="tools", help="suite area passed to test-unit")
    parser.add_argument("suites", nargs="+", help="suite stems in run order")
    args = parser.parse_args()

    lane_root = REPO / "artifacts" / "lane" / args.lane
    lane_config = lane_root / "config.json"
    lane_bin = lane_root / "build" / "bin"
    if not lane_config.is_file():
        sys.stderr.write(f"missing lane config: {lane_config}\n")
        return 2

    env = dict(os.environ)
    env["BT_CONFIG"] = str(lane_config)
    env["BT_BIN_DIR"] = str(lane_bin)

    log_path = REPO / args.log if not Path(args.log).is_absolute() else Path(args.log)
    log_path.parent.mkdir(parents=True, exist_ok=True)

    failed = []
    with open(log_path, "a", encoding="utf-8", newline="\n") as log:
        log.write(f"lane suites start {stamp()} lane={args.lane}\n")
        log.flush()
        for suite in args.suites:
            log.write(f"=== suite {suite} begin {stamp()} ===\n")
            log.flush()
            proc = subprocess.run(
                [sys.executable, "bt.py", "test-unit", args.area, "--only", suite],
                cwd=REPO,
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
            status = "exit 0" if proc.returncode == 0 else f"exit {proc.returncode}"
            log.write(f"=== suite {suite} {status} {stamp()} ===\n")
            log.flush()
            if proc.returncode != 0:
                failed.append(suite)
        log.write(
            f"lane suites done {stamp()} passed={len(args.suites) - len(failed)} "
            f"failed={len(failed)}"
            + (f" failed_list={','.join(failed)}" if failed else "")
            + "\n"
        )
    print(f"done failed={len(failed)}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
