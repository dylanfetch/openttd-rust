"""Short road-controller branch witnesses from the committed player saves (#121)."""

import hashlib
import lzma
from pathlib import Path

from .core import ROOT, decode_element, read_save, run_game
from .disasters import patch

ROAD = "roadveh[0]/"
COMMON = ROAD + "common[0]/"


def scenarios(soak):
    # These transient windows stay short in soak; play saves cover longer runs.
    return [
        {
            "name": f"roads-{operation}",
            "kind": "save",
            "short_checkpoint": True,
            "roads": operation,
            "save": str(ROOT / "migration/saves" / f"{source}.sav"),
            "console": [
                f"setting vehicle.roadveh_acceleration_model {model}",
                "unpause",
            ],
            "ticks": ticks,
        }
        for operation, source, ticks, model in (
            ("original", "opus-55-167-002", 30, 0),
            ("realistic", "opus-55-167-002", 30, 1),
            ("invalidate", "opus-55-167-002", 30, 1),
            ("blocking-failsafe", "opus-55-167-002", 100, 1),
            ("overtake-start", "grok-159-001", 1, 1),
            ("overtake-timeout", "grok-159-001", 30, 1),
            ("reload", "grok-159-001", 30, 1),
            ("depot-service", "opus-55-167-002", 12, 1),
        )
    ]


def rows(path):
    chunk = read_save(path)["VEHS"]
    return {
        index: fields
        for index, body in chunk["elements"]
        if (fields := decode_element(chunk, body))["type"] == 1
    }


def path_cache(row):
    return [
        (row[f"{ROAD}path[{index}]/trackdir"], row[f"{ROAD}path[{index}]/tile"])
        for index in range(sum(key.endswith("/trackdir") for key in row))
    ]


def position(row):
    return tuple(row[COMMON + name] for name in ("tile", "x_pos", "y_pos"))


def prepare(scenario, binaries, builds, out, timeout, env, result):
    if "roads" not in scenario:
        return scenario
    folder = out / scenario["name"] / "input"
    folder.mkdir(parents=True, exist_ok=True)
    source = Path(scenario["save"])
    packed = source.read_bytes()
    unpacked = folder / "player.sav"
    unpacked.write_bytes(b"OTTN" + packed[4:8] + lzma.decompress(packed[8:]))
    operation, changes = scenario["roads"], {}
    if operation == "invalidate":
        # At the next junction, the tail now names the wrong tile. The earlier
        # void-tile entry must also disappear, witnessing whole-cache rejection.
        changes = {
            "VEHS": {3: {ROAD + "path[0]/tile": 16383, ROAD + "path[3]/tile": 10814}}
        }
    if operation == "overtake-start":
        # Restore the naturally overtaking truck to its normal lane immediately
        # behind the existing broken-down truck 74; do not seed an active overtake.
        changes = {
            "VEHS": {
                38: {
                    ROAD + "overtaking": 0,
                    ROAD + "overtaking_ctr": 0,
                    COMMON + "x_pos": 969,
                }
            }
        }
    # Loaded overdue link jobs can spend every null-driver iteration waiting
    # for threads. Keep their relative join order, beyond these transient windows.
    jobs = read_save(unpacked)["LGRJ"]
    changes["LGRJ"] = {
        index: {"join_date": decode_element(jobs, body)["join_date"] + 32}
        for index, body in jobs["elements"]
    }
    fixture = folder / "road-input.sav"
    receipt = patch(unpacked, fixture, changes)
    receipt.update(
        committed_source=str(source),
        committed_sha256=hashlib.sha256(packed).hexdigest(),
        console=scenario["console"],
    )
    if operation == "reload":
        run = run_game(
            dict(scenario, save=str(fixture), ticks=1),
            binaries["reference"],
            builds["reference"],
            folder / "reference",
            timeout,
            env,
            False,
        )
        if run["exit"] != 0 or not run["snapshots"]:
            raise RuntimeError("road reload reference preparation failed")
        reference = run["snapshots"][-1]
        row = rows(reference)[65]
        if not path_cache(row) or not row[ROAD + "blocked_ctr"]:
            raise RuntimeError(
                "road reload lacks a live path and active blocked counter"
            )
        fixture = folder / "reload-input.sav"
        receipt["reload"] = patch(reference, fixture, {})
        receipt["reload"]["head"] = 65
    result["road_input"] = receipt
    return dict(scenario, save=str(fixture))


def check(scenario, run, mode, role, result):
    if "roads" not in scenario:
        return
    if not run["snapshots"]:
        raise RuntimeError("road witness has no output save")
    source, final = Path(scenario["save"]), run["snapshots"][-1]
    before, after = rows(source), rows(final)
    chunks = read_save(source)
    operation = scenario["roads"]
    witnesses = {}

    def require(condition, message):
        if not condition:
            raise RuntimeError(f"road {operation}: {message}")

    def observe(index):
        keys = [
            ROAD + key
            for key in ("state", "frame", "blocked_ctr", "overtaking", "overtaking_ctr")
        ] + [
            COMMON + key
            for key in (
                "tile",
                "x_pos",
                "y_pos",
                "direction",
                "z_pos",
                "cur_speed",
                "breakdown_ctr",
                "current_order.type",
                "date_of_last_service",
                "reliability",
                "breakdowns_since_last_service",
            )
        ]
        witnesses[index] = {
            "before": {key.removeprefix(ROAD): before[index][key] for key in keys},
            "after": {key.removeprefix(ROAD): after[index][key] for key in keys},
            "path_before": path_cache(before[index]),
            "path_after": path_cache(after[index]),
        }
        return before[index], after[index]

    def map_tile(tile):
        values = {cid: chunks[cid]["raw"][tile] for cid in ("MAPT", "MAP5")}
        witnesses.setdefault("map", {})[tile] = values
        return values["MAPT"] >> 4, values["MAP5"]

    pats = read_save(final)["PATS"]
    model = decode_element(pats, pats["elements"][0][1])[
        "vehicle.roadveh_acceleration_model"
    ]
    require(
        model == (0 if operation == "original" else 1), "acceleration setting missing"
    )
    witnesses["acceleration_model"] = model

    if operation in ("original", "realistic", "invalidate"):
        a, b = observe(3)
        old, new = path_cache(a), path_cache(b)
        tile, roadbits = map_tile(10815)
        require(
            tile == 2 and roadbits & 15 not in (5, 10),
            "cache choice is not a road junction",
        )
        require(
            b[COMMON + "tile"] == 10815 and b[ROAD + "state"] == 10,
            "cached junction turn missing",
        )
        if operation == "invalidate":
            require(
                old[-1][1] != 10815 and old[0][1] == 16383, "stale input cache missing"
            )
            require(
                new and all(tile not in (16383, 10814) for _, tile in new),
                "stale cache entries survived",
            )
        else:
            require(
                old[-1] == (10, 10815) and new == old[:-1],
                "cached route was not consumed at its junction",
            )

    if operation in ("original", "realistic", "depot-service"):
        a, b = observe(90)
        tile, roadtype = map_tile(a[COMMON + "tile"])
        require(tile == 2 and roadtype >> 6 == 2, "service arrival is not at a depot")
        require(a[COMMON + "current_order.type"] & 15 == 2, "input lacks depot order")
        require(
            b[COMMON + "date_of_last_service"] > a[COMMON + "date_of_last_service"]
            and b[COMMON + "reliability"] > a[COMMON + "reliability"]
            and a[COMMON + "breakdowns_since_last_service"] == 1
            and b[COMMON + "breakdowns_since_last_service"] == 0
            and b[COMMON + "current_order.type"] & 15 == 1
            and b[ROAD + "state"] != 254
            and b[COMMON + "cur_speed"] > 0,
            "depot service/reset/departure missing",
        )

    if operation in ("original", "realistic"):
        a, b = observe(5)
        leader, _ = observe(6)
        require(
            position(a) == position(b)
            and b[COMMON + "cur_speed"] == 0
            and b[ROAD + "blocked_ctr"] > a[ROAD + "blocked_ctr"]
            and leader[COMMON + "x_pos"] - a[COMMON + "x_pos"] == 8
            and leader[COMMON + "y_pos"] == a[COMMON + "y_pos"]
            and leader[COMMON + "z_pos"] == a[COMMON + "z_pos"]
            and leader[COMMON + "direction"] == a[COMMON + "direction"] == 5,
            "nearby same-lane vehicle did not block movement",
        )

    if operation == "blocking-failsafe":
        a, b = observe(17)
        leader, last_leader = observe(36)
        require(
            0 < a[ROAD + "blocked_ctr"] <= 1480 < b[ROAD + "blocked_ctr"]
            and position(a) != position(b)
            and b[COMMON + "cur_speed"] > 0
            and position(leader) == position(last_leader)
            and leader[COMMON + "y_pos"] == a[COMMON + "y_pos"] - 8
            and leader[COMMON + "x_pos"] == a[COMMON + "x_pos"]
            and leader[COMMON + "z_pos"] == a[COMMON + "z_pos"]
            and leader[COMMON + "direction"] == a[COMMON + "direction"] == 7,
            "1480-count blocking escape missing",
        )

    if operation in ("overtake-start", "overtake-timeout", "reload"):
        a, b = observe(38)
        tile, roadbits = map_tile(a[COMMON + "tile"])
        require(tile == 2 and roadbits == 5, "overtake is not on a straight road")
        if operation == "overtake-start":
            obstacle, last_obstacle = observe(74)
            require(
                map_tile(a[COMMON + "tile"] + 128) == (2, 5),
                "next overtake tile is not a straight road",
            )
            require(
                a[ROAD + "overtaking"] == 0
                and a[ROAD + "overtaking_ctr"] == 0
                and b[ROAD + "overtaking"] == 16
                and b[ROAD + "overtaking_ctr"] == 17
                and obstacle[COMMON + "breakdown_ctr"] != 0
                and obstacle[COMMON + "x_pos"] == a[COMMON + "x_pos"]
                and obstacle[COMMON + "z_pos"] == a[COMMON + "z_pos"]
                and obstacle[COMMON + "direction"] == a[COMMON + "direction"] == 3
                and 0 < obstacle[COMMON + "y_pos"] - a[COMMON + "y_pos"] < 8
                and position(obstacle) == position(last_obstacle),
                "broken-down neighbour did not initiate overtaking",
            )
        else:
            require(
                a[ROAD + "overtaking"] == 16
                and 0 < a[ROAD + "overtaking_ctr"] < 35
                and b[ROAD + "overtaking"] == 0
                and b[ROAD + "overtaking_ctr"] == 35
                and position(a) != position(b),
                "active overtaking did not reach its straight-road timeout",
            )
        if operation == "reload":
            a, b = observe(65)
            require(
                path_cache(a)
                and path_cache(a) == path_cache(b)
                and 0 < a[ROAD + "blocked_ctr"] < b[ROAD + "blocked_ctr"]
                and position(a) == position(b),
                "reference reload did not retain a live path and advance the blocked counter",
            )
    result[f"{mode}_{role}_roads"] = witnesses
