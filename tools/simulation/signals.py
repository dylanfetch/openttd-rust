"""Signal traversal, return classifications and transient work-set evidence (#219)."""

import hashlib
import json
from pathlib import Path

from . import core
from .console_adapter import console_adapter

ADAPTER = core.ROOT / "tools/simulation/signals-console.cpp"


def scenarios(soak):
    return [
        {
            "name": f"signals-{year}",
            "kind": "save",
            "save": str(core.ROOT / f"migration/saves/padhattan-ridge-{year}.sav"),
            "signals": str(year),
            "ticks": (2 if soak else 1) * 365 * core.TICKS_PER_DAY
            + core.SNAPSHOT_TICKS,
            "console": ["signals_scenario ordinary", "unpause"],
        }
        for year in (1996, 2000, 2006)
    ]


def game_environment(scenario):
    return (
        core.WITNESS_ENVIRONMENT
        if scenario.get("signals") and scenario.get("witnesses")
        else {}
    )


def adapter(build, folder, env):
    """Include unchanged signal.cpp to expose portable private sets to inspection.

    The adapter replaces its ordinary signal object; the pinned source is never
    edited. The same input/snapshot code is used on both sides. Candidate state
    inspection calls the Rust diagnostic entry without resetting its owner.
    """
    folder.mkdir(parents=True, exist_ok=True)
    commands = json.loads((build / "compile_commands.json").read_text())
    signal = Path(
        next(row["file"] for row in commands if row["file"].endswith("/signal.cpp"))
    )
    source = folder / "signals-source.cpp"
    source.write_text(f'#include "{signal}"\n' + ADAPTER.read_text())
    binary, receipt = console_adapter(
        build, folder, env, source, "signals", replace_objects=("/src/signal.cpp.o",)
    )
    receipt["adapter_sha256"] = hashlib.sha256(ADAPTER.read_bytes()).hexdigest()
    receipt["signal_source_sha256"] = hashlib.sha256(signal.read_bytes()).hexdigest()
    return binary, receipt


def prepare(scenario, binaries, builds, out, timeout, env, result):
    if "signals" not in scenario:
        return scenario
    import migration

    from .rails import normalize
    from .runtime import prepared_adapter

    folder = out / scenario["name"] / "input"
    source, receipt = normalize(Path(scenario["save"]), folder / "normalized")
    result["signals_input"] = receipt
    native_builds = {
        "reference": migration.REFERENCE_BUILD,
        "candidate": migration.REFERENCE_BUILD
        if binaries["reference"] == binaries["candidate"]
        else core.ROOT / "build-rust",
    }
    executables, provenance = {}, {}
    for role in ("reference", "candidate"):
        prepared = prepared_adapter("signals", role, binaries, builds, folder / role)
        if prepared is not None:
            executables[role], provenance[role] = prepared
        else:
            lock = (
                migration.reference_lock(shared=True)
                if native_builds[role] == migration.REFERENCE_BUILD
                else migration.candidate_lock(native_builds[role], shared=True)
            )
            with lock:
                executables[role], provenance[role] = adapter(
                    native_builds[role], folder / role, env
                )
        if (
            provenance[role]["ordinary_binary_sha256"]
            != hashlib.sha256(binaries[role].read_bytes()).hexdigest()
        ):
            raise RuntimeError("signal adapter differs from frozen build")
        for name in core.RUNTIME_DIRECTORIES:
            if (builds[role] / name).is_dir():
                (folder / role / name).symlink_to(
                    builds[role] / name, target_is_directory=True
                )
    result["signals_adapter"] = provenance
    return dict(scenario, save=str(source), executables=executables)


def check(scenario, run, mode, role, result):
    if "signals" not in scenario or run["exit"] or not run["snapshots"]:
        return
    lines = [line for line in run["log"] if line.startswith("SIGNALS ")]
    for name in (
        "initial",
        "duplicates",
        "public-flush",
        "below-threshold",
        "forced-flush",
        "private-flush",
        "commands",
    ):
        if not any(line.startswith(f"SIGNALS pending {name} ") for line in lines):
            result["problems"].append(
                f"{mode}/{role}: missing signal pending-state witness {name}"
            )
    if "SIGNALS rng true" not in lines or not any(
        line.startswith("SIGNALS states ") for line in lines
    ):
        result["problems"].append(f"{mode}/{role}: missing signal return/RNG witness")
    key = mode + "_signals_witnesses"
    if role == "reference":
        result[key] = lines
    elif result[key] != lines:
        result["problems"].append(
            f"{mode}: native signal state/command witnesses differ"
        )
    counts = core.branch_witnesses(
        scenario,
        run,
        role,
        "signal",
        ("updates", "tiles", "forced_flush", "changed", "train"),
    )
    if counts is not None:
        result[mode + "_signals_profile"] = counts
