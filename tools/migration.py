#!/usr/bin/env python3
"""Build the pinned original and migration fork, retaining validation evidence."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
LOCAL = ROOT / ".local"
REFERENCE = LOCAL / "reference/openttd"
BASELINE = json.loads((ROOT / "migration/baseline.json").read_text())


def environment():
    env = os.environ.copy()

    def prepend(name, value):
        env[name] = str(value) + (os.pathsep + env[name] if env.get(name) else "")

    if (LOCAL / "cargo/bin").exists():
        env["CARGO_HOME"] = str(LOCAL / "cargo")
        env["RUSTUP_HOME"] = str(LOCAL / "rustup")
        prepend("PATH", LOCAL / "cargo/bin")
    deps = LOCAL / "deps"
    if deps.exists():
        prepend("PATH", deps / "usr/bin")
        prepend("LD_LIBRARY_PATH", deps / "usr/lib/x86_64-linux-gnu")
        prepend("CMAKE_PREFIX_PATH", deps / "usr")
        prepend("CMAKE_INCLUDE_PATH", deps / "usr/include")
        prepend("CPATH", deps / "usr/include")
        prepend("PKG_CONFIG_PATH", deps / "usr/lib/x86_64-linux-gnu/pkgconfig")
        env["PKG_CONFIG_SYSROOT_DIR"] = str(deps)
    return env


def git(*args, cwd=ROOT):
    return subprocess.check_output(["git", *args], cwd=cwd, text=True).strip()


def ensure_reference():
    if not REFERENCE.exists():
        REFERENCE.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([
            "git", "worktree", "add", "--detach", str(REFERENCE), BASELINE["commit"],
        ], cwd=ROOT, check=True)
    if git("rev-parse", "HEAD", cwd=REFERENCE) != BASELINE["commit"]:
        raise RuntimeError("Reference checkout differs from migration/baseline.json")
    if git("status", "--porcelain", "--untracked-files=all", cwd=REFERENCE):
        raise RuntimeError("Reference checkout has changes; restore the original before comparison")


def supply_graphics(build):
    candidates = [
        LOCAL / "deps/usr/share/games/openttd/baseset/opengfx",
        Path("/usr/share/games/openttd/baseset/opengfx"),
        Path("/usr/share/openttd/baseset/opengfx"),
    ]
    source = next((path for path in candidates if path.is_dir()), None)
    if source is None:
        raise RuntimeError("OpenGFX is needed for regression games; install it or run tools/bootstrap-local.py")
    destination = build / "baseset/opengfx"
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not destination.exists():
        destination.symlink_to(source, target_is_directory=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("build", "verify"), nargs="?", default="verify")
    parser.add_argument("--jobs", type=int, default=min(6, os.cpu_count() or 1))
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    ensure_reference()
    env = environment()
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    evidence = LOCAL / "verification" / stamp
    evidence.mkdir(parents=True)
    report = {
        "baseline": BASELINE,
        "candidate_commit": git("rev-parse", "HEAD"),
        "candidate_status": git("status", "--short"),
        "action": args.action,
        "started_at": stamp,
        "commands": [],
        "passed": False,
    }
    patch = subprocess.check_output(["git", "diff", "HEAD", "--binary"], cwd=ROOT)
    (evidence / "candidate.patch").write_bytes(patch)
    # Also retain new files so an uncommitted verification run remains inspectable.
    untracked = subprocess.check_output([
        "git", "ls-files", "--others", "--exclude-standard", "-z",
    ], cwd=ROOT).decode().split("\0")
    for name in filter(None, untracked):
        destination = evidence / "untracked" / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / name, destination)

    def run(name, command, cwd=ROOT):
        print(f"{name}: {' '.join(map(str, command))}", flush=True)
        log = evidence / f"{name}.log"
        start = time.monotonic()
        with log.open("w") as output:
            result = subprocess.run(command, cwd=cwd, env=env, stdout=output, stderr=subprocess.STDOUT)
        report["commands"].append({
            "name": name, "argv": list(map(str, command)), "cwd": str(cwd),
            "exit_code": result.returncode, "seconds": round(time.monotonic() - start, 3),
            "log": str(log.relative_to(ROOT)),
        })
        if result.returncode:
            print(log.read_text(errors="replace")[-16000:], file=sys.stderr)
            raise RuntimeError(f"{name} failed; see {log}")
        return log

    builds = {"reference": ROOT / "build-reference", "candidate": ROOT / "build-rust"}
    try:
        common = [
            "-G", "Ninja", "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
            "-DOPTION_USE_ASSERTS=ON", "-DOPTION_DEDICATED=OFF",
            "-DCMAKE_DISABLE_FIND_PACKAGE_Grfcodec=ON", "-DBUILD_TESTING=ON",
        ]
        for name, build in builds.items():
            source = REFERENCE if name == "reference" else ROOT
            extra = [] if name == "reference" else [
                "-DBINARY_NAME=openttd-rust",
            ]
            run(f"{name}-configure", ["cmake", "-S", str(source), "-B", str(build), *common, *extra])
            supply_graphics(build)
            run(f"{name}-build", ["cmake", "--build", str(build), "--parallel", str(args.jobs)])
            binary = build / ("openttd" if name == "reference" else "openttd-rust")
            report[f"{name}_binary_sha256"] = hashlib.sha256(binary.read_bytes()).hexdigest()

        if args.action == "verify":
            inventories = {}
            for name, build in builds.items():
                log = run(f"{name}-test-inventory", ["ctest", "--show-only=json-v1"], cwd=build)
                inventories[name] = {test["name"] for test in json.loads(log.read_text())["tests"]}
                report[f"{name}_test_count"] = len(inventories[name])
                if not inventories[name]:
                    raise RuntimeError(f"{name.capitalize()} CTest inventory is empty; refusing untested verification")
                run(f"{name}-tests", [
                    "ctest", "--output-on-failure", "--parallel", str(args.jobs),
                    "--output-junit", str(evidence / f"{name}-tests.xml"),
                ], cwd=build)
            missing = inventories["reference"] - inventories["candidate"]
            if missing:
                raise RuntimeError(f"Candidate removed reference tests: {sorted(missing)}")
        ensure_reference()
        report["passed"] = True
    finally:
        (evidence / "report.json").write_text(json.dumps(report, indent=2) + "\n")
        print(f"Evidence: {evidence / 'report.json'}", flush=True)
    print(f"{args.action}: passed", flush=True)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError) as error:
        sys.exit(str(error))
