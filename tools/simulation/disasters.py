"""Disaster scheduler, live targets and lifecycle witnesses (#103)."""

import hashlib
import json
import shutil
import struct
import threading
from pathlib import Path

from . import core
from .core import (
    FILE_TYPES,
    HAS_LENGTH,
    ROOT,
    Reader,
    decode_element,
    field_spans,
    read_save,
    run_game,
    save_moment,
)

AI_FOLDER = "disaster-scenario-ai"
LOCK = threading.Lock()
INVALID = 1048575
SCHEMA = list(
    zip(
        "next subtype tile dest_tile x_pos y_pos z_pos direction owner vehstatus state sprite_cache.sprite_seq.seq[0].sprite age tick_counter image_override big_ufo_destroyer_target flags".split(),
        (6, 2, 6, 6, 5, 5, 5, 2, 2, 2, 4, 4, 5, 2, 6, 6, 2),
        strict=True,
    )
)
FAMILIES = {
    "zeppelin": (1935, 0, (0, 1)),
    "small-ufo": (1945, 1, (2, 3)),
    "airplane": (1965, 1, (4, 5)),
    "helicopter": (1995, 0, (6, 7, 8)),
    "big-ufo": (2005, 0, (9, 10)),
    "small-sub": (1945, 6, (13,)),
    "big-sub": (2005, 6, (14,)),
    "coal": (1956, 6, ()),
}


def uses_ai(scenario):
    return "disasters" in scenario


def install(scenario, run_dir):
    if uses_ai(scenario):
        ai = run_dir / "ai/disasters"
        shutil.copytree(scenario["scenario_ai"], ai)
        (ai / "parameters.nut").write_text(
            f'DISASTER_SETUP <- {str(scenario.get("setup", False)).lower()};\nDISASTER_ACTION <- "{scenario.get("action", "none")}";\nDISASTER_TARGET <- {scenario.get("target", 0)};\n'
        )


def game_args(scenario):
    return (
        ["-d", "script=2"] if uses_ai(scenario) and scenario["kind"] != "save" else []
    )


def gamma(value):
    for size, limit, prefix in (
        (1, 128, 0),
        (2, 16384, 0x8000),
        (3, 2097152, 0xC00000),
        (4, 268435456, 0xE0000000),
    ):
        if value < limit:
            return (value | prefix).to_bytes(size, "big")
    return b"\xf0" + value.to_bytes(4, "big")


def rows(path, cid="VEHS"):
    chunk = read_save(path)[cid]
    result = {index: decode_element(chunk, body) for index, body in chunk["elements"]}
    if any(row is None for row in result.values()):
        raise RuntimeError(f"undecodable disaster evidence {cid}")
    return result


def disasters(path):
    return {
        index: {
            key.removeprefix("disaster[0]/"): value
            for key, value in row.items()
            if key != "type"
        }
        for index, row in rows(path).items()
        if row["type"] == 5
    }


def patch(source, target, changes, ai=None):
    """Only declared typed input fields, AIPL company 1 and optional GLOG change."""
    original, chunks = source.read_bytes(), read_save(source)
    work, allowed, assignments = bytearray(original), set(), []
    for cid, byindex in changes.items():
        chunk = chunks[cid]
        if chunk["kind"] not in (3, 4):
            raise RuntimeError(f"unexpected disaster input table {cid}")
        reader = Reader(original, chunk["span"][0] + 5)
        reader.take(reader.gamma() - 1)
        index, seen = 0, set()
        while length := reader.gamma():
            end = reader.pos + length - 1
            if chunk["kind"] == 4:
                index = reader.gamma()
            begin = reader.pos
            body = reader.take(end - begin)
            if index in byindex:
                fields, locations = (
                    decode_element(chunk, body),
                    field_spans(body, chunk["header"]),
                )
                for key, value in byindex[index].items():
                    lo, hi, kind = locations[key]
                    fmt, width = FILE_TYPES[kind & 15]
                    vals = value if kind & HAS_LENGTH else (value,)
                    at = begin + lo
                    if kind & HAS_LENGTH:
                        array = Reader(body, lo)
                        if array.gamma() != len(vals):
                            raise RuntimeError("disaster input array size differs")
                        at = begin + array.pos
                    if at + len(vals) * width != begin + hi:
                        raise RuntimeError("disaster input field width differs")
                    for value_index, scalar in enumerate(vals):
                        struct.pack_into(fmt, work, at + value_index * width, scalar)
                    allowed.update(range(at, begin + hi))
                    assignments.append([cid, index, key, fields.get(key), value])
                seen.add(index)
            index += 1
        if seen != set(byindex):
            raise RuntimeError("disaster input omitted edited element")
    if any(
        a != b and index not in allowed
        for index, (a, b) in enumerate(zip(original, work, strict=True))
    ):
        raise RuntimeError("disaster input changed undeclared bytes")
    if ai is not None:
        chunk = chunks["AIPL"]
        if chunk["kind"] != 3 or chunk["header"] != [
            {"type": 26, "key": "name"},
            {"type": 26, "key": "settings"},
            {"type": 6, "key": "version"},
        ]:
            raise RuntimeError("unexpected AIPL schema")
        elements = dict(chunk["elements"])
        if ai == "human":
            payload = elements[0]
            if payload != b"\0":
                raise RuntimeError("company zero is not an empty human AI slot")
        else:
            name = b"MigrationDisasters"
            config = gamma(len(name)) + name + b"\0" + struct.pack(">I", 1)
            payload = config * 2 + b"\0"
        elements[1] = payload
        reader = Reader(original, chunk["span"][0] + 5)
        reader.take(reader.gamma() - 1)
        begin, end = reader.pos, chunk["span"][1]
        body = (
            b"".join(
                gamma(len(elements.get(index, b"")) + 1) + elements.get(index, b"")
                for index in range(max(elements) + 1)
            )
            + b"\0"
        )
        work = work[:begin] + body + work[end:]
        assignments.append(["AIPL", 1, "configuration", ai])
    target.write_bytes(work)
    patched = read_save(target)
    if "GLOG" in patched:
        begin, end = patched["GLOG"]["span"]
        work = work[:begin] + work[end:]
        target.write_bytes(work)
    after = read_save(target)
    if set(after) != set(chunks) - {"GLOG"}:
        raise RuntimeError("disaster input changed chunk set")
    for cid in set(after) - set(changes) - {"AIPL" if ai is not None else "GLOG"}:
        if original[slice(*chunks[cid]["span"])] != bytes(
            work[slice(*after[cid]["span"])]
        ):
            raise RuntimeError(f"disaster input changed unrelated {cid}")
    return {
        "source": str(source),
        "source_sha256": hashlib.sha256(original).hexdigest(),
        "sha256": hashlib.sha256(work).hexdigest(),
        "assignments": assignments,
        "removed_chunk": "GLOG",
    }


def calendar(year):
    return year * 365 + (year - 1) // 4 - (year - 1) // 100 + (year - 1) // 400 + 1


def prepare(scenario, binaries, builds, out, timeout, env, result):
    if "disasters" not in scenario:
        return scenario
    folder = out / scenario["name"] / "input"
    folder.mkdir(parents=True, exist_ok=True)

    def run(label, setup, ticks):
        cache = out / "disaster-fixtures" / label
        fixture = cache / "save/autosave/exit.sav"
        if not fixture.exists():
            with core.MACHINE.hold(alone=True):
                trial = run_game(
                    dict(setup, ticks=ticks),
                    binaries["reference"],
                    builds["reference"],
                    cache,
                    timeout,
                    env,
                    False,
                )
            if trial["exit"] or not fixture.exists():
                raise RuntimeError(f"disaster preparation {label} failed")
            (cache / "provenance.json").write_text(
                json.dumps(
                    {
                        "ticks": ticks,
                        "moment": save_moment(fixture),
                        "sha256": hashlib.sha256(fixture.read_bytes()).hexdigest(),
                        "log": trial["log"],
                    },
                    indent=2,
                )
                + "\n"
            )
        return fixture

    with LOCK:
        base = run(
            "world",
            {
                "kind": "generate",
                "seed": 1,
                "map_log2": 8,
                "land_generator": 1,
                "settings": {
                    "game_creation": {"landscape": "temperate", "starting_year": 1950},
                    "difficulty": {"number_industries": 4, "disasters": "false"},
                },
            },
            600,
        )
        kind = scenario["disasters"]
        station = (
            kind.startswith(("zeppelin", "train", "road")) or kind == "small-ufo-crash"
        )
        if station:
            initial = run(
                "stationlist",
                {"kind": "save", "save": str(ROOT / "regression/stationlist/test.sav")},
                1,
            )
            script = out / "disaster-fixtures/script.sav"
            patch(initial, script, {}, "observer")
            setup = dict(
                scenario,
                kind="save",
                save=str(script),
                setup=True,
                console=[
                    "setting difficulty.disasters 0",
                    "setting linkgraph.recalc_time 9000",
                    "setting linkgraph.recalc_interval 4",
                    "unpause",
                ],
            )
            base = run("rail", setup, 3000)
            if not (
                rows(base)[17]["train[0]/tile"] == 10007
                and rows(base)[17]["train[0]/subtype"] & 1
            ):
                raise RuntimeError(
                    "reference rail commands did not park actual front train 17 on plain rail"
                )
            if "large" in kind:
                base = run(
                    "large-airport",
                    dict(
                        scenario,
                        kind="save",
                        save=str(base),
                        action="large-airport",
                        console=["unpause"],
                    ),
                    64,
                )
                if rows(base, "STNN")[0]["airport[0]/type"] != 1:
                    raise RuntimeError("actual large-airport construction failed")
        family = scenario.get(
            "family",
            "zeppelin"
            if kind.startswith("zeppelin")
            else "big-ufo"
            if kind.startswith("train")
            else "small-ufo",
        )
        year, seed, expected = FAMILIES[family]
        if kind == "small-ufo-crash":
            year, seed = 1969, 4
        elif kind.startswith("train"):
            year, seed = 2099, None
        elif kind in ("disabled", "empty-year", "countdown-wrap"):
            year = 2130 if kind == "empty-year" else 1956
        date = {
            "date": calendar(year),
            "date_fract": 73,
            "economy_date_fract": 73,
            "next_disaster_start": 0 if kind == "countdown-wrap" else 1,
        }
        if seed is not None:
            date["random_state"] = (seed, seed)
        changes = {"DATE": {0: date}}
        if kind.startswith("train"):
            changes["PLYR"] = {1: {"is_ai": 0}}
        target = folder / "input.sav"
        receipt = patch(
            base, target, changes, "human" if kind.startswith("train") else None
        )
        schema = next(
            field["fields"]
            for field in read_save(target)["VEHS"]["header"]
            if field["key"] == "disaster"
        )
        if schema != [{"key": key, "type": dtype} for key, dtype in SCHEMA]:
            raise RuntimeError("unexpected VEHS disaster schema")
        console = [
            f"setting difficulty.disasters {int(kind not in ('disabled', 'countdown-wrap'))}",
            "unpause",
        ]
        scenario = dict(
            scenario,
            kind="save",
            save=str(target),
            console=console,
            expected_subtypes=expected,
        )
        receipt["moment"] = save_moment(target)
        result["disaster_input"] = receipt
    return scenario


def check(scenario, run, mode, role, result):
    if "disasters" not in scenario or run["exit"] or not run["snapshots"]:
        return
    path = run["snapshots"][-1]
    elapsed = save_moment(path)[2] - save_moment(Path(scenario["save"]))[2]
    if elapsed != scenario["ticks"]:
        raise RuntimeError(
            f"disaster witness elapsed {elapsed}, expected {scenario['ticks']}"
        )
    live, kind = disasters(path), scenario["disasters"]
    observed = tuple(sorted(row["subtype"] for row in live.values()))
    date = rows(path, "DATE")[0]
    events = [
        line.rsplit("DISASTER-EVENT ", 1)[1]
        for line in run["log"]
        if "DISASTER-EVENT " in line
    ]
    if kind == "init" and observed != scenario["expected_subtypes"]:
        raise RuntimeError(f"natural scheduler omitted subtype family: {observed}")
    if kind in ("disabled", "empty-year", "countdown-wrap") and live:
        raise RuntimeError("ineligible scheduler created disaster")
    if kind == "countdown-wrap" and date["next_disaster_start"] != 65535:
        raise RuntimeError("disaster countdown did not underflow")
    if (
        kind in ("init", "disabled", "empty-year")
        and not 730 <= date["next_disaster_start"] <= 1241
    ):
        raise RuntimeError("scheduler delay reset omitted")
    if kind.startswith("zeppelin"):
        airport = rows(path, "STNN")[0]["airport[0]/flags"]
        if scenario["ticks"] == 5000:
            if (
                not any(
                    row["subtype"] == 0 and row["state"] == 3 for row in live.values()
                )
                or airport != 4611686018427388160
                or "zeppelin-crashed 0" not in events
            ):
                raise RuntimeError("zeppelin crash, block or delivered event omitted")
        elif live or airport != 0 or "zeppelin-cleared 0" not in events:
            raise RuntimeError("zeppelin clear, removal or delivered event omitted")
    if kind == "small-ufo-crash" and not any(
        event.startswith("vehicle-crashed ") and event.split()[2] == "2"
        for event in events
    ):
        raise RuntimeError("small UFO did not crash a real road vehicle")
    if kind == "train-target" and not any(
        row["subtype"] == 9 and row["state"] == 1 and row["dest_tile"] == 10007
        for row in live.values()
    ):
        raise RuntimeError("big UFO did not select actual human front train")
    if kind == "train-clear":
        types = read_save(path)["MAPT"]["raw"]
        if (
            observed != (11, 12)
            or any(types[tile] >> 4 != 0 for tile in (10005, 10006))
            or types[10007] >> 4 != 1
        ):
            raise RuntimeError(
                "destroyer, ground clearing or occupied-rail retention omitted"
            )
    if kind == "train-complete" and live:
        raise RuntimeError("destroyer did not expire")
    result[f"{mode}_{role}_disasters"] = {
        "elapsed_ticks": elapsed,
        "vehicles": live,
        "delay": date["next_disaster_start"],
        "events": events,
    }


def scenarios(soak):
    cases = [(f"init-{family}", "init", 1, family) for family in FAMILIES]
    cases += [
        (kind, kind, 1, "coal") for kind in ("disabled", "empty-year", "countdown-wrap")
    ]
    cases += [
        (f"{kind}-{ticks}", kind, ticks, "zeppelin")
        for kind in ("zeppelin-small", "zeppelin-large")
        for ticks in (5000, 19000)
    ]
    cases += [
        ("small-ufo-crash", "small-ufo-crash", 19000, "small-ufo"),
        ("train-target", "train-target", 12000, "big-ufo"),
        ("train-clear", "train-clear", 14000, "big-ufo"),
        ("train-complete", "train-complete", 20000, "big-ufo"),
    ]
    if soak:
        cases += [
            (f"soak-{family}", "init-soak", 9000, family)
            for family in ("small-sub", "big-sub", "airplane", "helicopter")
        ]
    return [
        {
            "name": f"disasters-{label}",
            "kind": "save",
            "disasters": kind,
            "family": family,
            "ticks": ticks,
            "short_checkpoint": True,
        }
        for label, kind, ticks, family in cases
    ]
