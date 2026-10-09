#!/usr/bin/env python3
"""Compare only identified ScriptList owner, real-VM and persistence gaps."""

import hashlib
import json
import runpy
import subprocess
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MIGRATION = runpy.run_path(str(ROOT / "tools/migration.py"))
REFERENCE = MIGRATION["REFERENCE"].resolve()
OUT = ROOT / ".local/script-list-comparison"


def extract(text, signature):
    start = text.index(signature)
    opened = text.index("{", start)
    depth, end = 1, opened + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def main():
    MIGRATION["ensure_reference"]()
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "report.json").unlink(missing_ok=True)
    env = MIGRATION["environment"]()
    archive = MIGRATION["rust_archive"](ROOT / "build-rust")
    squirrel = (REFERENCE / "src/script/squirrel.cpp").read_text()
    allocator_body = extract(squirrel, "struct ScriptAllocator {") + ";"
    if allocator_body not in (ROOT / "src/script/squirrel.cpp").read_text():
        raise RuntimeError("Original VM allocator changed")
    allocator = OUT / "allocator.cpp"
    allocator.write_text(
        '#include "stdafx.h"\n#include "3rdparty/fmt/format.h"\n#include "error_func.h"\n'
        '#include "script/script_fatalerror.hpp"\n#include <squirrel.h>\n'
        "static struct { struct { uint32_t script_max_memory_megabytes = 1; } script; } _settings_game;\n"
        + allocator_body
        + "\nScriptAllocator *_squirrel_allocator = nullptr;\n"
        "void *sq_vm_malloc(SQUnsignedInteger n) { return _squirrel_allocator->Malloc(n); }\n"
        "void *sq_vm_realloc(void *p, SQUnsignedInteger a, SQUnsignedInteger b) { return _squirrel_allocator->Realloc(p,a,b); }\n"
        "void sq_vm_free(void *p, SQUnsignedInteger n) { _squirrel_allocator->Free(p,n); }\n"
        "void BeginAllocator() { _squirrel_allocator = new ScriptAllocator; }\n"
        "size_t EndAllocator() { auto n = _squirrel_allocator->GetAllocatedSize(); delete _squirrel_allocator; _squirrel_allocator = nullptr; return n; }\n"
    )
    tile_source = (REFERENCE / "src/script/api/script_tilelist.cpp").read_text()
    signatures = (
        "bool ScriptTileList::SaveObject(",
        "ScriptObject *ScriptTileList::CloneObject(",
    )
    bodies = [extract(tile_source, signature) for signature in signatures]
    if any(
        body not in (ROOT / "src/script/api/script_tilelist.cpp").read_text()
        for body in bodies
    ):
        raise RuntimeError("TileList persistence/clone changed")
    tile = OUT / "tile-persistence.cpp"
    tile.write_text(
        '#include "stdafx.h"\n#include "script/api/script_tilelist.hpp"\n'
        + "\n".join(bodies)
        + "\n"
    )
    # Scalar cargo gaps use exact original Update/SetValue/destructor and planned
    # traversal bodies. Only direct collector construction omits world lookup;
    # saved-game regressions remain the actual station/cargo validity evidence.
    station_source = (REFERENCE / "src/script/api/script_stationlist.cpp").read_text()
    candidate_station = (ROOT / "src/script/api/script_stationlist.cpp").read_text()
    validation = extract(station_source, "CargoCollector::CargoCollector(")
    validation_body = validation[
        validation.index("{") + 1 : validation.rfind("}")
    ].strip()
    active_station = candidate_station.split("#else\n\nclass CargoCollector", 1)[0]
    if validation_body.replace("\n\t", "\n\t\t") not in active_station:
        raise RuntimeError("Station-before-cargo validation body/order changed")
    guards = "if (collector.GE() == nullptr) return;\n\tif (!collector.GE()->HasData()) return;"
    if guards not in active_station or guards not in station_source:
        raise RuntimeError("Null goods / HasData guard order changed")
    cargo_signatures = (
        "class CargoCollector {",
        "CargoCollector::~CargoCollector()",
        "void CargoCollector::SetValue()",
        "void CargoCollector::Update(",
    )
    cargo_bodies = {
        signature: extract(station_source, signature) for signature in cargo_signatures
    }
    if any(body not in candidate_station for body in cargo_bodies.values()):
        raise RuntimeError("Portable cargo reducer changed")
    initializer = station_source[
        station_source.index("CargoCollector::CargoCollector(") :
    ]
    initializer = initializer[: initializer.index("{")]
    cargo_text = "\n".join(
        body + (";" if name == "class CargoCollector {" else "")
        for name, body in cargo_bodies.items()
    )
    cargo_text = cargo_text.replace(
        cargo_bodies["void CargoCollector::Update("],
        "template <ScriptStationList_Cargo::CargoSelector Tselector>\n"
        + cargo_bodies["void CargoCollector::Update("],
    )
    cargo_text += "\n" + initializer + "{ /* Direct fixture: no world access. */ }\n"
    planned = extract(station_source, "void ScriptStationList_CargoPlanned::Add(")
    planned_loop = planned[
        planned.index("FlowStatMap::const_iterator iter") : planned.rfind("}")
    ]
    planned_loop = planned_loop.replace("collector.GE()->GetData().flows", "flows")
    cargo_text += (
        "\ntemplate <ScriptStationList_Cargo::CargoSelector Tselector>\nvoid OriginalCargoPlanned(CargoCollector &collector, const FlowStatMap &flows) {\n"
        + planned_loop
        + "}\n"
    )
    filtered = extract(
        station_source,
        "ScriptStationList_CargoPlannedFromByVia::ScriptStationList_CargoPlannedFromByVia(",
    )
    filtered_loop = filtered[
        filtered.index("FlowStatMap::const_iterator iter") : filtered.rfind("}")
    ]
    filtered_loop = filtered_loop.replace("collector.GE()->GetData().flows", "flows")
    cargo_text += (
        "\nvoid OriginalCargoFind(CargoCollector &collector, const FlowStatMap &flows, StationID from) {\nconstexpr auto CS_FROM_BY_VIA = ScriptStationList_Cargo::CS_FROM_BY_VIA;\n"
        + filtered_loop
        + "}\n"
    )
    cargo_text = cargo_text.replace("CargoCollector", "ReferenceCargoCollector")
    # Reader type deduction accepts both native shares and the Rust borrowing
    # facade; the copied reference traversal and reduction expressions stay intact.
    cargo_text = cargo_text.replace(
        "const FlowStat::SharesMap *shares", "const auto *shares"
    )
    cargo_text = cargo_text.replace(
        "FlowStat::SharesMap::const_iterator flow_iter", "auto flow_iter"
    )
    (OUT / "cargo-reducer.hpp").write_text(cargo_text)
    vm_sources = sorted((REFERENCE / "src/3rdparty/squirrel/squirrel").glob("*.cpp"))
    for source in vm_sources:
        if source.read_bytes() != (ROOT / source.relative_to(REFERENCE)).read_bytes():
            raise RuntimeError("Bundled VM source changed")
    commands = []

    def compile(command, name):
        commands.append(command)
        with (OUT / f"{name}-compile.log").open("wb") as log:
            subprocess.run(
                command, env=env, stdout=log, stderr=subprocess.STDOUT, check=True
            )

    # The inherited export parser tracks access markers without recognizing an
    # un-inherited nested helper class. Preserve the actual List API in all modes.
    exporter = Path("cmake/scripts/SquirrelExport.cmake")
    if (ROOT / exporter).read_bytes() != (REFERENCE / exporter).read_bytes():
        raise RuntimeError("Original binding generator changed")
    binding_hashes = {}
    for apilc, apiuc in (("ai", "AI"), ("game", "GS"), ("template", "Template")):
        generated = []
        for label, source in (("reference", REFERENCE), ("candidate", ROOT)):
            target = OUT / f"bindings-{label}-{apilc}.sq.hpp"
            compile(
                [
                    "cmake",
                    f"-DSCRIPT_API_SOURCE_FILE={source / 'src/script/api/squirrel_export.sq.hpp.in'}",
                    f"-DSCRIPT_API_BINARY_FILE={target}",
                    f"-DSCRIPT_API_FILE={source / 'src/script/api/script_list.hpp'}",
                    f"-DAPIUC={apiuc}",
                    f"-DAPILC={apilc}",
                    "-P",
                    str(source / exporter),
                ],
                f"bindings-{label}-{apilc}",
            )
            generated.append(target.read_bytes())
        if generated[0] != generated[1]:
            raise RuntimeError(f"ScriptList {apilc} generated bindings changed")
        binding_hashes[apilc] = hashlib.sha256(generated[0]).hexdigest()

    # VM and allocator are real unchanged code. Each label uses its actual string helpers.
    flags = [
        "g++",
        "-std=c++20",
        "-DFMT_HEADER_ONLY",
        "-DUNIX",
        "-DPOINTER_IS_64BIT",
        "-Wno-multichar",
        "-ffunction-sections",
        "-fdata-sections",
        "-I",
        str(OUT),
        "-I",
        str(ROOT / "build-rust/generated"),
    ]
    objects = {}
    for group, source_root in (("reference", REFERENCE), ("rust", ROOT)):
        objects[group] = []
        group_flags = flags + (["-DWITH_RUST"] if group == "rust" else [])
        for number, source in enumerate(vm_sources + [allocator]):
            # Relative includes resolve beside the source; use this group's copy.
            if source.is_relative_to(REFERENCE):
                source = source_root / source.relative_to(REFERENCE)
            obj = OUT / f"vm-{group}-{number}.o"
            compile(
                [
                    *group_flags,
                    "-O2",
                    "-I",
                    str(source_root / "src"),
                    "-I",
                    str(source_root / "src/3rdparty/squirrel/include"),
                    "-c",
                    str(source),
                    "-o",
                    str(obj),
                ],
                f"vm-{group}-{number}",
            )
            objects[group].append(obj)
    outputs = {}
    fixture = ROOT / "tools/migration/script-list-comparison.cpp"
    for mode in ("O0", "O2"):
        for label, source in (
            ("reference", REFERENCE),
            ("candidate", ROOT),
            ("candidate-cpp", ROOT),
        ):
            name = f"{label}-{mode}"
            command = [
                *flags,
                "-" + mode,
                "-I",
                str(source / "src"),
                "-I",
                str(source / "src/3rdparty/squirrel/include"),
                str(fixture),
                str(tile),
                str(source / "src/script/api/script_list.cpp"),
                str(source / "src/core/string_consumer.cpp"),
                str(source / "src/core/utf8.cpp"),
                *map(str, objects["rust" if label == "candidate" else "reference"]),
                "-Wl,--gc-sections",
                "-o",
                str(OUT / name),
            ]
            if label == "candidate":
                command.extend(
                    ["-DWITH_RUST", str(archive), "-ldl", "-lpthread", "-lm"]
                )
            compile(command, name)
            result = subprocess.run(
                [str(OUT / name)],
                env=env,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            (OUT / f"{name}.out").write_bytes(result.stdout)
            (OUT / f"{name}.err").write_bytes(result.stderr)
            if result.returncode or result.stderr:
                raise RuntimeError(f"{name} failed; see retained output")
            outputs[label, mode] = result.stdout
            if label != "reference" and result.stdout != outputs["reference", mode]:
                expected, actual = (
                    outputs["reference", mode].splitlines(),
                    result.stdout.splitlines(),
                )
                failures = [
                    {
                        "line": i + 1,
                        "expected": expected[i].decode()
                        if i < len(expected)
                        else "<missing>",
                        "actual": actual[i].decode()
                        if i < len(actual)
                        else "<missing>",
                    }
                    for i in range(max(len(expected), len(actual)))
                    if (expected[i] if i < len(expected) else None)
                    != (actual[i] if i < len(actual) else None)
                ]
                (OUT / "failures.json").write_text(
                    json.dumps(failures, indent=2) + "\n"
                )
                raise RuntimeError(f"{name}: {len(failures)} discrepancies")
    MIGRATION["ensure_reference"]()
    records = outputs["reference", "O0"].splitlines()
    report = {
        "baseline": MIGRATION["BASELINE"],
        "candidate_commit": MIGRATION["git"]("rev-parse", "HEAD"),
        "candidate_status": MIGRATION["git"]("status", "--porcelain"),
        "rust_archive": str(archive),
        "commands": commands,
        "records_per_mode": len(records),
        "record_counts": dict(Counter(line.split()[0].decode() for line in records)),
        "modes": ["O0", "O2"],
        "source_sha256": {
            name: hashlib.sha256((REFERENCE / name).read_bytes()).hexdigest()
            for name in (
                "src/script/api/script_list.cpp",
                "src/script/api/script_list.hpp",
                "src/script/api/script_tilelist.cpp",
                "src/script/squirrel.cpp",
            )
        },
        "cargo_unchanged_validation_sha256": hashlib.sha256(
            validation_body.encode()
        ).hexdigest(),
        "cargo_unchanged_goods_guards_sha256": hashlib.sha256(
            guards.encode()
        ).hexdigest(),
        "cargo_original_body_sha256": {
            name: hashlib.sha256(body.encode()).hexdigest()
            for name, body in cargo_bodies.items()
        },
        "cargo_original_planned_sha256": hashlib.sha256(planned.encode()).hexdigest(),
        "cargo_original_find_sha256": hashlib.sha256(filtered.encode()).hexdigest(),
        "allocator_body_sha256": hashlib.sha256(allocator_body.encode()).hexdigest(),
        "fixture_sha256": hashlib.sha256(fixture.read_bytes()).hexdigest(),
        "script_list_binding_sha256": binding_hashes,
        "outputs_sha256": {
            f"{label}-{mode}": hashlib.sha256(output).hexdigest()
            for (label, mode), output in outputs.items()
        },
        "limits": [
            "Full unchanged game regressions are primary; this only closes listed gaps",
            "Command-disabling scope uses a host bool sentinel; actual VM/allocator/operation limit remain unchanged",
            "Save/clone TileList bodies are extracted unchanged; world population is not simulated",
            "Undefined signed-overflow/evaded invalid-iterator valuation cases excluded",
            "Load nonnumeric/integer failed-getter and unrepresentable float cases excluded (deferred issue #67)",
            "VM filter uses four stable typed items, including two live index reads across a real callback; no simulated world",
            "Cargo fixture directly initializes scalar collector without world validation; unchanged stationlist saved-game regression is world/query adapter evidence",
            "Failed station/cargo/company policy and missing HasData supplemental probes are unexecuted; typed validation and goods-guard bodies/order are checked unchanged",
        ],
        "passed": True,
    }
    (OUT / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        f"ScriptList comparisons passed: {len(records)} records at O0/O2; {OUT / 'report.json'}"
    )


if __name__ == "__main__":
    main()
