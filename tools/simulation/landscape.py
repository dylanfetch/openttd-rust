"""Landscape feasibility: default scheduler gap check and reproducible perf replay."""

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

from . import core


def scheduler_gap():
    """Compare unreachable map phases and commit-after-batch with unchanged C++."""
    import migration

    root = migration.ROOT
    out = migration.LOCAL / "landscape-loop-gap"
    out.mkdir(parents=True, exist_ok=True)
    source = (migration.REFERENCE / "src/landscape.cpp").read_text()
    start = source.index("void RunTileLoop()")
    end = source.index("\nvoid InitializeLandscape()", start)
    (out / "landscape-loop-reference.inc").write_text(source[start:end])
    binary = out / "landscape-loop-gap"
    subprocess.run(
        [
            "c++",
            "-std=c++20",
            "-O2",
            "-I" + str(root / "src"),
            "-I" + str(out),
            str(root / "tools/simulation/landscape-loop-reference.cpp"),
            str(migration.rust_archive(root / "build-rust")),
            "-ldl",
            "-lpthread",
            "-lm",
            "-o",
            str(binary),
        ],
        cwd=root,
        env=migration.environment(),
        check=True,
    )
    subprocess.run([str(binary)], cwd=root, check=True)


def replay(report_path, role, mode, output):
    """Replay a frozen benchmark command with its source runtime and game lock."""
    import migration

    report = json.loads(report_path.read_text())
    result = next(r for r in report["results"] if r["scenario"] == "generate-tgp-256-1")
    if not result["passed"]:
        raise ValueError("profiling requires a semantically passing benchmark")
    command = result["plain_commands"][role]
    source = Path(report["binaries"][role]["source_path"])
    if (
        hashlib.sha256(source.read_bytes()).hexdigest()
        != report["binaries"][role]["sha256"]
    ):
        raise ValueError(
            "source executable changed since benchmark; restore the frozen runtime"
        )
    build = Path(command[0]).parent
    core.copy_runtime(source.parent, source, build, candidate=role == "candidate")
    run_dir = Path(command[command.index("-c") + 1]).parent
    env = dict(
        migration.environment(),
        HOME=str(run_dir),
        XDG_DATA_HOME=str(run_dir / "xdg-data"),
        XDG_CONFIG_HOME=str(run_dir / "xdg-config"),
        XDG_CACHE_HOME=str(run_dir / "xdg-cache"),
    )
    env.pop("OPENTTD_WITNESS", None)
    output.mkdir(parents=True, exist_ok=True)
    prefix = (
        [
            "perf",
            "stat",
            "-r",
            "9",
            "-e",
            "task-clock,cycles,instructions,branches,branch-misses",
        ]
        if mode == "stat"
        else [
            "perf",
            "record",
            "-e",
            "cycles:u",
            "-g",
            "--call-graph",
            "dwarf",
            "-o",
            str(output / "perf.data"),
        ]
    )
    launcher = [sys.executable, str(core.ROOT / "tools/simulation/game_launcher.py")]
    with (
        core.game_slot(True),
        (output / "stdout.log").open("wb") as stdout,
        (output / "stderr.log").open("wb") as stderr,
    ):
        subprocess.run(
            [*prefix, "--", *launcher, *command],
            cwd=build,
            env=env,
            stdout=stdout,
            stderr=stderr,
            check=True,
        )
    if mode == "record":
        with (output / "profile.txt").open("wb") as profile:
            subprocess.run(
                [
                    "perf",
                    "report",
                    "--stdio",
                    "--children",
                    "-i",
                    str(output / "perf.data"),
                ],
                stdout=profile,
                check=True,
            )
    print(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--replay", type=Path)
    parser.add_argument(
        "--role", choices=("reference", "candidate"), default="candidate"
    )
    parser.add_argument("--mode", choices=("stat", "record"), default="stat")
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    if args.replay:
        if not args.out:
            parser.error("--replay requires --out")
        replay(args.replay, args.role, args.mode, args.out.resolve())
    else:
        scheduler_gap()


if __name__ == "__main__":
    main()
