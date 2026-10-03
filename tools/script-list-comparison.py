#!/usr/bin/env python3
"""Compare only identified ScriptList owner, real-VM and persistence gaps."""

from collections import Counter
import hashlib
import json
from pathlib import Path
import runpy
import subprocess

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
    allocator.write_text('#include "stdafx.h"\n#include "3rdparty/fmt/format.h"\n#include "error_func.h"\n'
                         '#include "script/script_fatalerror.hpp"\n#include <squirrel.h>\n'
                         'static struct { struct { uint32_t script_max_memory_megabytes = 1; } script; } _settings_game;\n'
                         + allocator_body + '\nScriptAllocator *_squirrel_allocator = nullptr;\n'
                         'void *sq_vm_malloc(SQUnsignedInteger n) { return _squirrel_allocator->Malloc(n); }\n'
                         'void *sq_vm_realloc(void *p, SQUnsignedInteger a, SQUnsignedInteger b) { return _squirrel_allocator->Realloc(p,a,b); }\n'
                         'void sq_vm_free(void *p, SQUnsignedInteger n) { _squirrel_allocator->Free(p,n); }\n'
                         'void BeginAllocator() { _squirrel_allocator = new ScriptAllocator; }\n'
                         'size_t EndAllocator() { auto n = _squirrel_allocator->GetAllocatedSize(); delete _squirrel_allocator; _squirrel_allocator = nullptr; return n; }\n')
    tile_source = (REFERENCE / "src/script/api/script_tilelist.cpp").read_text()
    signatures = ("bool ScriptTileList::SaveObject(", "ScriptObject *ScriptTileList::CloneObject(")
    bodies = [extract(tile_source, signature) for signature in signatures]
    if any(body not in (ROOT / "src/script/api/script_tilelist.cpp").read_text() for body in bodies):
        raise RuntimeError("TileList persistence/clone changed")
    tile = OUT / "tile-persistence.cpp"
    tile.write_text('#include "stdafx.h"\n#include "script/api/script_tilelist.hpp"\n' + "\n".join(bodies) + "\n")
    vm_sources = sorted((REFERENCE / "src/3rdparty/squirrel/squirrel").glob("*.cpp"))
    for source in vm_sources:
        if source.read_bytes() != (ROOT / source.relative_to(REFERENCE)).read_bytes():
            raise RuntimeError("Bundled VM source changed")
    commands = []

    def compile(command, name):
        commands.append(command)
        with (OUT / f"{name}-compile.log").open("wb") as log:
            subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)

    # VM and allocator are real unchanged code. Each label uses its actual string helpers.
    flags = ["g++", "-std=c++20", "-DFMT_HEADER_ONLY", "-DUNIX", "-DPOINTER_IS_64BIT", "-Wno-multichar",
             "-ffunction-sections", "-fdata-sections"]
    objects = {}
    for group, source_root in (("reference", REFERENCE), ("rust", ROOT)):
        objects[group] = []
        group_flags = flags + (["-DWITH_RUST"] if group == "rust" else [])
        for number, source in enumerate(vm_sources + [allocator]):
            obj = OUT / f"vm-{group}-{number}.o"
            compile([*group_flags, "-O2", "-I", str(source_root / "src"), "-I", str(source_root / "src/3rdparty/squirrel/include"),
                     "-c", str(source), "-o", str(obj)], f"vm-{group}-{number}")
            objects[group].append(obj)
    outputs = {}
    fixture = ROOT / "tools/migration/script-list-comparison.cpp"
    for mode in ("O0", "O2"):
        for label, source in (("reference", REFERENCE), ("candidate", ROOT), ("candidate-cpp", ROOT)):
            name = f"{label}-{mode}"
            command = [*flags, "-" + mode, "-I", str(source / "src"), "-I", str(source / "src/3rdparty/squirrel/include"),
                       str(fixture), str(tile), str(source / "src/script/api/script_list.cpp"),
                       str(source / "src/core/string_consumer.cpp"), str(source / "src/core/utf8.cpp"),
                       *map(str, objects["rust" if label == "candidate" else "reference"]), "-Wl,--gc-sections", "-o", str(OUT / name)]
            if label == "candidate":
                command.extend(["-DWITH_RUST", str(archive), "-ldl", "-lpthread", "-lm"])
            compile(command, name)
            result = subprocess.run([str(OUT / name)], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            (OUT / f"{name}.out").write_bytes(result.stdout)
            (OUT / f"{name}.err").write_bytes(result.stderr)
            if result.returncode or result.stderr:
                raise RuntimeError(f"{name} failed; see retained output")
            outputs[label, mode] = result.stdout
            if label != "reference" and result.stdout != outputs["reference", mode]:
                expected, actual = outputs["reference", mode].splitlines(), result.stdout.splitlines()
                failures = [{"line": i + 1, "expected": expected[i].decode() if i < len(expected) else "<missing>",
                             "actual": actual[i].decode() if i < len(actual) else "<missing>"}
                            for i in range(max(len(expected), len(actual)))
                            if (expected[i] if i < len(expected) else None) != (actual[i] if i < len(actual) else None)]
                (OUT / "failures.json").write_text(json.dumps(failures, indent=2) + "\n")
                raise RuntimeError(f"{name}: {len(failures)} discrepancies")
    MIGRATION["ensure_reference"]()
    records = outputs["reference", "O0"].splitlines()
    report = {"baseline": MIGRATION["BASELINE"], "candidate_commit": MIGRATION["git"]("rev-parse", "HEAD"),
              "candidate_status": MIGRATION["git"]("status", "--porcelain"), "rust_archive": str(archive),
              "commands": commands, "records_per_mode": len(records),
              "record_counts": dict(Counter(line.split()[0].decode() for line in records)),
              "modes": ["O0", "O2"], "source_sha256": {
                  name: hashlib.sha256((REFERENCE / name).read_bytes()).hexdigest() for name in
                  ("src/script/api/script_list.cpp", "src/script/api/script_list.hpp", "src/script/api/script_tilelist.cpp", "src/script/squirrel.cpp")},
              "allocator_body_sha256": hashlib.sha256(allocator_body.encode()).hexdigest(),
              "fixture_sha256": hashlib.sha256(fixture.read_bytes()).hexdigest(),
              "outputs_sha256": {f"{label}-{mode}": hashlib.sha256(output).hexdigest() for (label, mode), output in outputs.items()},
              "limits": ["Full unchanged game regressions are primary; this only closes listed gaps",
                         "Command-disabling scope uses a host bool sentinel; actual VM/allocator/operation limit remain unchanged",
                         "Save/clone TileList bodies are extracted unchanged; world population is not simulated",
                         "Undefined signed-overflow/evaded invalid-iterator valuation cases excluded"], "passed": True}
    (OUT / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"ScriptList comparisons passed: {len(records)} records at O0/O2; {OUT / 'report.json'}")


if __name__ == "__main__":
    main()
