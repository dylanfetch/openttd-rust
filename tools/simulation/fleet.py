"""Fleet hierarchy, renewal and actual replacement transactions."""

import hashlib
import json
import shlex
import subprocess
from pathlib import Path

from .core import ROOT, RUNTIME_DIRECTORIES, compare_saves, run_game

ADAPTER = ROOT / "tools/simulation/fleet-console.cpp"


def scenarios(soak):
    cases = [
        {
            "name": "fleet-" + mode,
            "kind": "save",
            "save": str(
                ROOT
                / "migration/saves"
                / (
                    "padhattan-ridge-1996.sav"
                    if mode.startswith("train")
                    else "water-ferry.sav"
                    if mode == "ship"
                    else "aircraft-route.sav"
                    if mode == "aircraft"
                    else "opus-55-167-002.sav"
                )
            ),
            "fleet": mode,
            "short_checkpoint": True,
            "ticks": 74 if soak else 1,
            "console": ["fleet_scenario " + mode, "unpause"],
        }
        for mode in (
            "groups",
            "list",
            "replace",
            "cash",
            "train",
            "train-rollback",
            "train-wagons",
            "ship",
            "aircraft",
        )
    ]

    cases.append(dict(cases[0], name="fleet-reload", fleet="reload"))
    return cases


def adapter(build, folder, env):
    """Relink this fleet-only adapter with the game's unchanged built objects."""
    folder.mkdir(parents=True, exist_ok=True)
    commands = json.loads((build / "compile_commands.json").read_text())
    entry = next(item for item in commands if item["file"].endswith("/console.cpp"))
    compile_args = shlex.split(entry["command"])
    compile_args[compile_args.index("-o") + 1] = str(folder / "fleet.o")
    compile_args[compile_args.index("-c") + 1] = str(ADAPTER)
    compile_args += ["-I" + str(Path(entry["file"]).parent)]
    subprocess.run(compile_args, cwd=entry["directory"], env=env, check=True)
    target = "openttd-rust" if (build / "openttd-rust").exists() else "openttd"
    ordinary_hash = hashlib.sha256((build / target).read_bytes()).hexdigest()
    command = subprocess.check_output(
        ["ninja", "-t", "commands", target], cwd=build, env=env, text=True
    ).splitlines()[-1]
    link_args = shlex.split(command.removeprefix(": && ").removesuffix(" && :"))
    binary = folder / "openttd-fleet"
    link_args[link_args.index("-o") + 1] = str(binary)
    link_args = [
        arg for arg in link_args if not arg.startswith("-Wl,--dependency-file=")
    ]
    link_args += [str(folder / "fleet.o"), "-Wl,--wrap=_Z22IConsoleStdLibRegisterv"]
    subprocess.run(link_args, cwd=build, env=env, check=True)
    return binary, {
        "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "ordinary_binary_sha256": ordinary_hash,
        "adapter_sha256": hashlib.sha256(ADAPTER.read_bytes()).hexdigest(),
        "compile": compile_args,
        "link": link_args,
    }


def prepare(scenario, binaries, builds, out, timeout, env, result):
    if "fleet" not in scenario:
        return scenario
    import migration

    folder = out / scenario["name"] / "input"
    if scenario["fleet"].startswith("train"):
        from .rails import normalize

        source, receipt = normalize(Path(scenario["save"]), folder / "normalized")
        result["fleet_input"] = receipt
        scenario = dict(scenario, save=str(source))
    native_builds = {
        "reference": migration.REFERENCE_BUILD,
        "candidate": migration.REFERENCE_BUILD
        if binaries["reference"] == binaries["candidate"]
        else ROOT / "build-rust",
    }
    executables, provenance = {}, {}
    for role in ("reference", "candidate"):
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
            raise RuntimeError("fleet adapter differs from frozen build")
        for name in RUNTIME_DIRECTORIES:
            if (builds[role] / name).is_dir():
                (folder / role / name).symlink_to(
                    builds[role] / name, target_is_directory=True
                )
    result["fleet_adapter"] = provenance
    if scenario["fleet"] == "reload":
        saves, capture = {}, {}
        setup = dict(
            scenario, console=["fleet_scenario groups-store", "unpause"], ticks=1
        )
        for role in ("reference", "candidate"):
            run = run_game(
                setup,
                executables[role],
                builds[role],
                folder / role / "capture",
                timeout,
                env,
                desync=False,
            )
            if (
                run["exit"]
                or not run["snapshots"]
                or "FLEET groups true 1 1 true" not in run["log"]
            ):
                raise RuntimeError("fleet reload capture lacks live groups/rules")
            saves[role] = run["snapshots"][-1]
            capture[role] = {
                "input_sha256": hashlib.sha256(saves[role].read_bytes()).hexdigest(),
                "witnesses": [line for line in run["log"] if line.startswith("FLEET ")],
            }
        differences = compare_saves(
            saves["reference"],
            saves["candidate"],
            scenario["name"],
            20,
            result["stats"],
        )
        if (
            differences
            or capture["reference"]["witnesses"] != capture["candidate"]["witnesses"]
        ):
            result["differences"].extend(differences)
            raise RuntimeError("fleet reload capture differs")
        result["fleet_capture"] = capture
        return dict(
            scenario,
            executables=executables,
            role_inputs={role: {"save": str(save)} for role, save in saves.items()},
            console=["fleet_scenario inspect", "unpause"],
        )
    return dict(scenario, executables=executables)


def check(scenario, run, mode, role, result):
    if "fleet" not in scenario or run["exit"] or not run["snapshots"]:
        return
    required = {
        "reload": ["FLEET reloaded true 1 true"],
        "list": [
            "FLEET cost list-test true 0 65535",
            "FLEET cost list-wide-id false 0 65535",
            "FLEET cost list-invalid false 0 65535",
            "FLEET list-rejected true true true true",
            "FLEET list-group true true true",
        ],
        "train-wagons": [
            "FLEET free-replaced true true true true",
            "FLEET wagons-replaced true true true 6 true 0",
        ],
        "groups": [
            "FLEET rule all true true",
            "FLEET rule protected true false",
            "FLEET rule inherited true true",
            "FLEET groups true 1 1 true",
            "FLEET deleted true true true",
        ],
        **{
            operation: [
                "FLEET test-rng true original true",
                "FLEET test-cargo true",
                "FLEET replaced true",
                "FLEET transferred true true",
            ]
            for operation in ("replace", "train", "ship", "aircraft")
        },
        "cash": [
            "FLEET test-rng true original true",
            "FLEET rollback true true true true",
        ],
        "train-rollback": [
            "FLEET test-rng true original true",
            "FLEET rollback true true true true",
        ],
    }[scenario["fleet"]]
    for line in required:
        if line not in run["log"]:
            result["problems"].append(f"{mode}/{role}: missing {line}")
    witnesses = [line for line in run["log"] if line.startswith("FLEET ")]
    key = mode + "_fleet_witnesses"
    if role == "reference":
        result[key] = witnesses
    elif result[key] != witnesses:
        result["problems"].append(f"{mode}: native fleet witnesses differ")
