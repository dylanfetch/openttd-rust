"""towns scenario evidence."""

import hashlib
import re
import shutil
import struct

from . import core
from .core import (
    SNAPSHOT_TICKS,
    TICKS_PER_DAY,
    Reader,
    decode_element,
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
    return scenarios


def prepare(scenario, binaries, builds, out, timeout, env, result):
    if scenario.get("town_seeds"):
        scenario, result["town_name_input"] = prepare_town_names(
            scenario, binaries, builds, out, timeout, env
        )
    return scenario


def check(scenario, run, mode, role, result):
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
