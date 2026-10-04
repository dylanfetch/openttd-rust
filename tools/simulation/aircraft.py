"""Reference-built supplemental aircraft transport fixture (#86)."""

import hashlib
import json
import shutil
from pathlib import Path

from . import core
from .core import (
    ROOT,
    SNAPSHOT_TICKS,
    TICKS_PER_DAY,
    copy_runtime,
    decode_element,
    read_save,
    run_game,
    save_moment,
)
from .play_saves import DISTRIBUTIONS


def scenarios(soak):
    return [
        {
            "name": f"aircraft-route-{distribution}",
            "kind": "save",
            "aircraft_fixture": True,
            "save": str(ROOT / "migration/saves/aircraft-route.sav"),
            "console": commands,
            "ticks": (4 if soak else 2) * 365 * TICKS_PER_DAY + SNAPSHOT_TICKS,
        }
        for distribution, commands in DISTRIBUTIONS.items()
    ]


def game_args(scenario):
    return ["-d", "script=2"] if scenario.get("aircraft_setup") else []


def prepare_aircraft_save(migration, out, timeout):
    """Construct with the pinned reference; comparison never installs the setup AI."""
    runtime = out / "reference-runtime"
    with migration.reference_lock(shared=True):
        binary = copy_runtime(
            migration.REFERENCE_BUILD, migration.REFERENCE_BUILD / "openttd", runtime
        )
    scripts = ROOT / "tools/aircraft-scenario-ai"
    frozen = runtime / "ai/aircraft-setup"
    shutil.copytree(scripts, frozen)
    setup = {
        "kind": "generate",
        "aircraft_setup": True,
        "seed": 86,
        "map_log2": 7,
        "land_generator": 1,
        "ticks": 6000,
        "console": ["start_ai MigrationAircraft", "unpause"],
        "settings": {
            "game_creation": {
                "starting_year": 1980,
                "terrain_type": 0,
                "quantity_sea_lakes": 0,
                "variety": 0,
                "custom_town_number": 20,
            },
            "difficulty": {
                "number_towns": 4,
                "max_loan": 2_000_000,
            },
            "economy": {"initial_city_size": 4},
        },
    }
    with core.MACHINE.hold(alone=False):
        run = run_game(
            setup,
            binary,
            runtime,
            out / "built",
            timeout,
            migration.environment(),
            False,
        )
    markers = [
        line.split("AIRCRAFT-SETUP-END ", 1)[1]
        for line in run["log"]
        if "AIRCRAFT-SETUP-END " in line
    ]
    if run["exit"] != 0 or len(markers) != 1:
        raise RuntimeError(
            f"aircraft construction did not complete; see {out / 'built'}"
        )
    raw = run["snapshots"][-1]
    data, chunks = raw.read_bytes(), read_save(raw)
    begin, end = chunks["GLOG"]["span"]
    destination = ROOT / "migration/saves/aircraft-route.sav"
    destination.write_bytes(data[:begin] + data[end:])
    normalized = read_save(destination)
    if set(normalized) != set(chunks) - {"GLOG"} or any(
        data[slice(*chunks[cid]["span"])]
        != destination.read_bytes()[slice(*normalized[cid]["span"])]
        for cid in normalized
    ):
        raise RuntimeError("aircraft input changed outside optional GLOG history")
    if destination.stat().st_size >= 1_000_000:
        raise RuntimeError("aircraft fixture exceeds committed-save budget")
    if core.read_value(core.Reader(chunks["GSDT"]["elements"][0][1]), 10):
        raise RuntimeError("aircraft fixture contains a GameScript")
    first, last, plane, helicopter = map(int, markers[0].split())
    receipt = {
        "baseline": migration.BASELINE,
        "reference_binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "raw_sha256": hashlib.sha256(data).hexdigest(),
        "normalized_sha256": hashlib.sha256(destination.read_bytes()).hexdigest(),
        "setup_files": {
            path.name: hashlib.sha256(path.read_bytes()).hexdigest()
            for path in sorted(frozen.glob("*.nut"))
        },
        "setup": setup,
        "airports": [first, last],
        "aircraft": {"plane": plane, "helicopter": helicopter},
        "removed_chunk": "GLOG",
        "setup_markers": markers,
        "save_moment": save_moment(raw),
    }
    destination.with_suffix(".json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(f"Prepared {destination}; raw input and logs: {out}")


def check(scenario, run, mode, role, result):
    if not scenario.get("aircraft_fixture"):
        return
    source = Path(scenario["save"])
    receipt = json.loads(source.with_suffix(".json").read_text())
    if hashlib.sha256(source.read_bytes()).hexdigest() != receipt["normalized_sha256"]:
        raise RuntimeError("aircraft fixture differs from its preparation receipt")
    if not any("loading 'dummy' AI" in line for line in run["log"]) or any(
        "AIRCRAFT-BUILD" in line or "AIRCRAFT-SETUP-END" in line for line in run["log"]
    ):
        raise RuntimeError(
            "aircraft comparison did not replace setup logic with the dummy AI"
        )
    paths = [source, *run["snapshots"]]
    observations = []
    for path in paths:
        chunk = read_save(path)["VEHS"]
        observations.append(
            {index: decode_element(chunk, body) for index, body in chunk["elements"]}
        )
    witnesses = {}
    for name, head in receipt["aircraft"].items():
        if any(head not in snapshot for snapshot in observations):
            raise RuntimeError(f"{name} disappeared from aircraft snapshots")
        rows = [snapshot[head] for snapshot in observations]
        if any(row["type"] != 3 for row in rows):
            raise RuntimeError("aircraft witness is not an aircraft")
        prefix = "aircraft[0]/common[0]/"
        positions = {
            tuple(row[prefix + field] for field in ("x_pos", "y_pos", "z_pos"))
            for row in rows
        }
        cargo_counts = []
        for snapshot in observations:
            index, cargo = head, 0
            while index is not None:
                row = snapshot[index]
                cargo += sum(row[prefix + "cargo.action_counts"])
                index = row[prefix + "next"] - 1 if row[prefix + "next"] else None
            cargo_counts.append(cargo)
        paid = [
            b[prefix + "profit_this_year"] - a[prefix + "profit_this_year"]
            for a, b in zip(rows, rows[1:], strict=False)
            if a[prefix + "profit_last_year"] == b[prefix + "profit_last_year"]
            and b[prefix + "profit_this_year"] > a[prefix + "profit_this_year"]
        ]
        stations = sorted(
            {row[prefix + "last_station_visited"] for row in rows} - {65535}
        )
        states = sorted({row["aircraft[0]/state"] for row in rows})
        motion = [row[prefix + "motion_counter"] for row in (rows[0], rows[-1])]
        if (len(positions) < 2 and motion[0] == motion[1]) or (
            mode == "snapshots"
            and (
                not any(cargo_counts)
                or not paid
                or len(stations) < 2
                or 14 not in states
            )
        ):
            raise RuntimeError(
                f"{name} lacks aircraft movement/cargo/delivery witnesses"
            )
        witnesses[name] = {
            "head": head,
            "positions": sorted(positions),
            "motion_counter": motion,
            "stations": stations,
            "airport_states": states,
            "cargo_counts": cargo_counts,
            "positive_profit_deltas": paid,
        }
    result[f"{mode}_{role}_aircraft"] = witnesses
