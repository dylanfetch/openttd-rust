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
import fnmatch
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

ROOT = Path(__file__).resolve().parents[1]

# Fields that legitimately differ between two runs or two binaries. Mask nothing
# else; a divergence caused by a port is a bug, listed in KNOWN_FAILURES below.
# Keys are (chunk, field path without indices, shell-style).
MASKS = {
    ("DATE", "id"): "random savegame id generated for every new game",
    ("GLOG", "action/revision/revision.text"): "build revision string of each binary",
    ("GLOG", "action/revision/revision.modified"): "whether each build tree was modified",
    ("GLOG", "action/revision/revision.newgrf"): "OpenTTD version each build reports to NewGRFs (from its git revision)",
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


def scenario_list(soak):
    """Data-driven scenario set; extend this for new ports rather than adding tools."""
    scenarios = [
        {"name": "regression-regression", "kind": "regression", "test": "regression", "ticks": 30000},
        {"name": "regression-stationlist", "kind": "regression", "test": "stationlist", "ticks": 30000},
    ]
    seeds = (1, 12345, 777, 31337, 2024, 99) if soak else (1, 12345)
    sizes = (6, 7, 8, 9) if soak else (7, 8)
    years = 6 if soak else 2
    for generator, label in ((1, "tgp"), (0, "original")):
        for size in sizes:
            for seed in seeds:
                scenarios.append({
                    "name": f"generate-{label}-{1 << size}-{seed}", "kind": "generate", "seed": seed,
                    "map_log2": size, "land_generator": generator,
                    "ticks": years * 365 * TICKS_PER_DAY + SNAPSHOT_TICKS,
                })
    return scenarios


class Corrupt(Exception):
    pass


class Reader:
    def __init__(self, data, pos=0, end=None):
        self.data, self.pos, self.end = data, pos, len(data) if end is None else end

    def take(self, count):
        if self.pos + count > self.end:
            raise Corrupt("read past end")
        value = self.data[self.pos:self.pos + count]
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
FILE_TYPES = {1: (">b", 1), 2: (">B", 1), 3: (">h", 2), 4: (">H", 2), 5: (">i", 4),
              6: (">I", 4), 7: (">q", 8), 8: (">Q", 8), 9: (">H", 2)}
FILE_STRING, FILE_STRUCT, HAS_LENGTH = 10, 11, 0x10


def read_header(reader):
    """Table header: (type, key) pairs ending in type 0, then sub-headers of struct fields."""
    fields = []
    while True:
        kind = reader.byte()
        if kind == 0:
            break
        fields.append({"type": kind, "key": reader.take(reader.gamma()).decode("utf-8", "replace")})
    for field in fields:
        if field["type"] & 0x0F == FILE_STRUCT:
            field["fields"] = read_header(reader)
    return fields


def read_value(reader, kind):
    base = kind & 0x0F
    if base == FILE_STRING:
        return reader.take(reader.gamma()).decode("utf-8", "replace")
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
                if element.pos < len(element.data):  # SlIterateArray skips empty slots
                    chunk["elements"].append((index, element.data[element.pos:]))
                index += 1
        chunks[cid.decode("latin-1")] = chunk


def decode_element(chunk, body):
    if chunk["header"] is None:
        return None
    reader, out = Reader(body), {}
    try:
        read_object(reader, chunk["header"], "", out)
    except (Corrupt, struct.error):
        return None
    # Chunks whose handlers append data beyond the described fields fall back to bytes.
    return out if reader.pos == len(body) else None


def masked(cid, path):
    generic = re.sub(r"\[\d+\]", "", path)
    return any(cid == chunk and fnmatch.fnmatchcase(generic, pattern) for chunk, pattern in MASKS)


def compare_chunk(cid, ref, cand, limit):
    """Yield (element, field, reference, candidate) differences in one chunk."""
    if ref["kind"] != cand["kind"]:
        yield ("", "chunk type", ref["kind"], cand["kind"])
        return
    if ref["kind"] == 0:
        a, b = ref["raw"], cand["raw"]
        if a != b:
            if len(a) != len(b):
                yield ("", "length", len(a), len(b))
            shown = 0
            for offset in range(min(len(a), len(b))):
                if a[offset] != b[offset]:
                    yield (f"byte {offset}", "", a[offset], b[offset])
                    shown += 1
                    if shown >= limit:
                        return
        return
    if ref["header"] != cand["header"]:
        yield ("", "table header", "differs", "differs")
        return
    left, right = dict(ref["elements"]), dict(cand["elements"])
    for index in sorted(set(left) | set(right)):
        a, b = left.get(index), right.get(index)
        if a == b:
            continue
        if a is None or b is None:
            yield (str(index), "element", "present" if a is not None else "absent", "present" if b is not None else "absent")
            continue
        fields_a, fields_b = decode_element(ref, a), decode_element(cand, b)
        if fields_a is None or fields_b is None:
            # Undecodable element: masks cannot apply, so any difference counts.
            yield (str(index), "bytes", a.hex()[:64], b.hex()[:64])
            continue
        for path in sorted(set(fields_a) | set(fields_b)):
            va, vb = fields_a.get(path), fields_b.get(path)
            if va != vb and not masked(cid, path):
                yield (str(index), path, va, vb)


def compare_saves(ref_path, cand_path, limit):
    ref, cand = read_save(ref_path), read_save(cand_path)
    diffs = []
    for cid in sorted(set(ref) | set(cand)):
        if cid not in ref or cid not in cand:
            diffs.append((cid, "", "chunk", cid in ref, cid in cand))
            continue
        for diff in compare_chunk(cid, ref[cid], cand[cid], limit):
            diffs.append((cid, *diff))
            if len(diffs) >= limit:
                return diffs, len(ref)
    return diffs, len(ref)


def write_config(scenario, build, run_dir):
    if scenario["kind"] == "regression":
        text = (build / "regression/regression.cfg").read_text()
        # A second [misc] section would be ignored; extend the existing one.
        text, count = re.subn(r"^\[misc\]\s*$", "[misc]\nsavegame_format = none", text, count=1, flags=re.M)
        if count != 1:
            raise RuntimeError("regression.cfg has no [misc] section")
    else:
        text = (
            "[misc]\nsavegame_format = none\nlanguage = english.lng\n"
            "[gui]\nautosave = off\n"
            "[difficulty]\nmax_no_competitors = 0\n"
            "[game_creation]\ntown_name = english\n"
            f"map_x = {scenario['map_log2']}\nmap_y = {scenario['map_log2']}\n"
            f"land_generator = {scenario['land_generator']}\n"
        )
    (run_dir / "openttd.cfg").write_text(text)


def run_game(scenario, binary, build, run_dir, timeout, base_env=None):
    """Run one binary on one scenario in an isolated personal directory."""
    shutil.rmtree(run_dir, ignore_errors=True)
    run_dir.mkdir(parents=True)
    write_config(scenario, build, run_dir)
    if scenario["kind"] == "regression":
        game = ["-g", f"ai/{scenario['test']}/test.sav", "-d", "script=2", "-Q"]
    else:
        game = ["-g", "-G", str(scenario["seed"])]
    command = [str(binary), "-x", "-c", str(run_dir / "openttd.cfg"), *game,
               "-snull", "-mnull", f"-vnull:ticks={scenario['ticks']}", "-d", "desync=3"]
    env = dict(base_env or os.environ, HOME=str(run_dir), XDG_DATA_HOME=str(run_dir / "xdg-data"),
               XDG_CONFIG_HOME=str(run_dir / "xdg-config"), XDG_CACHE_HOME=str(run_dir / "xdg-cache"))
    started = time.monotonic()
    with open(run_dir / "stdout.log", "wb") as out, open(run_dir / "stderr.log", "wb") as err:
        result = subprocess.run(command, cwd=build, env=env, stdout=out, stderr=err, timeout=timeout)
    snapshots = sorted(path for path in (run_dir / "save/autosave").glob("dmp_cmds_*.sav")
                       if not path.name.endswith("_00000000_00000000.sav"))  # title screen
    return {"exit": result.returncode, "seconds": round(time.monotonic() - started, 1),
            "snapshots": snapshots, "command": command}


def script_log(run_dir):
    """Script (AI) output with timestamps and save/desync chatter removed."""
    lines = []
    for line in (run_dir / "stderr.log").read_text(errors="replace").splitlines():
        line = re.sub(r"^\[[^\]]*\] ", "", line)
        if re.match(r"^\[?(desync|save|load|net)", line):
            continue
        lines.append(line)
    return lines


def run_scenario(scenario, binaries, builds, out, limit, timeout, env):
    result = {"scenario": scenario["name"], "differences": [], "known_failures": [], "problems": []}
    runs = {}
    for role in ("reference", "candidate"):
        runs[role] = run_game(scenario, binaries[role], builds[role], out / scenario["name"] / role, timeout, env)
        result[f"{role}_seconds"] = runs[role]["seconds"]
        if runs[role]["exit"] != 0:
            result["problems"].append(f"{role} exited with {runs[role]['exit']}")
    ref_names = [path.name for path in runs["reference"]["snapshots"]]
    cand_names = [path.name for path in runs["candidate"]["snapshots"]]
    if ref_names != cand_names:
        result["problems"].append(f"snapshot sets differ: {len(ref_names)} reference, {len(cand_names)} candidate; "
                                  f"first mismatch {next((a, b) for a, b in zip(ref_names + [''], cand_names + ['']) if a != b)}")
    if not ref_names:
        result["problems"].append("no snapshots were written")
    result["snapshots"] = len(ref_names)
    chunks = 0
    for name in sorted(set(ref_names) & set(cand_names)):
        diffs, count = compare_saves(out / scenario["name"] / "reference/save/autosave" / name,
                                     out / scenario["name"] / "candidate/save/autosave" / name, limit)
        chunks += count
        for cid, element, field, ref_value, cand_value in diffs:
            key = (scenario["name"], cid, re.sub(r"\[\d+\]", "", field))
            record = {"snapshot": name, "chunk": cid, "element": element, "field": field,
                      "reference": repr(ref_value), "candidate": repr(cand_value)}
            if key in KNOWN_FAILURES:
                record["issue"] = KNOWN_FAILURES[key]
                result["known_failures"].append(record)
            else:
                result["differences"].append(record)
        if result["differences"]:
            break  # Later snapshots only repeat the first divergence.
    result["chunks_compared"] = chunks
    if scenario["kind"] == "regression":
        ref_log = script_log(out / scenario["name"] / "reference")
        cand_log = script_log(out / scenario["name"] / "candidate")
        if ref_log != cand_log:
            first = next(i for i, (a, b) in enumerate(zip(ref_log + [None], cand_log + [None])) if a != b)
            result["differences"].append({"snapshot": "script log", "chunk": "", "element": str(first + 1), "field": "line",
                                          "reference": repr(ref_log[first] if first < len(ref_log) else None),
                                          "candidate": repr(cand_log[first] if first < len(cand_log) else None)})
    result["passed"] = not result["differences"] and not result["problems"]
    if result["passed"]:
        shutil.rmtree(out / scenario["name"])  # Keep only failing runs.
    return result


def main():
    sys.path.insert(0, str(ROOT / "tools"))
    import importlib.util
    spec = importlib.util.spec_from_file_location("migration", ROOT / "tools/migration.py")
    migration = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(migration)

    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("names", nargs="*", help="run only scenarios whose name contains one of these")
    parser.add_argument("--soak", action="store_true", help="larger set: more seeds, sizes and years")
    parser.add_argument("--self", action="store_true", help="compare the reference with itself")
    parser.add_argument("--candidate", type=Path, help="candidate binary (default: build-rust/openttd-rust)")
    parser.add_argument("--jobs", type=int, default=max(1, min(4, (os.cpu_count() or 2) // 2)))
    parser.add_argument("--limit", type=int, default=20, help="differences reported per snapshot")
    parser.add_argument("--timeout", type=int, default=1200, help="seconds per game run")
    parser.add_argument("--list", action="store_true")
    args = parser.parse_args()

    scenarios = [s for s in scenario_list(args.soak) if not args.names or any(n in s["name"] for n in args.names)]
    if args.list:
        print("\n".join(s["name"] for s in scenarios))
        return 0
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    out = migration.LOCAL / "simulation" / stamp
    out.mkdir(parents=True)
    builds = {"reference": out / "reference-runtime", "candidate": ROOT / "build-rust"}
    # The game finds its data next to the executable, so copy the shared
    # reference's runtime (about 16 MB) while no other worktree can rebuild it,
    # then run without holding the lock.
    with migration.reference_lock(shared=True):
        shared = migration.REFERENCE_BUILD.resolve()
        builds["reference"].mkdir()
        shutil.copy2(shared / "openttd", builds["reference"] / "openttd")
        for name in RUNTIME_DIRECTORIES:
            if (shared / name).is_dir():
                shutil.copytree(shared / name, builds["reference"] / name, symlinks=True)
    reference = builds["reference"] / "openttd"
    binaries = {"reference": reference, "candidate": args.candidate or builds["candidate"] / "openttd-rust"}
    if args.self:
        builds["candidate"], binaries["candidate"] = builds["reference"], reference
    for role, binary in binaries.items():
        if not Path(binary).is_file():
            parser.error(f"{role} binary {binary} is missing; run python3 tools/migration.py build")

    started = time.monotonic()
    results = []
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        # The driver's environment supplies bootstrapped runtime libraries locally.
        env = migration.environment()
        futures = {pool.submit(run_scenario, s, binaries, builds, out, args.limit, args.timeout, env): s for s in scenarios}
        for future in concurrent.futures.as_completed(futures):
            result = future.result()
            results.append(result)
            status = "ok  " if result["passed"] else "FAIL"
            known = f", {len(result['known_failures'])} known" if result["known_failures"] else ""
            print(f"{status} {result['scenario']}: {result['snapshots']} snapshots, "
                  f"{result['chunks_compared']} chunks{known}", flush=True)
            for problem in result["problems"]:
                print(f"     {problem}", flush=True)
            for diff in result["differences"][:args.limit]:
                print(f"     {diff['snapshot']} {diff['chunk']}/{diff['element']}/{diff['field']}: "
                      f"{diff['reference']} -> {diff['candidate']}", flush=True)
    results.sort(key=lambda r: r["scenario"])
    report = {
        "mode": "reference-vs-reference" if args.self else "reference-vs-candidate",
        "candidate_commit": migration.git("rev-parse", "HEAD"),
        "masks": {":".join(key): reason for key, reason in MASKS.items()},
        "known_failures": {":".join(key): issue for key, issue in KNOWN_FAILURES.items()},
        "seconds": round(time.monotonic() - started, 1),
        "results": results,
        "passed": all(r["passed"] for r in results),
    }
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    shutil.rmtree(out / "reference-runtime", ignore_errors=True)
    passed = sum(r["passed"] for r in results)
    print(f"Simulation: {passed}/{len(results)} scenarios equal, "
          f"{sum(r['snapshots'] for r in results)} snapshots in {report['seconds']} s. Report: {out / 'report.json'}")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
