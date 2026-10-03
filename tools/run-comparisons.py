#!/usr/bin/env python3
"""Run every reference comparison tool in parallel after a candidate build.

Tools are discovered as tools/*-comparison.py plus tools/compare-integers.py, so
adding a comparison never requires a workflow edit. Each tool keeps its own
scratch directory under .local/; their logs go to .local/comparison-logs/.
"""

import argparse
from concurrent.futures import ThreadPoolExecutor
import os
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
LOGS = ROOT / ".local/comparison-logs"
TIMEOUT_SECONDS = 1200


def discover():
    tools = sorted(ROOT.glob("tools/*-comparison.py"))
    tools.append(ROOT / "tools/compare-integers.py")
    return [tool for tool in tools if tool.is_file()]


def run(tool):
    log = LOGS / f"{tool.stem}.log"
    start = time.monotonic()
    with log.open("w") as output:
        try:
            code = subprocess.run([sys.executable, str(tool)], cwd=ROOT, stdout=output, stderr=subprocess.STDOUT,
                                  timeout=TIMEOUT_SECONDS).returncode
        except subprocess.TimeoutExpired:
            output.write(f"\nTimed out after {TIMEOUT_SECONDS} seconds\n")
            code = -1
    return tool, code, time.monotonic() - start, log


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jobs", type=int, default=min(6, os.cpu_count() or 1))
    parser.add_argument("names", nargs="*", help="Only run tools whose file name contains one of these")
    args = parser.parse_args()
    tools = [tool for tool in discover() if not args.names or any(name in tool.name for name in args.names)]
    if not tools:
        sys.exit("No comparison tools selected")
    LOGS.mkdir(parents=True, exist_ok=True)
    failed = []
    with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        for tool, code, seconds, log in pool.map(run, tools):
            print(f"{'ok  ' if code == 0 else 'FAIL'} {seconds:6.1f}s  {tool.name}", flush=True)
            if code:
                failed.append(log)
    for log in failed:
        print(f"\n===== {log.relative_to(ROOT)} (last 80 lines) =====")
        print("\n".join(log.read_text(errors="replace").splitlines()[-80:]))
    if failed:
        sys.exit(f"{len(failed)} of {len(tools)} comparisons failed")
    print(f"All {len(tools)} comparisons passed")


if __name__ == "__main__":
    main()
