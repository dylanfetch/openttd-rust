"""towns scenario evidence."""

import hashlib
import json
import re
import shutil
import struct
import subprocess
import threading
from pathlib import Path

from . import core
from .core import (
    ROOT,
    SNAPSHOT_TICKS,
    TICKS_PER_DAY,
    Reader,
    decode_element,
    field_spans,
    read_save,
    run_game,
)

TOWN_STYLES = (
    "english french german american latin silly swedish dutch finnish polish "
    "slovak norwegian hungarian austrian romanian czech swiss danish turkish "
    "italian catalan"
).split()
# Explicit name parts, not world-generation seeds. Add source-derived branch
# witnesses here; single-bit/complement pairs exercise every input bit.
TOWN_SEEDS = (
    (0, 0xFFFFFFFF)
    + tuple(seed for bit in range(32) for seed in (1 << bit, 0xFFFFFFFF ^ (1 << bit)))
    + (
        # Finnish 5/10 thresholds, vowel harmony and trailing i -> e.
        21845,
        21846,
        43690,
        43691,
        3797316664,
        855005920,
        614243145,
        # Original/additional English initial-prefix substitutions.
        3674656959,
        737232967,
        2886390889,
        352485267,
        4261417127,
        587458959,
        3672210199,
        3736173387,
        1913994190,
        2719700081,
        470823877,
        3741662038,
        969238826,
        1323165768,
        1779546584,
        3668169832,
        2130189588,
        # Czech tail thresholds, fixed/free stems and required/absent postfix.
        45,
        49,
        65,
        69,
        3121,
        8257,
        10825,
        11821,
        13825,
        14901,
        26625,
        27209,
        94209,
        29249,
        9729,  # Czech dynamic feminine ending.
        15361,  # Czech avava postfix suppression.
    )
)


def prepare_town_names(scenario, binaries, builds, out, timeout, env):
    """Patch only CITY name type/parts in a reference-produced input fixture."""
    setup = {k: v for k, v in scenario.items() if k not in ("town_style", "console")}
    with core.MACHINE.hold(alone=False):
        run = run_game(
            setup,
            binaries["reference"],
            builds["reference"],
            out / scenario["name"] / "prepare",
            timeout,
            env,
        )
    if run["exit"] != 0 or not run["snapshots"]:
        raise RuntimeError("town-name preparation failed")
    fixture = run["snapshots"][-1]
    original = read_save(fixture)
    city = original["CITY"]
    # Modern CH_TABLE has these fixed-width leading fields; reject another
    # schema rather than silently modifying a different field's bytes.
    if (
        city["kind"] != 3
        or city["header"][:4]
        != [
            {"type": 6, "key": "xy"},
            {"type": 6, "key": "townnamegrfid"},
            {"type": 4, "key": "townnametype"},
            {"type": 6, "key": "townnameparts"},
        ]
        or len(city["elements"]) < len(TOWN_SEEDS)
    ):
        raise RuntimeError("unexpected CITY schema or too few towns for seed corpus")
    source = fixture.read_bytes()
    data = bytearray(source)
    reader = Reader(source, city["span"][0] + 5)
    reader.take(reader.gamma() - 1)  # table header
    assignments = []
    for index, body in city["elements"]:
        length = reader.gamma() - 1
        if reader.take(length) != body:
            raise RuntimeError("unexpected empty CITY slot")
        fields = decode_element(city, body)
        if fields is None or fields["townnamegrfid"] != 0 or fields["name"]:
            raise RuntimeError("town fixture is not a built-in generated name")
        seed = TOWN_SEEDS[index % len(TOWN_SEEDS)]
        struct.pack_into(
            ">HI", data, reader.pos - length + 8, 0x20C0 + scenario["town_style"], seed
        )
        assignments.append([index, seed])
    if {seed for _, seed in assignments} != set(TOWN_SEEDS):
        raise RuntimeError("town-name fixture omitted explicit seed corpus members")
    # Same prepared-input GLOG reset as road reloads; output is never normalized.
    begin, end = original["GLOG"]["span"]
    normalized = fixture.parent / "town-name-input.sav"
    normalized_data = bytes(data[:begin] + data[end:])
    normalized.write_bytes(normalized_data)
    check = read_save(normalized)
    if set(check) != set(original) - {"GLOG"} or any(
        source[slice(*original[cid]["span"])]
        != normalized_data[slice(*check[cid]["span"])]
        for cid in check
        if cid != "CITY"
    ):
        raise RuntimeError("town-name input changed outside CITY/GLOG")
    for (index, a), (other_index, b) in zip(
        city["elements"], check["CITY"]["elements"], strict=True
    ):
        left, right = decode_element(city, a), decode_element(check["CITY"], b)
        left.update(
            townnametype=0x20C0 + scenario["town_style"],
            townnameparts=TOWN_SEEDS[index % len(TOWN_SEEDS)],
        )
        if index != other_index or left != right or a[:8] != b[:8] or a[14:] != b[14:]:
            raise RuntimeError("town-name input changed unrelated CITY fields")
    return dict(scenario, kind="save", save=str(normalized)), {
        "reference": str(fixture),
        "normalized": str(normalized),
        "reference_sha256": hashlib.sha256(source).hexdigest(),
        "normalized_sha256": hashlib.sha256(normalized_data).hexdigest(),
        "removed_chunk": "GLOG",
        "town_style": scenario["town_style"],
        "assignments": assignments,
    }


def check_town_observer(scenario, run):
    """Require every exact rendered row; equal empty or truncated logs cannot pass."""
    city = read_save(run["snapshots"][-1])["CITY"]
    expected = []
    for index, body in city["elements"]:
        fields = decode_element(city, body)
        if (
            fields is None
            or fields["townnamegrfid"] != 0
            or fields["name"]
            or fields["townnametype"] != 0x20C0 + scenario["town_style"]
        ):
            raise RuntimeError("observed towns do not have the required built-in style")
        if (
            scenario["town_seeds"]
            and fields["townnameparts"] != TOWN_SEEDS[index % len(TOWN_SEEDS)]
        ):
            raise RuntimeError("explicit town seed changed on load")
        expected.append(index)
    rows = [re.search(r"TOWN-NAME (\d+) (.+)$", line) for line in run["log"]]
    ids = [int(row[1]) for row in rows if row]
    markers = [
        line.rsplit("TOWN-NAME-END ", 1)[1]
        for line in run["log"]
        if "TOWN-NAME-END " in line
    ]
    if not expected or ids != expected or markers != [str(len(expected))]:
        raise RuntimeError(
            "town-name observer missing ordered rows or completion/count marker"
        )


# Preserve church/stadium flags, but explicitly control growth-related bits.
GROWTH_FIELDS = (
    "flags",
    "grow_counter",
    "growth_rate",
    "fund_buildings_months",
    "road_build_months",
    "layout",
    "goal",
    "received[0]/old_act",
    "received[0]/new_act",
)
GROWTH_CASES = ("normal", "custom", "none", "funded", "blocked", "supplied")

# Growth saves build bridges and terraform, but never build a tunnel. Keep this
# bounded unchanged-body comparison in the component module, not another harness.
_TUNNEL_LOCK = threading.Lock()
_TUNNEL_REPORTS = {}


def tunnel_reference_gap(out, archive=None):
    """Compare unchanged pinned tunnel control with Rust over controlled services."""
    import migration

    out.mkdir(parents=True, exist_ok=True)
    report_path = out / "report.json"
    report_path.unlink(missing_ok=True)
    migration.ensure_reference()
    source = (migration.REFERENCE / "src/town_cmd.cpp").read_text()
    bodies = {}
    for name in ("CanRoadContinueIntoNextTile", "GrowTownWithTunnel"):
        start = source.index(f"static bool {name}(")
        end = source.index("{", start) + 1
        depth = 1
        # These pinned bodies have balanced braces, including their comments.
        while depth:
            depth += (source[end] == "{") - (source[end] == "}")
            end += 1
        bodies[name] = source[start:end]
    (out / "town-tunnel-original.hpp").write_text("\n\n".join(bodies.values()) + "\n")
    build = ROOT / "build-rust"
    configuration = migration.rust_configuration(build)
    archive = archive or migration.rust_archive(build)
    compiler = next(
        line.split("=", 1)[1]
        for line in (build / "CMakeCache.txt").read_text().splitlines()
        if line.startswith("CMAKE_CXX_COMPILER:FILEPATH=")
    )
    fixture = Path(__file__).with_name("town-tunnel-reference.cpp")
    binary = out / "town-tunnel-reference"
    command = [
        compiler,
        "-std=c++20",
        "-O2",
        "-I",
        str(out),
        str(fixture),
        str(archive),
        *configuration["native_libraries"],
        "-o",
        str(binary),
    ]
    env = migration.environment()
    with (out / "compile.log").open("wb") as log:
        subprocess.run(
            command, env=env, stdout=log, stderr=subprocess.STDOUT, check=True
        )
    run = subprocess.run([str(binary)], env=env, capture_output=True)
    (out / "transcript.txt").write_bytes(run.stdout)
    (out / "stderr.txt").write_bytes(run.stderr)
    if run.returncode != 0:
        raise RuntimeError(f"town tunnel reference gap failed: {run.stderr.decode()}")
    match = re.search(rb"\nPASS (\d+) unchanged-reference tunnel cases\n$", run.stdout)
    if not match or int(match[1]) == 0:
        raise RuntimeError("town tunnel reference gap omitted completion/count marker")
    migration.ensure_reference()
    report = {
        "passed": True,
        "cases": int(match[1]),
        "successful_tunnel_cases": len(re.findall(rb" result=1 ", run.stdout)),
        "execute_effect_cases": len(re.findall(rb" effects=1 ", run.stdout)),
        "reference": migration.BASELINE,
        "reference_body_sha256": {
            name: hashlib.sha256(body.encode()).hexdigest()
            for name, body in bodies.items()
        },
        "fixture_sha256": hashlib.sha256(fixture.read_bytes()).hexdigest(),
        "archive": str(archive),
        "archive_sha256": hashlib.sha256(archive.read_bytes()).hexdigest(),
        "transcript_sha256": hashlib.sha256(run.stdout).hexdigest(),
        "commands": [command, [str(binary)]],
        "limits": [
            "Controlled world and command outcomes; the paired save corpus has no actual tunnel build.",
            "Ordered road-mask/info services, command arguments, result, RNG and partial execute effects compare exactly.",
            "Pure C++ map getter calls and Rust bundled records have different granularity; their call counts/order are not compared.",
            "One active town-buildable road type; shared command internals and road-type selection are outside this gap check.",
        ],
        "agent": "/root/town_tunnel_reference_gap",
        "model": "gpt-6.1-sol",
        "reasoning_effort": "high",
    }
    report_path.write_text(json.dumps(report, indent=2) + "\n")
    return report


def town_growth_rows(chunks):
    city = chunks["CITY"]
    return {index: decode_element(city, body) for index, body in city["elements"]}


def prepare_growth(scenario, binaries, builds, out, timeout, env):
    """Patch only growth inputs in an unchanged-reference-produced modern save."""
    setup = {k: v for k, v in scenario.items() if k != "town_growth"}
    setup["ticks"] = 2 * SNAPSHOT_TICKS + TICKS_PER_DAY
    with core.MACHINE.hold(alone=False):
        run = run_game(
            setup,
            binaries["reference"],
            builds["reference"],
            out / scenario["name"] / "prepare",
            timeout,
            env,
        )
    if run["exit"] != 0 or not run["snapshots"]:
        raise RuntimeError("town-growth preparation failed")
    fixture = run["snapshots"][-1]
    chunks = read_save(fixture)
    city = chunks["CITY"]
    if city["kind"] != 3 or len(city["elements"]) < len(GROWTH_CASES):
        raise RuntimeError("town-growth fixture needs six modern CITY records")
    source = fixture.read_bytes()
    data, patches, assignments = bytearray(source), [], {}
    reader = Reader(source, city["span"][0] + 5)
    reader.take(reader.gamma() - 1)
    for ordinal, (index, body) in enumerate(city["elements"]):
        length = reader.gamma() - 1
        if reader.take(length) != body:
            raise RuntimeError("unexpected CITY element framing")
        fields = decode_element(city, body)
        spans = field_spans(body, city["header"])
        case = GROWTH_CASES[ordinal % len(GROWTH_CASES)]
        assignments[index] = case
        values = {
            "flags": fields["flags"] & 6 | (0 if case == "normal" else 9),
            "grow_counter": 0,
            "growth_rate": 511,
            "fund_buildings_months": 3 if case == "funded" else 0,
            "road_build_months": 6 if case == "funded" else 0,
            "goal": (1 if case in ("funded", "blocked", "supplied") else 0,) * 6,
        }
        if case == "none":
            values.update(
                flags=fields["flags"] & 6 | 8, grow_counter=65535, growth_rate=65535
            )
        if case == "funded":
            values["flags"] = fields["flags"] & 6 | 1
        for cargo in range(6):
            for period in ("old", "new"):
                values[f"received[{cargo}]/{period}_act"] = int(case == "supplied")
                values[f"received[{cargo}]/{period}_max"] = int(case == "supplied")
        for key, value in values.items():
            begin, end, kind = spans[key]
            width = core.FILE_TYPES[kind & 15][1]
            numbers = value if isinstance(value, tuple) else (value,)
            encoded = b"".join(number.to_bytes(width, "big") for number in numbers)
            # Arrays carry a length prefix; preserve it and their fixed size.
            start = end - len(encoded)
            if (
                start < begin
                or body[start:end]
                != source[reader.pos - length + start : reader.pos - length + end]
            ):
                raise RuntimeError("town-growth patch does not match field width")
            absolute = reader.pos - length + start
            patches.append(
                [
                    absolute - city["span"][0],
                    data[absolute : absolute + len(encoded)].hex(),
                    encoded.hex(),
                    f"CITY[{index}]/{key}",
                ]
            )
            data[absolute : absolute + len(encoded)] = encoded
    begin, end = chunks["GLOG"]["span"]
    normalized = fixture.parent / "town-growth-input.sav"
    normalized_data = bytes(data[:begin] + data[end:])
    normalized.write_bytes(normalized_data)
    check = read_save(normalized)
    if set(check) != set(chunks) - {"GLOG"}:
        raise RuntimeError("town-growth input changed chunk inventory")
    for cid in check:
        expected = bytearray(source[slice(*chunks[cid]["span"])])
        if cid == "CITY":
            for offset, old, new, _ in patches:
                value = bytes.fromhex(new)
                if expected[offset : offset + len(value)] != bytes.fromhex(old):
                    raise RuntimeError("town-growth input patch provenance mismatch")
                expected[offset : offset + len(value)] = value
        if bytes(expected) != normalized_data[slice(*check[cid]["span"])]:
            raise RuntimeError(f"town-growth input changed unintended {cid} bytes")
    return dict(
        scenario, kind="save", save=str(normalized), growth_assignments=assignments
    ), {
        "reference": str(fixture),
        "normalized": str(normalized),
        "reference_sha256": hashlib.sha256(source).hexdigest(),
        "normalized_sha256": hashlib.sha256(normalized_data).hexdigest(),
        "removed_chunk": "GLOG",
        "patches": patches,
        "assignments": assignments,
    }


def check_growth(scenario, run):
    """Require physical growth and saved control transitions, not equal emptiness."""
    paths = [Path(scenario["save"]), *run["snapshots"]]
    chunks = [read_save(path) for path in paths]
    towns = [town_growth_rows(chunk) for chunk in chunks]
    initial, final = chunks[0], chunks[-1]
    tile_types = [chunk["MAPT"]["raw"] for chunk in chunks]
    houses = sum(
        a >> 4 != 3 and b >> 4 == 3
        for a, b in zip(tile_types[0], tile_types[-1], strict=True)
    )
    roads = sum(
        a >> 4 != 2 and b >> 4 == 2
        for a, b in zip(tile_types[0], tile_types[-1], strict=True)
    )
    changed_roads = sum(
        a >> 4 == b >> 4 == 2 and x != y
        for a, b, x, y in zip(
            tile_types[0],
            tile_types[-1],
            initial["MAP5"]["raw"],
            final["MAP5"]["raw"],
            strict=True,
        )
    )
    heads = [
        i
        for i, (a, b) in enumerate(zip(tile_types[0], tile_types[-1], strict=True))
        if a >> 4 != 9 and b >> 4 == 9
    ]
    bridges = sum(bool(final["MAP5"]["raw"][i] & 128) for i in heads)
    tunnels = len(heads) - bridges
    heights = sum(
        a != b
        for a, b in zip(initial["MAPH"]["raw"], final["MAPH"]["raw"], strict=True)
    )
    if scenario["town_growth"] == "buildings" and roads:
        raise RuntimeError("town built roads while road expansion was disabled")
    if not houses or (scenario["town_growth"] == "roads" and not roads + changed_roads):
        raise RuntimeError("town-growth scenario did not build houses/roads")
    moments = [core.save_moment(path)[2] for path in paths]
    failed_intervals = 0
    for index, case in scenario["growth_assignments"].items():
        if case != "custom" or scenario["town_growth"] == "disabled":
            continue
        # With a fixed rate of 511, successful calls alone repeat every 512
        # ticks. A residue change proves the shorter failed-growth retry ran.
        for before, after, begin, end in zip(
            towns[:-1], towns[1:], moments[:-1], moments[1:], strict=True
        ):
            failed_intervals += (
                before[index]["grow_counter"] - (end - begin)
            ) % 512 != after[index]["grow_counter"]
    if scenario["town_growth"] == "buildings" and not failed_intervals:
        raise RuntimeError("buildings-only growth did not exercise failed retries")
    if (
        scenario["seed"] == 1
        and scenario["town_growth"] == "roads"
        and scenario["name"].startswith("town-growth-0-")
    ):
        if not bridges or not heights:
            raise RuntimeError("temperate growth did not build a bridge/terraform")
    witnesses = {}
    for case in GROWTH_CASES:
        ids = [
            index
            for index, label in scenario["growth_assignments"].items()
            if label == case
        ]
        rows = [[saved[index] for saved in towns] for index in ids]
        if case == "none" and any(
            row["growth_rate"] != 65535
            or row["flags"] & 1
            or row["grow_counter"] != 65535
            for history in rows
            for row in history
        ):
            raise RuntimeError("disabled custom growth changed its sentinel/counter")
        if case in ("custom", "supplied", "blocked") and any(
            row["growth_rate"] != 511 or not row["flags"] & 8
            for history in rows
            for row in history
        ):
            raise RuntimeError("custom growth rate/flag did not survive reload")
        if case == "funded" and any(
            history[-1]["fund_buildings_months"] or history[-1]["road_build_months"]
            for history in rows
        ):
            raise RuntimeError("monthly funding/road actions did not expire")
        if case in ("blocked", "supplied", "funded") and any(
            history[-1]["flags"] & 1 for history in rows
        ):
            raise RuntimeError("unsatisfied cargo goals did not stop custom growth")
        if case == "supplied" and any(
            history[-1]["received[0]/old_act"] or history[-1]["received[0]/new_act"]
            for history in rows
        ):
            raise RuntimeError(
                "monthly cargo receipt rotation did not exhaust supplied goals"
            )
        if scenario["town_growth"] == "disabled" and any(
            history[-1]["flags"] & 1 for history in rows
        ):
            raise RuntimeError(
                "disabled global growth did not stop after funding expired"
            )
        if case == "normal" and not any(
            row["growth_rate"] != history[0]["growth_rate"]
            for history in rows
            for row in history[1:]
        ):
            raise RuntimeError("normal growth rate was not recomputed")
        if (
            case in ("custom", "funded")
            and (case == "funded" or scenario["town_growth"] != "disabled")
            and not any(
                row["grow_counter"] != history[0]["grow_counter"]
                for history in rows
                for row in history[1:]
            )
        ):
            raise RuntimeError(f"no grow-counter transition for {case}")
        witnesses[case] = {
            str(index): [{key: row[key] for key in GROWTH_FIELDS} for row in history]
            for index, history in zip(ids, rows, strict=True)
        }
    return {
        "new_house_tiles": houses,
        "new_road_tiles": roads,
        "changed_road_bits": changed_roads,
        "new_bridge_heads": bridges,
        "new_tunnel_heads": tunnels,
        "failed_growth_intervals": failed_intervals,
        "changed_heights": heights,
        "towns": witnesses,
    }


def growth_scenarios(soak):
    # Natural generated roads witness bridges/terraform but this corpus builds
    # no tunnels. Roads-only expansion needs a deity/editor command; it has no
    # existing console entry point. Do not mistake zero counts for coverage.
    cases = [
        (layout, climate, "roads")
        for layout, climate in enumerate(("temperate", "arctic", "tropic", "toyland"))
    ]
    cases += [(0, "temperate", "buildings"), (0, "temperate", "disabled")]
    if soak:
        cases += [
            (layout, climate, "roads")
            for layout, climate in enumerate(
                ("toyland", "tropic", "arctic", "temperate")
            )
        ]
    scenarios = []
    for index, (layout, climate, mode) in enumerate(cases):
        scenario = {
            "name": f"town-growth-{layout}-{climate}-{mode}-reload",
            "kind": "generate",
            "seed": 12345 if soak else 1,
            "map_log2": 7,
            "land_generator": 1,
            "town_growth": mode,
            "ticks": (365 if soak else 210) * TICKS_PER_DAY + SNAPSHOT_TICKS,
            "console": [
                f"setting economy.allow_town_roads {int(mode != 'buildings')}",
                f"setting economy.town_growth_rate {0 if mode == 'disabled' else 4}",
                "unpause",
            ],
            "settings": {
                "difficulty": {"number_towns": 4},
                "game_creation": {"custom_town_number": 12, "landscape": climate},
                "economy": {"town_layout": layout},
            },
        }
        if index == 5:
            scenario.update(
                kind="save", save=str(ROOT / "migration/saves/opus-55-167-002.sav")
            )
        scenarios.append(scenario)
    return scenarios


def scenarios(soak):
    scenarios = []
    for style, label in enumerate(TOWN_STYLES):
        for prepared in (False, True):
            scenarios.append(
                {
                    "name": f"town-names-{label}-{'seeds' if prepared else 'generate'}",
                    "kind": "generate",
                    "seed": 12345 if soak else 1,
                    "map_log2": 8 if prepared else 6,
                    "land_generator": 1,
                    "town_style": style,
                    "town_seeds": prepared,
                    "ticks": 2 * SNAPSHOT_TICKS + TICKS_PER_DAY,
                    "console": ["start_ai MigrationTownNames", "unpause"],
                    "settings": {
                        "difficulty": {"number_towns": 4},
                        "game_creation": {
                            "custom_town_number": 128 if prepared else 8,
                            "town_name": "english" if prepared else label,
                        },
                    },
                }
            )
    return scenarios + growth_scenarios(soak)


def prepare(scenario, binaries, builds, out, timeout, env, result):
    if "town_growth" in scenario:
        if binaries["candidate"] != binaries["reference"]:  # --self stays C++ only.
            with _TUNNEL_LOCK:
                if out not in _TUNNEL_REPORTS:
                    _TUNNEL_REPORTS[out] = tunnel_reference_gap(
                        out / "town-tunnel-reference"
                    )
                result["tunnel_reference_gap"] = _TUNNEL_REPORTS[out]
        scenario, result["reload_input"] = prepare_growth(
            scenario, binaries, builds, out, timeout, env
        )
    if scenario.get("town_seeds"):
        scenario, result["town_name_input"] = prepare_town_names(
            scenario, binaries, builds, out, timeout, env
        )
    return scenario


def check(scenario, run, mode, role, result):
    if "town_growth" in scenario:
        result[f"{mode}_{role}_town_growth"] = check_growth(scenario, run)
    if "town_style" in scenario:
        check_town_observer(scenario, run)


def install(scenario, run_dir):
    if "town_style" in scenario:
        shutil.copytree(scenario["scenario_ai"], run_dir / "ai/town-names")


def game_args(scenario):
    return ["-d", "script=2"] if "town_style" in scenario else []


PREPARE_ORDER = 1


AI_FOLDER = "town-name-observer"


def uses_ai(scenario):
    return "town_style" in scenario


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description=tunnel_reference_gap.__doc__)
    parser.add_argument(
        "--out", type=Path, default=ROOT / ".local/town-tunnel-reference"
    )
    parser.add_argument("--archive", type=Path)
    args = parser.parse_args()
    report = tunnel_reference_gap(args.out.resolve(), args.archive)
    print(
        f"Tunnel reference gap passed: {report['cases']} cases; {args.out / 'report.json'}"
    )
