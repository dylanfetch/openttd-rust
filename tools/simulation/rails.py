"""Owner-built rail fixture; the existing ship route is also observed (#86)."""

import hashlib
import json
import lzma
import re
from pathlib import Path

from .core import (
    ROOT,
    SNAPSHOT_TICKS,
    TICKS_PER_DAY,
    decode_element,
    read_save,
    run_game,
)
from .play_saves import DISTRIBUTIONS


def scenarios(soak):
    cases = [
        {
            "name": f"play-padhattan-ridge-1996-{distribution}",
            "kind": "save",
            "rail_fixture": True,
            "console": commands,
            "save": str(ROOT / "migration/saves/padhattan-ridge-1996.sav"),
            "ticks": (6 if soak else 2) * 365 * TICKS_PER_DAY + SNAPSHOT_TICKS,
        }
        for distribution, commands in DISTRIBUTIONS.items()
    ]
    cases.append(
        dict(
            cases[0],
            name="rail-padhattan-ridge-1996-realistic",
            console=["setting vehicle.train_acceleration_model 1", "unpause"],
        )
    )
    for forbid90 in (0, 1):
        cases.append(
            dict(
                cases[0],
                name=f"rail-reservation-90-{forbid90}",
                console=[
                    f"setting pf.forbid_90_deg {forbid90}",
                    "setting pf.reserve_paths 1",
                    "setting pf.yapf.rail_look_ahead_max_signals 1",
                    "unpause",
                ],
            )
        )
    cases.append(dict(cases[-2], name="rail-reservation-reload", rail_reload=True))
    return cases


def game_args(scenario):
    return ["-d", "yapf=3"] if scenario.get("rail_fixture") else []


def normalize(source, directory):
    """Keep every original chunk byte except optional GLOG history."""
    packed = source.read_bytes()
    data = (
        b"OTTN" + packed[4:8] + lzma.decompress(packed[8:])
        if packed[:4] == b"OTTX"
        else packed
    )
    directory.mkdir(parents=True, exist_ok=True)
    original = directory / "owner-uncompressed.sav"
    original.write_bytes(data)
    chunks = read_save(original)
    begin, end = chunks["GLOG"]["span"]
    normalized_data = data[:begin] + data[end:]
    normalized = directory / "rail-input.sav"
    normalized.write_bytes(normalized_data)
    check = read_save(normalized)
    if set(check) != set(chunks) - {"GLOG"}:
        raise RuntimeError("rail input changed chunk inventory")
    if any(
        data[slice(*chunks[cid]["span"])] != normalized_data[slice(*check[cid]["span"])]
        for cid in check
    ):
        raise RuntimeError("rail input changed outside optional GLOG history")
    return normalized, {
        "source": str(source),
        "source_sha256": hashlib.sha256(packed).hexdigest(),
        "normalized_sha256": hashlib.sha256(normalized_data).hexdigest(),
        "removed_chunk": "GLOG",
        "typed_edits": [],
        "unchanged_chunks": sorted(check),
    }


def prepare(scenario, binaries, builds, out, timeout, env, result):
    if not scenario.get("rail_fixture"):
        return scenario
    directory = out / scenario["name"] / "prepare"
    normalized, receipt = normalize(Path(scenario["save"]), directory)
    result["rail_input"] = receipt
    scenario = dict(scenario, save=str(normalized))
    if scenario.get("rail_reload"):
        setup = dict(scenario, ticks=8 * SNAPSHOT_TICKS)
        run = run_game(
            setup,
            binaries["reference"],
            builds["reference"],
            directory / "warm",
            timeout,
            env,
        )
        if run["exit"] != 0:
            raise RuntimeError("rail reload preparation failed")
        fixture = next(
            (
                p
                for p in run["snapshots"]
                if reservation_rows(p)["rail"] and reservation_rows(p)["platforms"]
            ),
            None,
        )
        if fixture is None:
            raise RuntimeError("rail reload lacks live track/platform reservations")
        normalized, receipt = normalize(fixture, directory / "reload")
        result["rail_reload_input"] = receipt
        scenario = dict(scenario, save=str(normalized))
    return scenario


def reservation_rows(path):
    """Read current-save reservation fields; comparator still checks every byte."""
    chunks = read_save(path)
    tile_types = chunks["MAPT"]["raw"]
    m2 = chunks["MAP2"]["raw"]
    m5, m6 = chunks["MAP5"]["raw"], chunks["MAPE"]["raw"]
    # rail_map.h: plain railway m5 top bits 00/01, m2 bits8..11.
    # station_map.h: rail station/waypoint subtype0/7, m6 bit2.
    rail = [
        (tile, int.from_bytes(m2[tile * 2 : tile * 2 + 2], "big") >> 8 & 15)
        for tile, kind in enumerate(tile_types)
        if kind >> 4 == 1
        and m5[tile] & 0x80 == 0
        and int.from_bytes(m2[tile * 2 : tile * 2 + 2], "big") >> 8 & 7
    ]
    platforms = [
        tile
        for tile, kind in enumerate(tile_types)
        if kind >> 4 == 5 and m6[tile] >> 3 & 15 in (0, 7) and m6[tile] & 4
    ]
    return {"rail": rail, "platforms": platforms}


SEARCH = re.compile(
    r"\[YAPFt\]([!-])\s*(\d+) - (\d+) rounds - (\d+) open - "
    r"(\d+) closed - CHR\s*([\d.]+)% - C (-?\d+) D (-?\d+)"
)


def search_witnesses(scenario, run, mode):
    searches = [match.groups() for line in run["log"] if (match := SEARCH.search(line))]
    if not searches:
        raise RuntimeError("rail fixture lacks native YAPFt search witnesses")
    cache_hits = sum(float(row[5]) > 0 for row in searches)
    if mode == "plain" and "rail-reservation" in scenario["name"] and not cache_hits:
        raise RuntimeError("plain rail fixture lacks reused cached segment costs")
    reservations = [
        reservation_rows(p) for p in [Path(scenario["save"]), *run["snapshots"]]
    ]
    if "rail-reservation" in scenario["name"] and not any(
        row["rail"] and row["platforms"] for row in reservations[1:]
    ):
        raise RuntimeError("rail fixture lacks saved track/platform reservations")
    return {
        "searches": len(searches),
        "cache_hit_searches": cache_hits,
        "failed_searches": sum(row[0] == "!" for row in searches),
        "max_closed_nodes": max(int(row[4]) for row in searches),
        "reservations": reservations,
    }


def vehicle_rows(path):
    chunk = read_save(path)["VEHS"]
    rows = {}
    for index, body in chunk["elements"]:
        fields = decode_element(chunk, body)
        if fields["type"] not in (0, 2):
            continue
        prefix = "train[0]/common[0]/" if fields["type"] == 0 else "ship[0]/common[0]/"
        rows[index] = {
            name: fields[prefix + name]
            for name in (
                "next",
                "tile",
                "x_pos",
                "y_pos",
                "last_station_visited",
                "motion_counter",
                "cargo.packets",
                "cargo.action_counts",
                "profit_this_year",
                "profit_last_year",
            )
        }
    return rows


def check(scenario, run, mode, role, result):
    if not scenario.get("rail_fixture"):
        return
    result[f"{mode}_{role}_rail_search"] = search_witnesses(scenario, run, mode)
    profile = run["snapshots"][-1].parents[2] / "rail-profile.json"
    if profile.is_file():
        result[f"{mode}_{role}_rail_profile"] = json.loads(profile.read_text())
    paths = [Path(scenario["save"]), *run["snapshots"]]
    observations = [vehicle_rows(path) for path in paths]
    witnesses = {}
    for head in (8, 14, 2):  # Two train heads and the existing passenger ship.
        rows = [snapshot[head] for snapshot in observations]
        positions = {(row["tile"], row["x_pos"], row["y_pos"]) for row in rows}
        loaded = []
        for snapshot in observations:
            index, cargo = head, 0
            while index is not None:
                row = snapshot[index]
                cargo += sum(row["cargo.action_counts"])
                if row["cargo.packets"] and not cargo:
                    raise RuntimeError(f"vehicle {index} has packets without cargo")
                index = row["next"] - 1 if row["next"] else None
            loaded.append(cargo)
        paid = [
            b["profit_this_year"] - a["profit_this_year"]
            for a, b in zip(rows, rows[1:], strict=False)
            if a["profit_last_year"] == b["profit_last_year"]
            and b["profit_this_year"] > a["profit_this_year"]
        ]
        stations = sorted({row["last_station_visited"] for row in rows} - {65535})
        # Plain mode has only the exit checkpoint: a cyclic route can return
        # to its exact starting position, while motion_counter records travel.
        # Delivery revenue is witnessed by the
        # intermediate desync snapshots, which the semantic comparator checks.
        moved = (
            len(positions) > 1
            or rows[-1]["motion_counter"] != rows[0]["motion_counter"]
        )
        if not moved or (
            mode == "snapshots" and (not any(loaded) or not paid or len(stations) < 2)
        ):
            raise RuntimeError(
                f"vehicle {head} lacks movement/cargo/delivery witnesses"
            )
        witnesses[head] = {
            "positions": sorted(positions),
            "motion_counter": [rows[0]["motion_counter"], rows[-1]["motion_counter"]],
            "stations": stations,
            "cargo_counts": loaded,
            "positive_profit_deltas": paid,
        }
    result[f"{mode}_{role}_rail"] = witnesses
