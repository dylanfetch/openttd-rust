"""Ship transport, water-region cache mutation and reference fixture construction."""

import hashlib
import json
import shutil
import struct
from pathlib import Path

from . import core
from .core import (
    FILE_TYPES,
    ROOT,
    SNAPSHOT_TICKS,
    Reader,
    copy_runtime,
    decode_element,
    field_spans,
    read_save,
    read_value,
    run_game,
    save_moment,
)
from .play_saves import DISTRIBUTIONS

AI_FOLDER = "water-scenario-ai"


def uses_ai(scenario):
    return scenario.get("water") == "structures"


def install(scenario, run_dir):
    if uses_ai(scenario):
        ai = run_dir / "ai/water-scenarios"
        shutil.copytree(scenario["scenario_ai"], ai)
        (ai / "parameters.nut").write_text(
            f'WATER_SHIP <- 22;\nWATER_MODE <- "{scenario["water_operation"]}";\n'
        )


def scenarios(soak):
    scenarios = []
    for distribution, commands in DISTRIBUTIONS.items():
        scenarios.append(
            {
                "name": f"water-ferry-{distribution}",
                "kind": "save",
                "water": "ferry",
                "save": str(ROOT / "migration/saves/water-ferry.sav"),
                "console": commands,
                "ticks": 200000 if soak else 100000,
            }
        )
    for operation in ("route", "mutate", "depot", "reload"):
        scenarios.append(
            {
                "name": f"water-structures-{operation}",
                "kind": "save",
                "water": "structures",
                "water_operation": operation,
                "save": str(ROOT / "migration/saves/water-structures.sav"),
                "console": ["unpause"],
                "ticks": 100000 if soak else 60000,
            }
        )
    return scenarios


def check_water(scenario, run, desync):
    source = Path(scenario["save"])
    receipt = scenario.get("water_receipt") or json.loads(
        source.with_suffix(".json").read_text()
    )
    if hashlib.sha256(source.read_bytes()).hexdigest() != receipt["normalized_sha256"]:
        raise RuntimeError("water fixture differs from its preparation receipt")
    ship = receipt["ship"]

    def observe(path):
        chunk = read_save(path)["VEHS"]
        row = decode_element(chunk, dict(chunk["elements"])[ship])
        if row["type"] != 2:
            raise RuntimeError("water witness is not a ship")
        common = "ship[0]/common[0]/"
        return {
            "tick": save_moment(path)[2],
            **{
                key: row[common + key]
                for key in (
                    "tile",
                    "motion_counter",
                    "profit_this_year",
                    "profit_last_year",
                    "cargo.action_counts",
                    "cur_real_order_index",
                )
            },
            "path_length": sum(k.startswith("ship[0]/path[") for k in row),
        }

    rows = [observe(source), *[observe(path) for path in run["snapshots"]]]
    elapsed = rows[-1]["tick"] - rows[0]["tick"]
    if (
        elapsed < 2 * SNAPSHOT_TICKS
        or rows[-1]["motion_counter"] == rows[0]["motion_counter"]
    ):
        raise RuntimeError("water fixture did not advance a moving ship")
    if not rows[0]["path_length"]:
        raise RuntimeError("water fixture lacks a saved live ship path")
    loaded = any(sum(row["cargo.action_counts"]) > 0 for row in rows)
    paid = any(
        a["profit_last_year"] == b["profit_last_year"]
        and b["profit_this_year"] > a["profit_this_year"]
        for a, b in zip(rows, rows[1:], strict=False)
    )
    if desync and scenario["water"] == "ferry" and not (loaded and paid):
        raise RuntimeError("ferry snapshots lack loaded cargo and a paid delivery")
    markers = [
        line.split("WATER ", 1)[1].split() for line in run["log"] if "WATER " in line
    ]
    operation = scenario.get("water_operation")
    if scenario["water"] == "structures":
        expected = (
            ["ship", "warm-right", "warm-left"]
            + (
                [
                    "close-lock",
                    "lock-lost",
                    "open-lock",
                    "lock-recovered",
                    "close-canal",
                    "canal-lost",
                    "open-canal",
                    "canal-recovered",
                ]
                if operation == "mutate"
                else ["send-depot", "depot-arrival", "restart", "depot-recovered"]
                if operation == "depot"
                else []
            )
            + ["complete"]
        )
        if [row[0] for row in markers] != expected:
            raise RuntimeError("water command/lost/recovery witnesses are incomplete")
    return {
        "input": receipt,
        "elapsed_ticks": elapsed,
        "observations": rows,
        "loaded": loaded,
        "paid": paid,
        "markers": markers,
    }


def prepare(scenario, binaries, builds, out, timeout, env, result):
    if scenario.get("water_operation") != "reload":
        return scenario
    setup = dict(scenario, water_operation="route", ticks=3 * SNAPSHOT_TICKS)
    with core.MACHINE.hold(alone=False):
        run = run_game(
            setup,
            binaries["reference"],
            builds["reference"],
            out / scenario["name"] / "prepare",
            timeout,
            env,
        )
    if run["exit"] != 0:
        raise RuntimeError("ship reload preparation failed")
    ship = json.loads(Path(scenario["save"]).with_suffix(".json").read_text())["ship"]
    fixture = next(
        (
            path
            for path in run["snapshots"]
            if any(
                key.startswith("ship[0]/path[")
                for key in decode_element(
                    read_save(path)["VEHS"],
                    dict(read_save(path)["VEHS"]["elements"])[ship],
                )
            )
        ),
        None,
    )
    if fixture is None:
        raise RuntimeError("ship reload has no saved live path")
    original, data = read_save(fixture), fixture.read_bytes()
    begin, end = original["GLOG"]["span"]
    normalized = fixture.parent / "ship-reload-input.sav"
    normalized.write_bytes(data[:begin] + data[end:])
    chunks = read_save(normalized)
    if set(chunks) != set(original) - {"GLOG"} or any(
        data[slice(*original[cid]["span"])]
        != normalized.read_bytes()[slice(*chunks[cid]["span"])]
        for cid in chunks
    ):
        raise RuntimeError("ship reload input changed outside GLOG")
    receipt = {
        "ship": ship,
        "reference_sha256": hashlib.sha256(data).hexdigest(),
        "normalized_sha256": hashlib.sha256(normalized.read_bytes()).hexdigest(),
        "removed_chunk": "GLOG",
    }
    result["water_reload_input"] = receipt
    return dict(scenario, save=str(normalized), water_receipt=receipt)


def check(scenario, run, mode, role, result):
    if "water" in scenario:
        result[f"{mode}_{role}_water"] = check_water(scenario, run, mode == "snapshots")
        profile = run["snapshots"][-1].parents[2] / "water-profile.json"
        if profile.is_file():
            result[f"{mode}_{role}_water_profile"] = json.loads(profile.read_text())
        profile = run["snapshots"][-1].parents[2] / "ship-yapf-profile.json"
        if profile.is_file():
            branches = json.loads(profile.read_text())
            required = [
                "region_nodes",
                "track_nodes",
                "cache_truncations",
                "final_region_clears",
                "reverse_chosen",
            ]
            required += (
                ["intermediate", "alternate_docking"]
                if scenario["water"] == "ferry"
                else ["retries", "lost", "random_draws", "blocked_calls"]
            )
            if any(not branches[key] for key in required):
                raise RuntimeError("ship YAPF branch witnesses are incomplete")
            result[f"{mode}_{role}_ship_yapf_profile"] = branches


def prepare_water_save(layout, migration, out, timeout):
    """Build inputs with reference commands; only funding/AI identity are patched."""
    runtime = out / "reference-runtime"
    with migration.reference_lock(shared=True):
        binary = copy_runtime(
            migration.REFERENCE_BUILD, migration.REFERENCE_BUILD / "openttd", runtime
        )
    scripts = out / "water-ai"
    shutil.copytree(ROOT / "tools/water-scenario-ai", scripts)
    scenario = {
        "kind": "save",
        "save": str(migration.REFERENCE / "regression/stationlist/test.sav"),
        "ticks": 1,
        "console": ["unpause"],
    }
    with core.MACHINE.hold(alone=False):
        initial = run_game(
            scenario,
            binary,
            runtime,
            out / "initial",
            timeout,
            migration.environment(),
            False,
        )
    if initial["exit"] != 0 or not initial["snapshots"]:
        raise RuntimeError("water preparation could not emit the original save")
    emitted = initial["snapshots"][-1]
    data, chunks = bytearray(emitted.read_bytes()), read_save(emitted)

    def element_offset(chunk, wanted):
        reader = Reader(data, chunk["span"][0] + 5)
        reader.take(reader.gamma() - 1)
        for index, body in chunk["elements"]:
            size = reader.gamma() - 1
            begin = reader.pos
            if chunk["kind"] == 4 and reader.gamma() != index:
                raise RuntimeError("water fixture sparse index mismatch")
            offset = reader.pos
            if reader.take(size - (offset - begin)) != body:
                raise RuntimeError("water fixture table body mismatch")
            if index == wanted:
                return offset, body
        raise RuntimeError("water fixture company disappeared")

    funding = None
    if layout == "structures":
        company = chunks["PLYR"]
        offset, body = element_offset(company, 1)
        begin, end, kind = field_spans(body, company["header"])["money"]
        old = struct.unpack(FILE_TYPES[kind & 15][0], body[begin:end])[0]
        replacement = struct.pack(FILE_TYPES[kind & 15][0], 10_000_000)
        if kind != 7 or len(replacement) != end - begin:
            raise RuntimeError("water fixture money schema changed")
        data[offset + begin : offset + end] = replacement
        funding = {"company": 1, "field": "money", "old": old, "new": 10_000_000}
    funded = out / "funded-input.sav"
    funded.write_bytes(data)
    # Check every field and chunk around the sole optional funding edit.
    funded_chunks = read_save(funded)
    for cid, chunk in chunks.items():
        if cid == "PLYR" and funding:
            for (index, before), (other, after) in zip(
                chunk["elements"], funded_chunks[cid]["elements"], strict=True
            ):
                a, b = decode_element(chunk, before), decode_element(chunk, after)
                if index == 1:
                    a["money"] = funding["new"]
                if index != other or a != b:
                    raise RuntimeError("funding changed another company field")
        elif (
            emitted.read_bytes()[slice(*chunk["span"])]
            != data[slice(*funded_chunks[cid]["span"])]
        ):
            raise RuntimeError("funding changed another chunk")
    shutil.copy2(scripts / "setup.nut", runtime / "ai/stationlist/main.nut")
    (runtime / "ai/stationlist/parameters.nut").write_text(
        f'WATER_SETUP <- "{layout}";\n'
    )
    scenario.update(save=str(funded), ticks=6000)
    with core.MACHINE.hold(alone=False):
        built = run_game(
            scenario,
            binary,
            runtime,
            out / "built",
            timeout,
            migration.environment(),
            False,
        )
    markers = [
        line.split("WATER-SETUP-END ", 1)[1]
        for line in built["log"]
        if "WATER-SETUP-END " in line
    ]
    if (
        built["exit"] != 0
        or len(markers) != 1
        or not markers[0].startswith(layout + " ")
    ):
        raise RuntimeError(
            "water construction did not complete; see retained preparation logs"
        )
    raw = built["snapshots"][-1]
    data, chunks = bytearray(raw.read_bytes()), read_save(raw)
    chunk = chunks["AIPL"]
    offset, body = element_offset(chunk, 1)
    if chunk["header"] != [
        {"key": "name", "type": 26},
        {"key": "settings", "type": 26},
        {"key": "version", "type": 6},
    ]:
        raise RuntimeError("water AI configuration schema changed")
    reader, renamed = Reader(body), []
    for key, kind in (
        ("name", 10),
        ("settings", 10),
        ("version", 6),
        ("running_name", 10),
    ):
        begin = reader.pos
        value = read_value(reader, kind)
        if key in ("name", "running_name"):
            if value.casefold() != "stationlist":
                raise RuntimeError(
                    "water preparation did not use its isolated setup AI"
                )
            # Equal-length names preserve every length and all AI saved-data bytes.
            data[offset + begin + 1 : offset + reader.pos] = b"WaterScenes"
            renamed.append([offset + begin + 1, value, "WaterScenes"])
    glog = chunks["GLOG"]["span"]
    destination = ROOT / "migration/saves" / f"water-{layout}.sav"
    destination.write_bytes(data[: glog[0]] + data[glog[1] :])
    normalized = read_save(destination)
    if set(normalized) != set(chunks) - {"GLOG"}:
        raise RuntimeError("water normalization changed chunk inventory")
    for cid, chunk in normalized.items():
        if (
            bytes(data[slice(*chunks[cid]["span"])])
            != destination.read_bytes()[slice(*chunk["span"])]
        ):
            raise RuntimeError("water normalization changed unrelated bytes")
    if destination.stat().st_size >= 1_000_000:
        raise RuntimeError("water fixture exceeds the committed-save budget")
    receipt = {
        "baseline": migration.BASELINE,
        "layout": layout,
        "ship": int(markers[0].split()[1]),
        "reference_binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "source_sha256": hashlib.sha256(
            Path(initial["snapshots"][-1]).read_bytes()
        ).hexdigest(),
        "raw_sha256": hashlib.sha256(raw.read_bytes()).hexdigest(),
        "normalized_sha256": hashlib.sha256(destination.read_bytes()).hexdigest(),
        "setup_sha256": hashlib.sha256(
            (scripts / "setup.nut").read_bytes()
        ).hexdigest(),
        "funding": funding,
        "ai_renames": renamed,
        "removed_chunk": "GLOG",
        "setup_markers": markers,
        "elapsed_ticks": save_moment(raw)[2] - save_moment(funded)[2],
    }
    destination.with_suffix(".json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(f"Prepared {destination}; raw inputs and logs: {out}")


def profile_report(path):
    """Summarize per-process map crossings and unchanged-reference run timings."""
    report = json.loads(Path(path).read_text())
    rows = []
    for case in report["results"]:
        for mode in ("snapshots", "plain"):
            profile = case.get(f"{mode}_candidate_water_profile")
            if profile is None:
                branches = case.get(f"{mode}_candidate_ship_yapf_profile")
                if branches is not None:
                    rows.append(
                        {"scenario": case["scenario"], "mode": mode, **branches}
                    )
                continue
            rows.append(
                {
                    "scenario": case["scenario"],
                    "mode": mode,
                    **profile,
                    "map_crossings": profile["tracks"]
                    + profile["follows"]
                    + profile["aqueducts"],
                    "max_returned_scalar_bytes": 2 * profile["tracks"]
                    + 5 * profile["follows"]
                    + 4 * profile["aqueducts"],
                    "reference_seconds": case[f"{mode}_reference_seconds"],
                    "candidate_seconds": case[f"{mode}_candidate_seconds"],
                }
            )
    if not rows:
        raise RuntimeError("report has no water/ship profile measurements")
    return rows


if __name__ == "__main__":
    import sys

    print(json.dumps(profile_report(sys.argv[1]), indent=2))
