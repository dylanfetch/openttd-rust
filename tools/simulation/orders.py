"""Orders command, backup and initialized unbunching evidence on the player save."""

import hashlib
import json
import shlex
import subprocess
from pathlib import Path

from .core import RUNTIME_DIRECTORIES, ROOT, decode_element, read_save, run_game

ADAPTER = ROOT / "tools/simulation/orders-console.cpp"
COMMON = "roadveh[0]/common[0]/"


def scenarios(soak):
    return [
        {
            "name": "orders-" + mode,
            "kind": "save",
            "save": str(ROOT / "migration/saves/opus-55-167-002.sav"),
            "orders": mode,
            "short_checkpoint": True,
            "ticks": 74 if soak else 1,
            "console": ["unpause"],
        }
        for mode in (
            "commands",
            "offline-reload",
            "client-restore",
            "client-shared-restore",
            "active-reload",
        )
    ]


def game_environment(scenario):
    return {"OPENTTD_ORDERS_CLIENT": "1"} if scenario.get("orders_client") else {}


def adapter(build, folder, env):
    """Relink this orders-only adapter with the game's unchanged built objects."""
    folder.mkdir(parents=True, exist_ok=True)
    commands = json.loads((build / "compile_commands.json").read_text())
    entry = next(item for item in commands if item["file"].endswith("/console.cpp"))
    compile_args = shlex.split(entry["command"])
    compile_args[compile_args.index("-o") + 1] = str(folder / "orders.o")
    compile_args[compile_args.index("-c") + 1] = str(ADAPTER)
    compile_args += ["-I" + str(Path(entry["file"]).parent)]
    subprocess.run(compile_args, cwd=entry["directory"], env=env, check=True)
    target = "openttd-rust" if (build / "openttd-rust").exists() else "openttd"
    ordinary_hash = hashlib.sha256((build / target).read_bytes()).hexdigest()
    command = subprocess.check_output(
        ["ninja", "-t", "commands", target], cwd=build, env=env, text=True
    ).splitlines()[-1]
    link_args = shlex.split(command.removeprefix(": && ").removesuffix(" && :"))
    binary = folder / "openttd-orders"
    link_args[link_args.index("-o") + 1] = str(binary)
    link_args = [
        arg for arg in link_args if not arg.startswith("-Wl,--dependency-file=")
    ]
    link_args += [
        str(folder / "orders.o"),
        "-Wl,--wrap=_Z22IConsoleStdLibRegisterv",
        "-Wl,--wrap=_Z13AfterLoadGamev",
    ]
    subprocess.run(link_args, cwd=build, env=env, check=True)
    return binary, {
        "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "ordinary_binary_sha256": ordinary_hash,
        "adapter_sha256": hashlib.sha256(ADAPTER.read_bytes()).hexdigest(),
        "compile": compile_args,
        "link": link_args,
    }


def prepare(scenario, binaries, builds, out, timeout, env, result):
    if "orders" not in scenario:
        return scenario
    import migration

    folder = out / scenario["name"] / "input"
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
            raise RuntimeError("orders adapter objects differ from frozen game build")
        for name in RUNTIME_DIRECTORIES:
            if (builds[role] / name).is_dir():
                (folder / role / name).symlink_to(
                    builds[role] / name, target_is_directory=True
                )
    result["orders_adapter"] = provenance
    mode = scenario["orders"]
    if mode == "commands":
        return dict(
            scenario,
            executables=executables,
            console=["orders_scenario setup", "unpause"],
        )
    backup = folder / "backups.sav"
    shared = mode == "client-shared-restore"
    setup = dict(
        scenario,
        console=[
            f'orders_scenario setup "{backup}" {"shared" if shared else "unique"}',
            "unpause",
        ],
        ticks=1,
    )
    run = run_game(
        setup,
        executables["reference"],
        builds["reference"],
        folder / "capture",
        timeout,
        env,
        desync=False,
    )
    if run["exit"] or "ORDERS backups captured 2" not in run["log"]:
        raise RuntimeError("reference did not capture two actual order backups")
    if len(read_save(backup)["BKOR"]["elements"]) != 1:
        raise RuntimeError("server-policy save has no actual BKOR contents")
    result["orders_capture"] = {
        "input_sha256": hashlib.sha256(backup.read_bytes()).hexdigest(),
        "binary_sha256": provenance["reference"]["binary_sha256"],
        "commands": setup["console"],
        "witnesses": [line for line in run["log"] if line.startswith("ORDERS ")],
    }
    if mode == "active-reload":
        active = dict(
            scenario,
            save=str(backup),
            console=["orders_scenario active", "unpause"],
            ticks=1,
        )
        run = run_game(
            active,
            executables["reference"],
            builds["reference"],
            folder / "active",
            timeout,
            env,
            desync=False,
        )
        if run["exit"] or not any(
            line.startswith("ORDERS state conditional-active ") for line in run["log"]
        ):
            raise RuntimeError("reference did not execute the active conditional order")
        backup = run["snapshots"][-1]
        result["orders_active"] = {
            "input_sha256": hashlib.sha256(backup.read_bytes()).hexdigest(),
            "witnesses": [line for line in run["log"] if line.startswith("ORDERS ")],
        }
    return dict(
        scenario,
        save=str(backup),
        executables=executables,
        orders_client=mode in ("client-restore", "client-shared-restore"),
        console=[
            "orders_scenario restore " + ("shared" if shared else "unique")
            if mode in ("client-restore", "client-shared-restore")
            else "orders_scenario inspect",
            "unpause",
        ],
    )


def check(scenario, run, mode, role, result):
    if "orders" not in scenario or run["exit"] or not run["snapshots"]:
        return
    operation = scenario["orders"]
    required = {
        "commands": ["ORDERS backups captured 2", "ORDERS waiting true true"],
        "offline-reload": ["ORDERS backups offline 0"],
        "client-restore": [
            "ORDERS client-fixups 1 1",
            "ORDERS backups before 1",
            "ORDERS backups after 0",
        ],
        "client-shared-restore": [
            "ORDERS client-fixups 1 1",
            "ORDERS backups before 1",
            "ORDERS backups after 0",
        ],
        "active-reload": ["ORDERS backups offline 0"],
    }[operation]
    for line in required:
        if line not in run["log"]:
            result["problems"].append(f"{mode}/{role}: missing {line}")
    chunk = read_save(run["snapshots"][-1])["VEHS"]
    vehicles = {
        index: decode_element(chunk, body)
        for index, body in chunk["elements"]
        if decode_element(chunk, body)["type"] == 1
    }
    # Compare the actual initialized trip values separately from #83's general mask.
    trips = {index: row[COMMON + "round_trip_time"] for index, row in vehicles.items()}
    key = mode + "_orders_trips"
    if role == "reference":
        result[key] = trips
    elif result[key] != trips:
        result["problems"].append(f"{mode}: unmasked initialized-peer trip mismatch")
    if operation not in ("active-reload", "client-restore") and trips[126] != 3000:
        result["problems"].append(f"{mode}/{role}: no 3000-tick depot trip measurement")
