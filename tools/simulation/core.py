#!/usr/bin/env python3
"""Compare the semantic simulation state of the reference and candidate games.

Both binaries run the same headless scenarios. `-d desync=3` makes each write an
uncompressed snapshot every 32 economy days; this tool decodes every chunk of
every snapshot (table chunks field by field using the header stored in the
save, other chunks byte by byte) and reports differences as
`snapshot chunk/element/field: reference -> candidate`.

Run it through `python3 tools/migration.py simulate`, which builds both games
first. Use `--self` to compare the reference with itself (determinism and mask
check) and `--soak` for the larger scenario set.
"""

import argparse
import concurrent.futures
import contextlib
import fnmatch
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# Fields that legitimately differ between two runs or two binaries. Mask nothing
# else; a divergence caused by a port is a bug, listed in KNOWN_FAILURES below.
# Keys are (chunk, field path without indices, shell-style).
MASKS = {
    ("DATE", "id"): "random savegame id generated for every new game",
    ("GLOG", "action/revision/revision.text"): "build revision string of each binary",
    (
        "GLOG",
        "action/revision/revision.modified",
    ): "whether each build tree was modified",
    (
        "GLOG",
        "action/revision/revision.newgrf",
    ): "OpenTTD version each build reports to NewGRFs (from its git revision)",
    # BaseConsist::round_trip_time has no initializer (base_consist.h:43), so a
    # vehicle built in a reused pool slot saves stale heap bytes until its first
    # measured round trip; the reference differs from itself here (#83).
    ("VEHS", "*/common/round_trip_time"): "uninitialized in the original",
}

# Known reference-vs-candidate divergences caused by existing ports, recorded by
# their exact first divergence. Each names its bug issue; fix the port to remove
# an entry, never widen it. Format: (scenario, chunk, field path) -> "#issue".
KNOWN_FAILURES = {}

# Build-directory data the game loads at run time (languages, base graphics,
# scripts and the regression saves).
RUNTIME_DIRECTORIES = ("lang", "baseset", "ai", "game", "regression")

TICKS_PER_DAY = 74
SNAPSHOT_TICKS = 32 * TICKS_PER_DAY


def scenario_modules():
    """Families in scenario-list order; import after shared core initialization."""
    from . import disasters, effects, generated, play_saves, rails, ships, towns, trees

    return generated, play_saves, towns, trees, effects, rails, ships, disasters


def scenario_list(soak):
    return [
        scenario for module in scenario_modules() for scenario in module.scenarios(soak)
    ]


class Corrupt(Exception):
    pass


class Reader:
    def __init__(self, data, pos=0, end=None):
        self.data, self.pos, self.end = data, pos, len(data) if end is None else end

    def take(self, count):
        if self.pos + count > self.end:
            raise Corrupt("read past end")
        value = self.data[self.pos : self.pos + count]
        self.pos += count
        return value

    def byte(self):
        return self.take(1)[0]

    def gamma(self):
        """Saveload SlReadSimpleGamma: 1 to 5 bytes, big endian, prefix coded."""
        first = self.byte()
        if first < 0x80:
            return first
        if first < 0xC0:
            return ((first & 0x3F) << 8) | self.byte()
        if first < 0xE0:
            return ((first & 0x1F) << 16) | int.from_bytes(self.take(2), "big")
        if first < 0xF0:
            return ((first & 0x0F) << 24) | int.from_bytes(self.take(3), "big")
        if first < 0xF8:
            return int.from_bytes(self.take(4), "big")
        raise Corrupt("invalid gamma")


# SLE_FILE_* types: (struct format, size); 10 is STRING, 11 is STRUCT.
FILE_TYPES = {
    1: (">b", 1),
    2: (">B", 1),
    3: (">h", 2),
    4: (">H", 2),
    5: (">i", 4),
    6: (">I", 4),
    7: (">q", 8),
    8: (">Q", 8),
    9: (">H", 2),
}
FILE_STRING, FILE_STRUCT, HAS_LENGTH = 10, 11, 0x10


def read_header(reader):
    """Table header: (type, key) pairs ending in type 0, then sub-headers of struct fields."""
    fields = []
    while True:
        kind = reader.byte()
        if kind == 0:
            break
        fields.append(
            {
                "type": kind,
                "key": reader.take(reader.gamma()).decode("utf-8", "surrogateescape"),
            }
        )
    for field in fields:
        if field["type"] & 0x0F == FILE_STRUCT:
            field["fields"] = read_header(reader)
    return fields


def read_value(reader, kind):
    base = kind & 0x0F
    if base == FILE_STRING:
        # surrogateescape keeps distinct invalid byte sequences distinct.
        return reader.take(reader.gamma()).decode("utf-8", "surrogateescape")
    fmt, size = FILE_TYPES[base]
    return struct.unpack(fmt, reader.take(size))[0]


def read_object(reader, fields, prefix, out):
    """Flatten one object into out[path] = value, following SlObject's table encoding."""
    for field in fields:
        path = prefix + field["key"]
        kind, base = field["type"], field["type"] & 0x0F
        if base == FILE_STRUCT:
            for index in range(reader.gamma()):
                read_object(reader, field["fields"], f"{path}[{index}]/", out)
        elif base == FILE_STRING or not kind & HAS_LENGTH:
            out[path] = read_value(reader, kind)
        else:
            out[path] = tuple(read_value(reader, kind) for _ in range(reader.gamma()))


def read_save(path):
    """Return {chunk id: chunk} for an uncompressed OTTN save."""
    data = path.read_bytes()
    if data[:4] != b"OTTN":
        raise Corrupt(f"{path.name} is not an uncompressed save")
    reader, chunks = Reader(data, 8), {}
    while True:
        chunk_start = reader.pos
        cid = reader.take(4)
        if cid == b"\0\0\0\0":
            return chunks
        mode = reader.byte()
        kind = mode & 0x0F
        chunk = {"kind": kind, "elements": [], "header": None}
        if kind == 0:  # CH_RIFF
            length = int.from_bytes(reader.take(3), "big") + ((mode >> 4) << 24)
            chunk["raw"] = reader.take(length)
        else:
            if kind in (3, 4):  # CH_TABLE, CH_SPARSE_TABLE
                length = reader.gamma() - 1
                chunk["header"] = read_header(Reader(reader.take(length)))
            index = 0
            while True:
                length = reader.gamma()
                if length == 0:
                    break
                element = Reader(reader.take(length - 1))
                if kind in (2, 4):  # sparse: index precedes the element body
                    index = element.gamma()
                # SlIterateArray skips empty non-sparse slots; a sparse element
                # always counts because its length includes the index.
                if kind in (2, 4) or element.pos < len(element.data):
                    chunk["elements"].append((index, element.data[element.pos :]))
                index += 1
        chunk["span"] = (chunk_start, reader.pos)
        chunks[cid.decode("latin-1")] = chunk


def decode_element(chunk, body):
    if chunk["header"] is None:
        return None
    reader, out = Reader(body), {}
    try:
        read_object(reader, chunk["header"], "", out)
    except (Corrupt, struct.error, KeyError):
        return None
    # Chunks whose handlers append data beyond the described fields fall back to bytes.
    return out if reader.pos == len(body) else None


def mask_for(cid, path):
    """Return the MASKS pattern covering this field, if any."""
    generic = re.sub(r"\[\d+\]", "", path)
    return next(
        (
            pattern
            for chunk, pattern in MASKS
            if cid == chunk and fnmatch.fnmatchcase(generic, pattern)
        ),
        None,
    )


def compare_chunk(cid, ref, cand, stats):
    """Yield (element, field, reference, candidate) differences in one chunk."""
    if ref["kind"] != cand["kind"]:
        yield ("", "chunk type", ref["kind"], cand["kind"])
        return
    if ref["kind"] == 0:
        a, b = ref["raw"], cand["raw"]
        stats["elements"] += 1
        if a != b:
            if len(a) != len(b):
                yield ("", "length", len(a), len(b))
            for offset in range(min(len(a), len(b))):
                if a[offset] != b[offset]:
                    yield (f"byte {offset}", "", a[offset], b[offset])
        return
    if ref["header"] != cand["header"]:
        yield ("", "table header", "differs", "differs")
        return
    left, right = dict(ref["elements"]), dict(cand["elements"])
    for index in sorted(set(left) | set(right)):
        stats["elements"] += 1
        a, b = left.get(index), right.get(index)
        if a == b:
            continue
        if a is None or b is None:
            yield (
                str(index),
                "element",
                "present" if a is not None else "absent",
                "present" if b is not None else "absent",
            )
            continue
        fields_a, fields_b = decode_element(ref, a), decode_element(cand, b)
        if fields_a is None or fields_b is None:
            # Undecodable element: masks cannot apply, so any difference counts.
            yield (str(index), "bytes", a.hex()[:64], b.hex()[:64])
            continue
        for path in sorted(set(fields_a) | set(fields_b)):
            va, vb = fields_a.get(path), fields_b.get(path)
            if va == vb:
                continue
            pattern = mask_for(cid, path)
            if pattern is None:
                yield (str(index), path, va, vb)
            else:
                stats["masked"][f"{cid}:{pattern}"] = (
                    stats["masked"].get(f"{cid}:{pattern}", 0) + 1
                )


def failure_key(scenario, cid, element, field):
    """KNOWN_FAILURES key: the field path without indices, or the element for byte-level differences."""
    return (
        scenario,
        cid,
        re.sub(r"\[\d+\]", "", field) if field not in ("", "bytes") else element,
    )


def compare_saves(ref_path, cand_path, scenario, limit, stats):
    """Return difference records; stops after `limit` differences not in KNOWN_FAILURES."""
    ref, cand = read_save(ref_path), read_save(cand_path)
    records, unknown = [], 0
    for cid in sorted(set(ref) | set(cand)):
        stats["chunks"] += 1
        if cid not in ref or cid not in cand:
            diffs = [("", "chunk", cid in ref, cid in cand)]
        else:
            diffs = compare_chunk(cid, ref[cid], cand[cid], stats)
        for element, field, ref_value, cand_value in diffs:
            record = {
                "snapshot": ref_path.name,
                "chunk": cid,
                "element": element,
                "field": field,
                "reference": repr(ref_value),
                "candidate": repr(cand_value),
            }
            issue = KNOWN_FAILURES.get(failure_key(scenario, cid, element, field))
            if issue:
                record["issue"] = issue
            else:
                unknown += 1
            records.append(record)
            if unknown >= limit:
                return records
    return records


def set_option(text, section, line):
    """Add a line to an existing cfg section (a repeated section would be ignored)."""
    text, count = re.subn(
        rf"^\[{section}\]\s*$", f"[{section}]\n{line}", text, count=1, flags=re.M
    )
    return text if count else f"{text.rstrip()}\n[{section}]\n{line}\n"


def write_config(scenario, build, run_dir):
    if scenario["kind"] == "regression":
        text = (build / "regression/regression.cfg").read_text()
    else:
        text = (
            "[misc]\nlanguage = english.lng\n"
            "[gui]\nautosave = off\n"
            "[difficulty]\nmax_no_competitors = 0\ndisasters = true\n"
            "[game_creation]\ntown_name = english\n"
        )
        if scenario["kind"] == "generate":
            text += (
                f"map_x = {scenario['map_log2']}\nmap_y = {scenario.get('map_log2_y', scenario['map_log2'])}\n"
                f"land_generator = {scenario['land_generator']}\n"
            )
    for section, settings in scenario.get("settings", {}).items():
        for name, value in settings.items():
            text = set_option(text, section, f"{name} = {value}")
    text = set_option(text, "misc", "savegame_format = none")
    # The null video driver writes save/autosave/exit.sav when it stops.
    text = set_option(text, "gui", "autosave_on_exit = true")
    (run_dir / "openttd.cfg").write_text(text)


def run_game(scenario, binary, build, run_dir, timeout, base_env=None, desync=True):
    """Run one binary on one scenario in an isolated personal directory.

    desync=3 writes the 32-day snapshots but also checks and rebuilds caches
    every tick and switches YAPF rail to its uncached path, so every scenario
    also has a plain run without it whose exit save is compared."""
    shutil.rmtree(run_dir, ignore_errors=True)
    run_dir.mkdir(parents=True)
    write_config(scenario, build, run_dir)
    for module in scenario_modules():
        if install := getattr(module, "install", None):
            install(scenario, run_dir)
    if "console" in scenario:
        (run_dir / "scripts").mkdir()
        (run_dir / "scripts/game_start.scr").write_text(
            "".join(f"{line}\n" for line in scenario["console"])
        )
    if scenario["kind"] == "save":
        # A saved AI or GameScript missing from the runtime is replaced by the
        # idle dummy AI or dropped; the logs show it in both runs.
        game = ["-g", scenario["save"], "-d", "script=2"]
    elif scenario["kind"] == "regression":
        game = ["-g", f"ai/{scenario['test']}/test.sav", "-d", "script=2", "-Q"]
    else:
        game = ["-g", "-G", str(scenario["seed"])]
    for module in scenario_modules():
        if game_args := getattr(module, "game_args", None):
            game += game_args(scenario)
    command = [
        str(binary),
        "-x",
        "-c",
        str(run_dir / "openttd.cfg"),
        *game,
        "-snull",
        "-mnull",
        f"-vnull:ticks={scenario['ticks']}",
        *(["-d", "desync=3"] if desync else []),
    ]
    env = dict(
        base_env or os.environ,
        HOME=str(run_dir),
        XDG_DATA_HOME=str(run_dir / "xdg-data"),
        XDG_CONFIG_HOME=str(run_dir / "xdg-config"),
        XDG_CACHE_HOME=str(run_dir / "xdg-cache"),
    )
    started = time.monotonic()
    with (
        open(run_dir / "stdout.log", "wb") as out,
        open(run_dir / "stderr.log", "wb") as err,
    ):
        try:
            code = subprocess.run(
                command, cwd=build, env=env, stdout=out, stderr=err, timeout=timeout
            ).returncode
        except subprocess.TimeoutExpired:
            code = "timeout"
    autosave = run_dir / "save/autosave"
    snapshots = sorted(
        path
        for path in autosave.glob("dmp_cmds_*.sav")
        if not path.name.endswith("_00000000_00000000.sav")
    )  # title screen
    if (autosave / "exit.sav").is_file():
        snapshots.append(autosave / "exit.sav")
    return {
        "exit": code,
        "seconds": round(time.monotonic() - started, 3),
        "snapshots": snapshots,
        "log": log_lines(run_dir),
        "stdout": (run_dir / "stdout.log").read_bytes(),
    }


def log_lines(run_dir):
    """Debug output (script logs, warnings) without its timestamps."""
    return [
        re.sub(r"^\[[^\]]*\] ", "", line)
        for line in (run_dir / "stderr.log")
        .read_text(errors="surrogateescape")
        .splitlines()
    ]


class MachineLock:
    """Lets a retried plain pair run with no other harness game on the machine.

    A link graph job that has not finished by its join tick pauses the game, and
    the null driver keeps counting iterations while paused. The job has only a
    few milliseconds of wall time, so it misses that window when cores are
    oversubscribed. Ordinary runs hold `path` shared across every harness
    process and thread of the clone; a retry holds it exclusively. flock does
    not favour a waiting exclusive holder, so every run first passes through a
    gate lock that a retry keeps: once a retry waits, no new run starts and the
    wait is bounded by the runs already in progress."""

    def __init__(self, path):
        self.path, self.gate = path, path.with_suffix(".gate")

    @contextlib.contextmanager
    def hold(self, alone):
        try:
            import fcntl
        except ImportError:  # No flock (Windows): runs are not isolated.
            yield
            return
        with open(self.gate, "w") as gate, open(self.path, "w") as handle:
            fcntl.flock(gate, fcntl.LOCK_EX)
            fcntl.flock(handle, fcntl.LOCK_EX if alone else fcntl.LOCK_SH)
            if not alone:
                fcntl.flock(gate, fcntl.LOCK_UN)
            yield


MACHINE = None  # MachineLock, set by main()


def save_moment(path):
    """(date, date_fract, tick_counter) at which a save was written."""
    chunk = read_save(path)["DATE"]
    fields = decode_element(chunk, chunk["elements"][0][1])
    return fields["date"], fields["date_fract"], fields["tick_counter"]


def first_difference(a, b):
    index = next(
        i
        for i, (x, y) in enumerate(zip(a + [None], b + [None], strict=False))
        if x != y
    )
    return (
        index,
        (a[index] if index < len(a) else None),
        (b[index] if index < len(b) else None),
    )


def field_spans(body, header):
    """Locate table values for input patches without changing semantic decoding."""
    reader, spans = Reader(body), {}

    def visit(fields, prefix):
        for field in fields:
            kind, path = field["type"], prefix + field["key"]
            if kind & 15 == FILE_STRUCT:
                for index in range(reader.gamma()):
                    visit(field["fields"], f"{path}[{index}]/")
            else:
                begin = reader.pos
                if kind & 15 == FILE_STRING:
                    reader.take(reader.gamma())
                else:
                    count = reader.gamma() if kind & HAS_LENGTH else 1
                    reader.take(FILE_TYPES[kind & 15][1] * count)
                spans[path] = (begin, reader.pos, kind)

    visit(header, "")
    if reader.pos != len(body):
        raise RuntimeError("prepared VEHS element has undescribed bytes")
    return spans


def run_scenario(scenario, binaries, builds, out, limit, timeout, env):
    """Run both binaries twice (with and without desync snapshots) and compare.

    -vnull:ticks counts loop iterations, and the game pauses for iterations
    while a threaded link graph job is late (StateGameLoop_LinkGraphPauseControl),
    so how far a run gets depends on machine load. State at a given date is
    deterministic: snapshots are compared by date, a longer run's extra trailing
    snapshots and log lines are ignored, and exit saves are compared only when
    both runs stopped at the same moment (the short plain run is retried)."""
    name = scenario["name"]
    result = {
        "scenario": name,
        "differences": [],
        "known_failures": [],
        "problems": [],
        "notes": [],
        "snapshots": 0,
        "stats": {"chunks": 0, "elements": 0, "masked": {}},
    }

    def compare(mode, snapshot):
        records = compare_saves(
            out / name / mode / "reference/save/autosave" / snapshot,
            out / name / mode / "candidate/save/autosave" / snapshot,
            name,
            limit,
            result["stats"],
        )
        for record in records:
            record["snapshot"] = f"{mode}/{snapshot}"
            result["known_failures" if "issue" in record else "differences"].append(
                record
            )

    try:
        # Preserve preparation order independently of scenario-list order.
        for module in sorted(
            scenario_modules(), key=lambda m: getattr(m, "PREPARE_ORDER", 4)
        ):
            if prepare := getattr(module, "prepare", None):
                scenario = prepare(
                    scenario, binaries, builds, out, timeout, env, result
                )
        for mode, desync in (("snapshots", True), ("plain", False)):
            for attempt in range(1 if desync else 3):
                with MACHINE.hold(alone=attempt > 0):
                    runs = {
                        role: run_game(
                            scenario,
                            binaries[role],
                            builds[role],
                            out / name / mode / role,
                            timeout,
                            env,
                            desync,
                        )
                        for role in ("reference", "candidate")
                    }
                exits = [
                    runs[role]["snapshots"][-1]
                    if runs[role]["snapshots"]
                    and runs[role]["snapshots"][-1].name == "exit.sav"
                    else None
                    for role in ("reference", "candidate")
                ]
                # Retry only a clean pair whose end moments differ; a crash, hang
                # or missing save on any attempt is kept and reported.
                clean = all(exits) and all(run["exit"] == 0 for run in runs.values())
                same_end = clean and save_moment(exits[0]) == save_moment(exits[1])
                if same_end or not clean:
                    break
            for role, run in runs.items():
                for module in scenario_modules():
                    if check := getattr(module, "check", None):
                        check(scenario, run, mode, role, result)
                result[f"{mode}_{role}_seconds"] = run["seconds"]
                if run["exit"] != 0:
                    result["problems"].append(
                        f"{mode}: {role} exited with {run['exit']}"
                    )
                # Cache-check mismatches are repaired before the next tick, so they
                # never reach a save; any desync warning fails the scenario.
                warning = next(
                    (line for line in run["log"] if "[desync:" in line), None
                )
                if warning:
                    result["problems"].append(f"{mode}: {role} logged {warning!r}")
            if not all(exits):
                result["problems"].append(f"{mode}: an exit save is missing")
            periodic = [
                [p.name for p in runs[role]["snapshots"] if p.name != "exit.sav"]
                for role in ("reference", "candidate")
            ]
            shorter = min(periodic, key=len)
            if periodic[0][: len(shorter)] != periodic[1][: len(shorter)] or (
                same_end and periodic[0] != periodic[1]
            ):
                result["problems"].append(
                    f"{mode}: snapshot dates differ: {first_difference(*periodic)}"
                )
            elif len(periodic[0]) != len(periodic[1]):
                result["notes"].append(
                    f"{mode}: runs reached different dates ({len(periodic[0])} vs {len(periodic[1])} snapshots)"
                )
            if mode == "snapshots":
                result["snapshots"] = len(shorter) + bool(same_end)
                if (
                    len(shorter) < 2
                    and "effects" not in scenario
                    and not scenario.get("short_checkpoint")
                ):
                    result["problems"].append(
                        "fewer than two periodic snapshots were written"
                    )
                # Console output is invisible without a GUI, so confirm each
                # setting line took effect in the first snapshot of both runs.
                # Values must be written as PATS stores them (numbers, not
                # true/false or names), and only saved game settings qualify.
                for role, run in runs.items():
                    first = next(
                        (p for p in run["snapshots"] if p.name != "exit.sav"), None
                    )
                    for line in scenario.get("console", []) if first else []:
                        if line.startswith("setting "):
                            _, key, value = line.split()
                            chunk = read_save(first)["PATS"]
                            saved = decode_element(chunk, chunk["elements"][0][1]).get(
                                key
                            )
                            if str(saved) != value:
                                result["problems"].append(
                                    f"{role}: {key} is {saved}, not {value}, in {first.name}"
                                )
            for snapshot in shorter:
                compare(mode, snapshot)
                if result["differences"]:
                    break  # Later snapshots only repeat the first divergence.
            if same_end and not result["differences"]:
                compare(mode, "exit.sav")
            elif not same_end and not desync and all(exits):
                result["problems"].append(
                    f"plain: runs ended at different moments after {attempt + 1} attempts: "
                    f"{save_moment(exits[0])} vs {save_moment(exits[1])}"
                )
            for label in ("log", "stdout"):
                a, b = (
                    runs[role][label]
                    if label == "log"
                    else runs[role][label].splitlines()
                    for role in ("reference", "candidate")
                )
                if not same_end:
                    a, b = a[: min(len(a), len(b))], b[: min(len(a), len(b))]
                if a != b:
                    index, ref_line, cand_line = first_difference(a, b)
                    result["differences"].append(
                        {
                            "snapshot": f"{mode}/{label}",
                            "chunk": "",
                            "element": str(index + 1),
                            "field": "line",
                            "reference": repr(ref_line),
                            "candidate": repr(cand_line),
                        }
                    )
    except (
        Exception
    ) as error:  # Record and continue, so the report covers every scenario.
        result["problems"].append(f"harness error: {type(error).__name__}: {error}")
    result["passed"] = not result["differences"] and not result["problems"]
    if result["passed"]:
        if (
            "reload_input" in result
            or "town_name_input" in result
            or "tree_input" in result
            or "effect_input" in result
            or "disaster_input" in result
        ):
            for mode in ("snapshots", "plain"):
                shutil.rmtree(out / name / mode, ignore_errors=True)
        else:
            shutil.rmtree(out / name, ignore_errors=True)  # Keep only failing runs.
    return result


def copy_runtime(build, binary, destination):
    """Freeze the executable and its data, including symlink targets, for one run."""

    def missing_links(directory, names):
        # Extracted basesets can contain dangling documentation links. They were
        # unavailable in the source runtime too; resolve relative links there.
        return [
            name
            for name in names
            if (path := Path(directory) / name).is_symlink() and not path.exists()
        ]

    destination.mkdir()
    executable = destination / binary.name
    shutil.copy2(binary, executable)
    for name in RUNTIME_DIRECTORIES:
        if (build / name).is_dir():
            shutil.copytree(build / name, destination / name, ignore=missing_links)
    return executable


def main():
    sys.path.insert(0, str(ROOT / "tools"))
    import importlib.util

    spec = importlib.util.spec_from_file_location(
        "migration", ROOT / "tools/migration.py"
    )
    migration = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(migration)

    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument(
        "names", nargs="*", help="run only scenarios whose name contains one of these"
    )
    parser.add_argument(
        "--soak", action="store_true", help="larger set: more seeds, sizes and years"
    )
    parser.add_argument(
        "--self", action="store_true", help="compare the reference with itself"
    )
    parser.add_argument(
        "--candidate",
        type=Path,
        help="candidate binary (default: build-rust/openttd-rust)",
    )
    parser.add_argument(
        "--jobs", type=int, default=max(1, min(4, (os.cpu_count() or 2) // 2))
    )
    parser.add_argument(
        "--limit", type=int, default=20, help="differences reported per snapshot"
    )
    parser.add_argument(
        "--timeout", type=int, default=1200, help="seconds per game run"
    )
    parser.add_argument("--list", action="store_true")
    parser.add_argument(
        "--prepare-water-save",
        choices=("ferry", "structures"),
        help="build a committed ship fixture using only the pinned reference",
    )
    args = parser.parse_args()

    every = scenario_list(args.soak)
    # An exact scenario name selects only that scenario; anything else is a substring filter.
    scenarios = [
        s
        for s in every
        if not args.names
        or any(
            n == s["name"] if any(n == t["name"] for t in every) else n in s["name"]
            for n in args.names
        )
    ]
    if not scenarios:
        parser.error(f"no scenario matches {args.names}; see --list")
    if args.list:
        print("\n".join(s["name"] for s in scenarios))
        return 0
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    out = (
        migration.LOCAL / "simulation" / f"{stamp}-{os.getpid()}"
    )  # concurrent runs never share
    global MACHINE
    migration.COMMON_LOCAL.mkdir(parents=True, exist_ok=True)
    MACHINE = MachineLock(migration.COMMON_LOCAL / "simulation.lock")
    out.mkdir(parents=True)
    if args.prepare_water_save:
        from .ships import prepare_water_save

        prepare_water_save(args.prepare_water_save, migration, out, args.timeout)
        return 0
    # This is the checkout at invocation, not proof that an arbitrary --candidate
    # was built from it. The frozen executable's hash identifies what we execute.
    candidate_commit = migration.git("rev-parse", "HEAD")
    candidate_status = migration.git("status", "--short")
    builds = {role: out / f"{role}-runtime" for role in ("reference", "candidate")}
    # The game finds its data next to the executable, so copy the shared
    # reference's runtime (about 16 MB) while no other worktree can rebuild it,
    # then run without holding the lock.
    with migration.reference_lock(shared=True):
        shared = migration.REFERENCE_BUILD.resolve()
        reference = copy_runtime(shared, shared / "openttd", builds["reference"])
    sources = {"reference": shared / "openttd"}
    binaries = {"reference": reference}
    if args.self:
        builds["candidate"], binaries["candidate"] = builds["reference"], reference
        sources["candidate"] = sources["reference"]
    else:
        candidate = (args.candidate or ROOT / "build-rust/openttd-rust").resolve()
        if not candidate.is_file():
            parser.error(
                f"candidate binary {candidate} is missing; run python3 tools/migration.py build"
            )
        sources["candidate"] = candidate
        binaries["candidate"] = copy_runtime(
            candidate.parent, candidate, builds["candidate"]
        )
    binary_evidence = {
        role: {
            "path": str(path),
            "source_path": str(sources[role]),
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        }
        for role, path in binaries.items()
    }

    # Freeze the two scenario AIs before workers launch; both roles install the
    # same immutable script bytes while command parameters remain per-run.
    scenario_ai = {}
    for module in scenario_modules():
        if not hasattr(module, "AI_FOLDER"):
            continue
        folder = module.AI_FOLDER
        selected = [s for s in scenarios if module.uses_ai(s)]
        if not selected:
            continue
        frozen = out / "scenario-ai" / folder
        shutil.copytree(ROOT / "tools" / folder, frozen)
        scenario_ai[folder] = {
            "path": str(frozen),
            "files": {
                str(path.relative_to(frozen)): hashlib.sha256(
                    path.read_bytes()
                ).hexdigest()
                for path in sorted(frozen.rglob("*"))
                if path.is_file()
            },
        }
        for scenario in selected:
            scenario["scenario_ai"] = str(frozen)

    started = time.monotonic()
    results = []
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        # The driver's environment supplies bootstrapped runtime libraries locally.
        env = migration.environment()
        futures = {
            pool.submit(
                run_scenario, s, binaries, builds, out, args.limit, args.timeout, env
            ): s
            for s in scenarios
        }
        for future in concurrent.futures.as_completed(futures):
            result = future.result()
            results.append(result)
            status = "ok  " if result["passed"] else "FAIL"
            known = (
                f", {len(result['known_failures'])} known"
                if result["known_failures"]
                else ""
            )
            print(
                f"{status} {result['scenario']}: {result['snapshots']} snapshots, "
                f"{result['stats']['chunks']} chunks, {result['stats']['elements']} elements{known}",
                flush=True,
            )
            for problem in result["problems"]:
                print(f"     {problem}", flush=True)
            for diff in result["differences"][: args.limit]:
                print(
                    f"     {diff['snapshot']} {diff['chunk']}/{diff['element']}/{diff['field']}: "
                    f"{diff['reference']} -> {diff['candidate']}",
                    flush=True,
                )
    results.sort(key=lambda r: r["scenario"])
    report = {
        "mode": "reference-vs-reference" if args.self else "reference-vs-candidate",
        "started_at": stamp,
        "candidate_commit": candidate_commit,
        "candidate_status": candidate_status,
        "binaries": binary_evidence,
        "scenario_ai": scenario_ai,
        "masks": {":".join(key): reason for key, reason in MASKS.items()},
        "known_failures": {
            ":".join(key): issue for key, issue in KNOWN_FAILURES.items()
        },
        "masked_differences": {
            key: sum(r["stats"]["masked"].get(key, 0) for r in results)
            for key in sorted({k for r in results for k in r["stats"]["masked"]})
        },
        "seconds": round(time.monotonic() - started, 1),
        "results": results,
        "passed": all(r["passed"] for r in results),
    }
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    for build in set(builds.values()):
        shutil.rmtree(build, ignore_errors=True)
    passed = sum(r["passed"] for r in results)
    print(
        f"Simulation: {passed}/{len(results)} scenarios equal, "
        f"{sum(r['snapshots'] for r in results)} snapshots in {report['seconds']} s. Report: {out / 'report.json'}"
    )
    return 0 if report["passed"] else 1
