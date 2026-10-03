#!/usr/bin/env python3
"""Compare scoped Admin conversion gaps using unchanged pinned bodies and the real VM."""

import hashlib
import json
from pathlib import Path
import runpy
import subprocess

ROOT = Path(__file__).resolve().parents[1]
MIGRATION = runpy.run_path(str(ROOT / "tools/migration.py"))
REFERENCE = MIGRATION["REFERENCE"].resolve()
OUT = ROOT / ".local/admin-conversion-comparison"


def extract(text, signature):
    start = text.index(signature)
    opened = text.index("{", start)
    depth, end = 1, opened + 1
    # These bounded production bodies contain balanced braces, including strings.
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def main():
    MIGRATION["ensure_reference"]()
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "report.json").unlink(missing_ok=True)
    admin = (REFERENCE / "src/script/api/script_admin.cpp").read_text()
    event = (REFERENCE / "src/script/api/script_event_types.cpp").read_text()
    squirrel = (REFERENCE / "src/script/squirrel.cpp").read_text()
    bodies = {
        "outgoing": extract(admin, "bool ScriptAdminMakeJSON("),
        "incoming": extract(event, "static bool ScriptEventAdminPortReadValue("),
        "get_object": extract(event, "SQInteger ScriptEventAdminPort::GetObject("),
        "allocator": extract(squirrel, "struct ScriptAllocator {") + ";",
    }
    original = OUT / "original.cpp"
    original.write_text('#include "stdafx.h"\n#include "script/admin_conversion.hpp"\n'
                        '#include "script/api/script_log.hpp"\n#include "script/script_instance.hpp"\n'
                        + bodies["outgoing"].replace("ScriptAdminMakeJSON", "OriginalOutgoing") + "\n"
                        + bodies["incoming"].replace("ScriptEventAdminPortReadValue", "OriginalReadValue") + "\n"
                        + bodies["get_object"].replace("SQInteger ScriptEventAdminPort::GetObject(HSQUIRRELVM vm)",
                                                     "SQInteger OriginalGetObject(HSQUIRRELVM vm, const std::string &raw)")
                        .replace("this->json", "raw").replace("ScriptEventAdminPortReadValue", "OriginalReadValue") + "\n")
    direct = bodies["get_object"].replace("SQInteger ScriptEventAdminPort::GetObject(HSQUIRRELVM vm)",
                                          "SQInteger OriginalFromJSON(HSQUIRRELVM vm, nlohmann::json &json)")
    direct = direct.replace("\tauto json = nlohmann::json::parse(this->json, nullptr, false);", "")
    direct = direct.replace("ScriptEventAdminPortReadValue", "OriginalReadValue")
    with original.open("a") as output:
        output.write(direct + "\n")
    allocator = OUT / "allocator.cpp"
    allocator.write_text('#include "stdafx.h"\n#include "3rdparty/fmt/format.h"\n#include <cstdint>\n'
                         '#include "error_func.h"\n#include "script/script_fatalerror.hpp"\n#include <squirrel.h>\n'
                         + 'static struct { struct { uint32_t script_max_memory_megabytes = 1; } script; } _settings_game;\n'
                         + bodies["allocator"] + "\nScriptAllocator *_squirrel_allocator = nullptr;\n"
                         + 'void *sq_vm_malloc(SQUnsignedInteger n) { return _squirrel_allocator->Malloc(n); }\n'
                         + 'void *sq_vm_realloc(void *p, SQUnsignedInteger a, SQUnsignedInteger b) { return _squirrel_allocator->Realloc(p,a,b); }\n'
                         + 'void sq_vm_free(void *p, SQUnsignedInteger n) { _squirrel_allocator->Free(p,n); }\n'
                         + 'void BeginAllocator() { _settings_game.script.script_max_memory_megabytes = 1; _squirrel_allocator = new ScriptAllocator; }\n'
                         + 'size_t EndAllocator() { auto n = _squirrel_allocator->GetAllocatedSize(); delete _squirrel_allocator; _squirrel_allocator = nullptr; return n; }\n')
    env = MIGRATION["environment"]()
    config = MIGRATION["rust_configuration"](ROOT / "build-rust")
    archive = MIGRATION["rust_archive"](ROOT / "build-rust")
    cache = {}
    for line in (ROOT / "build-rust/CMakeCache.txt").read_text().splitlines():
        if line and not line.startswith(("#", "//")) and "=" in line:
            key, value = line.split("=", 1)
            cache[key.split(":", 1)[0]] = value
    flags = [cache["CMAKE_CXX_COMPILER"], "-std=c++20", "-DFMT_HEADER_ONLY", "-DWITH_RUST", "-DUNIX",
             "-ffunction-sections", "-fdata-sections", "-I", str(ROOT / "src"),
             "-I", str(ROOT / "src/3rdparty/squirrel/include"),
             "-I", str(ROOT / "src/script/api"), "-I", str(ROOT / "build-rust/generated"),
             "-I", str(ROOT / "build-rust/generated/script/api")]
    if config["pointer_bytes"] == 8:
        flags.append("-DPOINTER_IS_64BIT")
    if config["platform"] == "Darwin":
        flags.extend(["-arch", "arm64"])
        if config["sdk"]:
            flags.extend(["-isysroot", config["sdk"]])
        if config["deployment_target"]:
            flags.append("-mmacosx-version-min=" + config["deployment_target"])
        raise RuntimeError("GNU link wrapping in this exception/owner fixture currently requires native Linux")
    commands = []

    def compile_run(command, label):
        commands.append(command)
        with (OUT / f"{label}.log").open("wb") as log:
            subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)

    # One serial compiler, retaining the actual bundled VM. Its sources are unchanged.
    common_sources = sorted((ROOT / "src/3rdparty/squirrel/squirrel").glob("*.cpp"))
    vm_sources = sorted((ROOT / "src/3rdparty/squirrel").rglob("*.cpp")) + sorted((ROOT / "src/3rdparty/squirrel").rglob("*.h"))
    for source in vm_sources:
        if source.read_bytes() != (REFERENCE / source.relative_to(ROOT)).read_bytes():
            raise RuntimeError(f"Bundled VM source changed: {source}")
    common_sources += [ROOT / "src/core/string_consumer.cpp", ROOT / "src/core/utf8.cpp", allocator]
    objects = []
    for number, source in enumerate(common_sources):
        obj = OUT / f"vm-{number}.o"
        compile_run([*flags, "-O2", "-c", str(source), "-o", str(obj)], f"vm-{number}-compile")
        objects.append(obj)
    outputs = {}
    for mode in ("O0", "O2"):
        executable = OUT / f"comparison-{mode}"
        sources = [ROOT / "tools/migration/admin-conversion-comparison.cpp", original,
                   ROOT / "src/script/admin_conversion.cpp", ROOT / "src/script/api/script_admin.cpp",
                   ROOT / "src/script/api/script_event_types.cpp"]
        command = [*flags, "-" + mode, *map(str, sources), *map(str, objects), str(archive),
                   "-Wl,--gc-sections", "-Wl,--wrap=openttd_rust_admin_conversion_create",
                   "-Wl,--wrap=openttd_rust_admin_conversion_destroy", "-ldl", "-lpthread", "-lm", "-o", str(executable)]
        compile_run(command, f"{mode}-compile")
        result = subprocess.run([str(executable)], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (OUT / f"{mode}.out").write_bytes(result.stdout)
        (OUT / f"{mode}.err").write_bytes(result.stderr)
        if result.returncode or result.stderr:
            raise RuntimeError(f"{mode} comparison failed, exit {result.returncode}; see retained output")
        outputs[mode] = result.stdout
    # Each process already compares original/candidate on the same live VM.
    # Instance-key hashes depend on allocation addresses: separate processes may
    # legitimately visit stringification collisions in a different order.
    MIGRATION["ensure_reference"]()
    report = {"baseline": MIGRATION["BASELINE"], "candidate_commit": MIGRATION["git"]("rev-parse", "HEAD"),
              "candidate_status": MIGRATION["git"]("status", "--porcelain"), "commands": commands,
              "rust_archive": str(archive), "modes": ["O0", "O2"],
              "records_per_mode": {mode: len(output.splitlines()) for mode, output in outputs.items()},
              "original_bodies_sha256": {key: hashlib.sha256(value.encode()).hexdigest() for key, value in bodies.items()},
              "bundled_vm_sha256": {str(source.relative_to(ROOT)): hashlib.sha256(source.read_bytes()).hexdigest() for source in vm_sources},
              "fixture_sha256": hashlib.sha256((ROOT / "tools/migration/admin-conversion-comparison.cpp").read_bytes()).hexdigest(),
              "outputs_sha256": {key: hashlib.sha256(value).hexdigest() for key, value in outputs.items()},
              "limits": "Native Linux; logger records entry text/state rather than game-log storage. Extra control allocations have different failure timing. Unchanged ScriptAllocator reads a fixture-only script_max_memory_megabytes setting fixed to 1.",
              "passed": True}
    (OUT / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"Admin conversion comparisons passed; {OUT / 'report.json'}")


if __name__ == "__main__":
    main()
