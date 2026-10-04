"""Station service evidence from the existing road/CAPY reference preparation."""

import hashlib
import struct
from pathlib import Path

from . import core, economy
from .core import ROOT, SNAPSHOT_TICKS, decode_element, field_spans, read_save
from .play_saves import DISTRIBUTIONS

PREPARE_ORDER = 5
PREFIX = "normal[0]/"
VEHICLE = "roadveh[0]/common[0]/"


def scenarios(soak):
    return [
        {
            "name": f"stations-{mode}",
            "kind": "save",
            "save": str(ROOT / "migration/saves/grok-159-001.sav"),
            "console": DISTRIBUTIONS[
                "cargodist" if mode in ("cargodist", "stale-refresh") else "manual"
            ],
            "economy": "cargodist"
            if mode in ("cargodist", "stale-refresh")
            else "stockpile"
            if mode in ("acceptance-loss", "full-gradual")
            else "manual",
            "station_service": mode,
            "ticks": 32
            if mode == "acceptance-loss"
            else (8 if soak else 4) * SNAPSHOT_TICKS,
            "snapshot_minimum": 0 if mode == "acceptance-loss" else 2,
        }
        for mode in (
            "manual",
            "cargodist",
            "acceptance-loss",
            "full-gradual",
            "rating-expire",
            "rating-cap",
            "stale-refresh",
            "stale-remove",
        )
    ]


def prepare(scenario, binaries, builds, out, timeout, env, result):
    if "station_service" not in scenario:
        return scenario
    original = read_save(Path(scenario["save"]))
    source = Path(scenario["save"]).read_bytes()
    data = bytearray(source)
    patches = []

    def patch(cid, index, field, value):
        chunk = original[cid]
        body = dict(chunk["elements"])[index]
        begin, end, kind = field_spans(body, chunk["header"])[field]
        if source.count(body, *chunk["span"]) != 1:
            raise RuntimeError("ambiguous station input row")
        encoded = struct.pack(core.FILE_TYPES[kind & 15][0], value)
        if len(encoded) != end - begin:
            raise RuntimeError("station patch width mismatch")
        start = source.index(body, *chunk["span"]) + begin
        data[start : start + len(encoded)] = encoded
        patches.append((cid, index, field, value))

    mode = scenario["station_service"]
    stations = economy.rows(original, "STNN")
    if not any(row.get(PREFIX + "loading_vehicles") for row in stations.values()):
        raise RuntimeError("station input lacks a real active loading queue")
    if mode in ("acceptance-loss", "full-gradual"):
        front = economy.pending(original)
        if front is None:
            raise RuntimeError("station input lacks staged coal unloading")
        vehicle = economy.rows(original, "VEHS")[front]
        station = vehicle[VEHICLE + "last_station_visited"]
        cargo = vehicle[VEHICLE + "cargo_type"]
        if mode == "acceptance-loss":
            field = PREFIX + f"goods[{cargo}]/status"
            patch("STNN", station, field, stations[station][field] & ~1)
            # This is a short first-unload checkpoint, before natural acceptance
            # polling can restore acceptance. Preserve the already staged packet.
            tick = decode_element(original["DATE"], original["DATE"]["elements"][0][1])[
                "tick_counter"
            ]
            if (tick + station) % 250 > 217:
                raise RuntimeError("acceptance-loss input is too near a big tick")
        else:
            flags = vehicle[VEHICLE + "current_order.flags"]
            patch(
                "VEHS", front, VEHICLE + "current_order.flags", (flags & ~0x70) | 0x20
            )
        result["station_front"] = front
        result["station_target"] = (station, cargo)
    if mode.startswith(("rating-", "stale-")):
        # Stop transport, retaining the real waiting packet at the first rated
        # passenger station. No expected result is changed with this input.
        for index, vehicle in economy.rows(original, "VEHS").items():
            if vehicle["type"] == 1:
                patch(
                    "VEHS",
                    index,
                    VEHICLE + "vehstatus",
                    vehicle[VEHICLE + "vehstatus"] | 2,
                )
    if mode.startswith("stale-"):
        date = economy.rows(original, "DATE")[0]["economy_date"]
        old = date - 2000
        edges = 0
        for index, row in economy.rows(original, "LGRP").items():
            for field in row:
                if field.endswith("/last_unrestricted_update"):
                    patch("LGRP", index, field, old)
                    restricted = field.replace(
                        "last_unrestricted_update", "last_restricted_update"
                    )
                    patch("LGRP", index, restricted, -1)
                    edges += 1
        if not edges:
            raise RuntimeError("station input lacks real stale-link edges")
        result["station_edges"] = (edges, old)
    if mode.startswith("rating-"):
        station = next(
            index
            for index, row in stations.items()
            if row.get(PREFIX + "goods[0]/cargo[0]/second")
            and row[PREFIX + "goods[0]/status"] & 2
        )
        goods = PREFIX + "goods[0]/"
        patch("STNN", station, PREFIX + "base[0]/delete_ctr", 184)
        patch("STNN", station, goods + "time_since_pickup", 254)
        patch("STNN", station, goods + "rating", 1)
        patch("STNN", station, goods + "last_age", 255)
        patch("STNN", station, goods + "last_speed", 1)
        if mode == "rating-cap":
            packet = stations[station][goods + "cargo[0]/second"][0] - 1
            patch("CAPA", packet, "count", 32768)
            patch("STNN", station, goods + "max_waiting_cargo", 32768)
        result["station_target"] = (station, 0)
    prepared = Path(scenario["save"]).with_name("station-input.sav")
    prepared.write_bytes(data)
    read_save(prepared)
    result["station_input"] = {
        "reference_sha256": hashlib.sha256(source).hexdigest(),
        "input_sha256": hashlib.sha256(data).hexdigest(),
        "patches": patches,
        "loading_queues": {
            index: row.get(PREFIX + "loading_vehicles", ())
            for index, row in stations.items()
        },
    }
    scenario = dict(scenario, save=str(prepared))
    if mode not in ("manual", "cargodist"):
        del scenario["economy"]
    commands = [c for c in scenario["console"] if "order.gradual_loading" not in c]
    commands.insert(0, "setting order.gradual_loading 1")
    if mode.startswith("rating-"):
        commands.insert(0, f"setting order.selectgoods {int(mode == 'rating-expire')}")
    return dict(scenario, console=commands)


def check(scenario, run, mode, role, result):
    if "station_service" not in scenario or not run["snapshots"]:
        return
    saved = [read_save(path) for path in run["snapshots"]]
    before = read_save(Path(scenario["save"]))
    kind = scenario["station_service"]
    if kind.startswith("stale-"):
        count, old = result["station_edges"]
        updates = [
            value
            for row in economy.rows(saved[-1], "LGRP").values()
            for field, value in row.items()
            if field.endswith("/last_unrestricted_update")
        ]
        if kind == "stale-remove" and len(updates) >= count:
            result["problems"].append(f"{mode}/{role}: no stale edge was removed")
        if kind == "stale-refresh" and not any(value > old for value in updates):
            result["problems"].append(f"{mode}/{role}: no stale edge was refreshed")
        result[f"{mode}_{role}_station_edges"] = (count, len(updates))
        return
    if kind in ("manual", "cargodist"):
        queues = [
            row.get(PREFIX + "loading_vehicles", ())
            for chunks in saved
            for row in economy.rows(chunks, "STNN").values()
        ]
        fractions = [
            value
            for chunks in saved
            for row in economy.rows(chunks, "STNN").values()
            for field, value in row.items()
            if field.endswith("/amount_fract") and value
        ]
        if mode == "snapshots" and (not any(queues) or not fractions):
            result["problems"].append(
                f"{mode}/{role}: no active queue/fraction witness"
            )
        result[f"{mode}_{role}_station_queues"] = sum(bool(q) for q in queues)
        return
    station, cargo = result["station_target"]
    goods = PREFIX + f"goods[{cargo}]/"
    rows = [economy.rows(chunks, "STNN")[station] for chunks in saved]
    if kind == "rating-expire":
        if rows[-1][goods + "status"] & 2 or rows[-1][goods + "last_speed"]:
            result["problems"].append(f"{mode}/{role}: rating did not expire")
    elif kind == "rating-cap":
        counts = [
            sum(packet["count"] for packet in economy.rows(chunks, "CAPA").values())
            for chunks in (before, saved[-1])
        ]
        if counts[1] >= counts[0]:
            result["problems"].append(f"{mode}/{role}: waiting cargo did not truncate")
        result[f"{mode}_{role}_packet_counts"] = counts
    else:
        front = result["station_front"]
        vehicle = economy.rows(saved[-1], "VEHS")[front]
        if kind == "full-gradual":
            if vehicle[VEHICLE + "vehicle_flags"] & 1:
                result["problems"].append(
                    f"{mode}/{role}: full load finished while cargo unavailable"
                )
        elif not vehicle[VEHICLE + "cargo.action_counts"][2]:
            result["problems"].append(
                f"{mode}/{role}: acceptance loss did not retain staged delivery"
            )
