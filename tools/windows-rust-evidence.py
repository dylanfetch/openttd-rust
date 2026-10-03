#!/usr/bin/env python3
"""Record actual native MSVC target, CRT, role flags, linkage and fresh generators."""

import argparse
import json
from pathlib import Path
import re
import subprocess

import migration


def assertion_policy(command, consumer):
    """Original RelWithDebInfo: NDEBUG everywhere, WITH_ASSERT for game/tests only."""
    definitions = set(re.findall(r'(?:^|[\s"])[-/]D\s*"?([A-Za-z_]\w*)', command))
    expected = consumer in ("game", "tests", "abi")
    if "NDEBUG" not in definitions or ("WITH_ASSERT" in definitions) != expected:
        raise RuntimeError(f"{consumer} has unexpected original assertion flags: {command}")
    runtimes = re.findall(r'(?:^|[\s"])[-/](M[DT]d?)(?=[\s"]|$)', command)
    if not runtimes or any(runtime != "MT" for runtime in runtimes):
        raise RuntimeError(f"{consumer} is not exclusively static release CRT: {command}")
    return {"NDEBUG": True, "WITH_ASSERT": expected, "runtime_flags": runtimes}


def expand_responses(command, build):
    """Require every Ninja response file, retained with -d keeprsp, recursively."""
    seen, responses = set(), {}

    def expand(text):
        for match in re.finditer(r'@(?:"([^"]+)"|([^\s"&]+))', text):
            token = match[1] or match[2]
            path = (build / token).resolve()
            if path in seen:
                continue
            seen.add(path)
            content = path.read_text(encoding="utf-8-sig")
            responses[str(path)] = content
            expand(content)
    expand(command)
    return command + "\n" + "\n".join(responses.values()), responses


def linked_archive(command, build, archive):
    tokens = re.findall(r'(?:"([^"\n]*openttd_kernels\.lib)"|([^\s"]*openttd_kernels\.lib))(?=[\s"]|$)', command)
    return any((build / (quoted or plain)).resolve() == archive.resolve() for quoted, plain in tokens)


def native_arguments_linked(command, arguments):
    """Require the complete native argument group, including repeated libraries."""
    tokens = [r'(?:"' + re.escape(arg) + r'"|' + re.escape(arg) + r')' for arg in arguments]
    return bool(tokens) and re.search(r'(?:^|\s)' + r'\s+'.join(tokens) + r'(?=\s|$)', command, re.I) is not None


def select_link(lines, build, binary_name):
    """Find the target output after expanding retained linker response files."""
    selected = []
    for line in lines:
        # Ninja lists transitive compile and custom commands as well as links.
        if not re.search(r"(?:vs_link_exe|\blink(?:\.exe)?\b)", line, re.I):
            continue
        expanded, responses = expand_responses(line, build)
        normalized = expanded.replace("\\", "/").replace('"', '').lower()
        pattern = r"(?:^|\s)/out:" + re.escape(binary_name.lower()) + r"(?=\s|$)"
        if re.search(pattern, normalized):
            selected.append((line, expanded, responses))
    if len(selected) != 1:
        raise RuntimeError(f"Expected one actual link command for {binary_name}")
    return selected[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", choices=("i686-pc-windows-msvc", "x86_64-pc-windows-msvc"), required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--tools", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    (output / "report.json").unlink(missing_ok=True)
    report = {"commit": migration.git("rev-parse", "HEAD"), "status": migration.git("status", "--porcelain"),
              "commands": [], "builds": {}, "passed": False}

    def run(argv, name, cwd=None):
        command = list(map(str, argv))
        result = subprocess.run(command, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (output / f"{name}.log").write_bytes(result.stdout)
        report["commands"].append({"argv": command, "cwd": str(cwd) if cwd else None, "exit_code": result.returncode})
        if result.returncode:
            raise RuntimeError(f"{name} failed: {result.stdout.decode(errors='replace')}")
        return result.stdout

    for label, build in (("game", args.build.resolve()), ("tools", args.tools.resolve())):
        configuration = migration.rust_configuration(build)
        if configuration["platform"] != "Windows" or configuration["target"] != args.target:
            raise RuntimeError("This evidence requires the validated native Windows MSVC mode")
        archive = migration.rust_archive(build)
        for name in ("CMakeCache.txt", "rust-toolchain.txt", "rust-native-libs.log", "rust-build.log", "rust-build-configuration.txt", "compile_commands.json"):
            (output / f"{label}-{name}").write_bytes((build / name).read_bytes())
        compiler = next((build / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake"))
        (output / f"{label}-CMakeCXXCompiler.cmake").write_bytes(compiler.read_bytes())
        actual = re.findall(r"native-static-libs: ([^\r\n]+)", (build / "rust-build.log").read_text())
        configured = re.findall(r"native-static-libs: ([^\r\n]+)", (build / "rust-native-libs.log").read_text())
        if not actual or not configured or actual[-1] != configured[-1]:
            raise RuntimeError("Actual target/CRT archive native libraries differ from configure-time query")
        consumers = {"strgen": ("/strgen/strgen.cpp", "src/strgen/strgen.exe"),
                     "settingsgen": ("/settingsgen/settingsgen.cpp", "src/settingsgen/settingsgen.exe")}
        if label == "game":
            consumers.update({"game": ("/openttd.cpp", "openttd.exe"),
                              "tests": ("/tests/test_main.cpp", "openttd_test.exe"),
                              "abi": ("/migration/windows-abi.cpp", "openttd_rust_abi.exe")})
        compiles = json.loads((build / "compile_commands.json").read_text())
        evidence = {}
        for consumer, (suffix, binary_name) in consumers.items():
            selected = [entry for entry in compiles if entry["file"].replace("\\", "/").endswith(suffix)]
            if not selected:
                raise RuntimeError(f"Missing actual {label}/{consumer} compile command")
            for entry in selected:
                if "WITH_RUST" not in entry["command"]:
                    raise RuntimeError(f"Missing {consumer} WITH_RUST propagation")
                policy = assertion_policy(entry["command"], consumer)
            target = {"game": "openttd", "tests": "openttd_test", "abi": "openttd_rust_abi"}.get(consumer, consumer)
            lines = run(["ninja", "-C", build, "-t", "commands", target], f"{label}-{consumer}-commands").decode().splitlines()
            link, expanded, responses = select_link(lines, build, binary_name)
            if not linked_archive(expanded, build, archive):
                raise RuntimeError(f"{label}/{consumer} did not link the exact target archive")
            if not native_arguments_linked(expanded, configuration["native_libraries"]):
                raise RuntimeError(f"{label}/{consumer} did not retain the ordered native argument group")
            (output / f"{label}-{consumer}-link-and-responses.json").write_text(json.dumps({"command": link, "responses": responses}, indent=2) + "\n")
            machine = "14C" if configuration["pointer_bytes"] == 4 else "8664"
            headers = run(["dumpbin", "/headers", build / binary_name], f"{label}-{consumer}-pe-headers")
            if not re.search(rb"\b" + machine.encode() + rb" machine", headers, re.I):
                raise RuntimeError(f"{label}/{consumer} PE machine differs from configured Rust target")
            imports = run(["dumpbin", "/dependents", build / binary_name], f"{label}-{consumer}-imports")
            if re.search(rb"\b(?:VCRUNTIME\w*|MSVCP\w*|UCRTBASE)\.dll", imports, re.I):
                raise RuntimeError(f"{label}/{consumer} directly imports a dynamic C/C++ runtime")
            map_file = build / f"{target}.map"
            map_text = map_file.read_text(errors="replace")
            (output / f"{label}-{consumer}.map").write_text(map_text)
            if not re.search(r"\b_?openttd_rust_\w+\b", map_text):
                raise RuntimeError(f"{label}/{consumer} map has no actual Rust ABI symbol")
            if not re.search(r"\blibcmt[:.]", map_text, re.I):
                raise RuntimeError(f"{label}/{consumer} map has no static release CRT")
            evidence[consumer] = {"compiles": selected, "policy": policy, "binary": str(build / binary_name), "map": str(map_file)}
        report["builds"][label] = {"configuration": configuration, "consumers": evidence}
    run([args.build.resolve() / "openttd_rust_abi.exe"], "executed-ffi-layout-highbits-owners-locale")
    generated = output / "fresh-generated"
    generated.mkdir(exist_ok=True)
    names = ("strings.h", "english.lng", "french.lng", "settings.h")
    for name in names:
        (generated / name).unlink(missing_ok=True)
    lang = migration.ROOT / "src/lang"
    strgen = args.tools.resolve() / "src/strgen/strgen.exe"
    for name, extra in (("header", []), ("english", [lang / "english.txt"]), ("french", [lang / "french.txt"])):
        run([strgen, "-s", lang, "-d", generated, *extra], f"fresh-strgen-{name}")
    settings = migration.ROOT / "src/table/settings"
    inputs = [settings / name for name in re.findall(r"\$\{CMAKE_CURRENT_SOURCE_DIR\}/(\w+\.ini)", (settings / "CMakeLists.txt").read_text())]
    run([args.tools.resolve() / "src/settingsgen/settingsgen.exe", "-o", generated / "settings.h",
         "-b", migration.ROOT / "src/table/settings.h.preamble", "-a", migration.ROOT / "src/table/settings.h.postamble", *inputs], "fresh-settingsgen")
    if any(not (generated / name).is_file() or not (generated / name).stat().st_size for name in names):
        raise RuntimeError("Fresh current-source generator outputs missing or empty")
    report["passed"] = True
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
