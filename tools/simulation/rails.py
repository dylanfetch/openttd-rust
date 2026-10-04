"""Owner-built rail fixture; the existing ship route is also observed (#86)."""

import hashlib
import lzma
from pathlib import Path

from .core import ROOT, SNAPSHOT_TICKS, TICKS_PER_DAY, decode_element, read_save
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
    return cases


def prepare(scenario, binaries, builds, out, timeout, env, result):
    if not scenario.get("rail_fixture"):
        return scenario
    source = Path(scenario["save"])
    packed = source.read_bytes()
    # The owner's original XZ save remains committed byte-for-byte. Prepare an
    # uncompressed INPUT with only optional GLOG history removed, so both builds
    # append their load revision. Every OUTPUT field still compares normally.
    data = b"OTTN" + packed[4:8] + lzma.decompress(packed[8:])
    directory = out / scenario["name"] / "prepare"
    directory.mkdir(parents=True, exist_ok=True)
    original = directory / "owner-uncompressed.sav"
    original.write_bytes(data)
    chunks = read_save(original)
    begin, end = chunks["GLOG"]["span"]
    normalized = directory / "rail-input.sav"
    normalized_data = data[:begin] + data[end:]
    normalized.write_bytes(normalized_data)
    check = read_save(normalized)
    if set(check) != set(chunks) - {"GLOG"} or any(
        data[slice(*chunks[cid]["span"])] != normalized_data[slice(*check[cid]["span"])]
        for cid in check
    ):
        raise RuntimeError("rail input changed outside optional GLOG history")
    result["rail_input"] = {
        "source": str(source),
        "source_sha256": hashlib.sha256(packed).hexdigest(),
        "normalized_sha256": hashlib.sha256(normalized_data).hexdigest(),
        "removed_chunk": "GLOG",
    }
    return dict(scenario, save=str(normalized))


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
