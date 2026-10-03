#!/usr/bin/env python3
"""Compare bounded integer parser/builder gaps against pinned unchanged C++ sources."""

import hashlib
import json
from pathlib import Path
import re
import runpy
import subprocess

ROOT = Path(__file__).resolve().parents[1]
MIGRATION = runpy.run_path(str(ROOT / "tools/migration.py"))
REFERENCE = MIGRATION["REFERENCE"].resolve()
OUT = ROOT / ".local/integer-comparison"


def cases():
    inputs = [b"", b"-", b"+1", b" 1", b"0x", b"-0x", b"0X-1", b"0x-1", b"0x-", b"-0x-1", b"0x+1", b"01", b"0129", b"0y", b"0x0x1", b"42\x00junk", b"x\x00\xff\n", b"0x\x00\xffab", b"999999999999999999999999999999999999999999x\x00\xff\nmore"]
    for width in (8, 16, 32, 64):
        bounds = {0, 1, (1 << (width - 1)) - 1, 1 << (width - 1), (1 << (width - 1)) + 1, (1 << width) - 1, 1 << width, (1 << width) + 1}
        samples = inputs.copy()
        for value in sorted(bounds):
            decimal = str(value).encode()
            hexadecimal = format(value, "x").encode()
            octal = format(value, "o").encode()
            samples.extend((decimal, b"-" + decimal, decimal + b"xyz!tail", b"0x" + hexadecimal, b"-0x" + hexadecimal, hexadecimal, b"-" + hexadecimal, octal, b"-" + octal))
        for signed in (0, 1):
            for base in (0, 8, 10, 16):
                for clamp in (0, 1):
                    for src in dict.fromkeys(samples):
                        yield width, signed, base, clamp, src



def builder_cases():
    for width in (8, 16, 32, 64):
        mask = (1 << width) - 1
        for signed in (0, 1):
            low = -(1 << (width - 1)) if signed else 0
            high = (1 << (width - 1)) - 1 if signed else mask
            values = {low, low + 1, 0, 1, high - 1, high}
            if signed:
                values.add(-1)
            for base in range(2, 37):
                samples = values.copy()
                # Exactly 32 versus 33 bytes, including the minus sign. Only
                # representable inputs are included; reference to_chars is the oracle.
                for exponent in (30, 31, 32):
                    boundary = base ** exponent
                    for value in (boundary - 1, boundary, boundary + 1):
                        if low <= value <= high:
                            samples.add(value)
                        if signed and low <= -value <= high:
                            samples.add(-value)
                for value in sorted(samples):
                    yield width, signed, base, value & mask


def compare_builders(env, binaries):
    corpus = list(builder_cases())
    records = [f"{width} {signed} {base} {bits:x}\n".encode() for width, signed, base, bits in corpus]
    (OUT / "builder-corpus.txt").write_bytes(b"".join(records))
    commands = []
    streams = {}
    for name in ("reference", "candidate", "candidate-cpp"):
        command = [str(binaries[name, False]), "--builder"]
        commands.append(command)
        result = subprocess.run(command, input=b"".join(records), env=env,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
        (OUT / f"{name}-builder.txt").write_bytes(result.stdout)
        streams[name] = result.stdout.splitlines()
    expected = streams["reference"]
    assert len(expected) == len(corpus) + 10, "Builder probe inventory changed"
    for name in ("candidate", "candidate-cpp"):
        failures = [(index, records[index].decode().strip() if index < len(records) else "fixed sink/alias case",
                     want.decode(), got.decode())
                    for index, (want, got) in enumerate(zip(expected, streams[name])) if want != got]
        (OUT / f"{name}-builder-failures.json").write_text(json.dumps(failures, indent=2) + "\n")
        if len(streams[name]) != len(expected) or failures:
            raise RuntimeError(f"Builder comparison failed: {name}; {OUT / f'{name}-builder-failures.json'}")
    return {"cases": len(corpus), "fixed_alias_sink_cases": 10, "commands": commands,
            "output_sha256": hashlib.sha256(b"\n".join(expected) + b"\n").hexdigest()}

def compare_generators(env):
    outputs = {}
    commands = []
    settings_dir = REFERENCE / "src/table/settings"
    settings_inputs = [str(settings_dir / name) for name in re.findall(r"\$\{CMAKE_CURRENT_SOURCE_DIR\}/(\w+\.ini)", (settings_dir / "CMakeLists.txt").read_text())]
    lang = REFERENCE / "src/lang"
    tools = {"reference": (ROOT / "build-reference/src").resolve(), "candidate": ROOT / ".local/build-tools-rust/src"}
    for name, tool_dir in tools.items():
        out = OUT / (name + "-generated")
        out.mkdir(exist_ok=True)
        for filename in ("strings.h", "settings.h", "english.lng", "french.lng"):
            (out / filename).unlink(missing_ok=True)
        strgen = str(tool_dir / "strgen/strgen")
        settingsgen = str(tool_dir / "settingsgen/settingsgen")
        invocations = [
            [strgen, "-s", str(lang), "-d", str(out)],
            [strgen, "-s", str(lang), "-d", str(out), str(lang / "english.txt")],
            [strgen, "-s", str(lang), "-d", str(out), str(lang / "french.txt")],
            [settingsgen, "-o", str(out / "settings.h"), "-b", str(REFERENCE / "src/table/settings.h.preamble"), "-a", str(REFERENCE / "src/table/settings.h.postamble"), *settings_inputs],
        ]
        for index, command in enumerate(invocations):
            commands.append(command)
            with (OUT / f"{name}-generator-{index}.log").open("wb") as log:
                subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
        outputs[name] = {filename: (out / filename).read_bytes() for filename in ("strings.h", "settings.h", "english.lng", "french.lng")}
    assert outputs["reference"] == outputs["candidate"], "Generator output mismatch"
    malformed = []
    original = (lang / "english.txt").read_bytes()
    for index, pragma in enumerate((b"##id 0x-", b"##id 999999999999999999999junk", b"##winlangid -0x80000001", b"##id 0x\x00\xff")):
        directory = OUT / "malformed" / str(index)
        directory.mkdir(parents=True, exist_ok=True)
        (directory / "english.txt").write_bytes(original.replace(b"##id 0x0000", pragma, 1))
        results = []
        for name, tool_dir in tools.items():
            command = [str(tool_dir / "strgen/strgen"), "-s", str(directory), "-d", str(directory)]
            commands.append(command)
            result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            results.append((result.returncode, result.stdout, result.stderr))
        assert results[0] == results[1] and results[0][0] == 2, results
        malformed.append({"pragma_hex": pragma.hex(), "exit_code": results[0][0], "stderr_hex": results[0][2].hex()})
    return {"commands": commands, "generated_sha256": {name: hashlib.sha256(value).hexdigest() for name, value in outputs["reference"].items()}, "malformed_strgen": malformed}


def main():
    MIGRATION["ensure_reference"]()
    OUT.mkdir(parents=True, exist_ok=True)
    env = MIGRATION["environment"]()
    archive = ROOT / ".local/build-tools-rust/cargo/release/libopenttd_kernels.a"
    if not archive.exists():
        raise RuntimeError("Build the native Rust tools first; see migration guide")
    binaries = {}
    for name, source in (("reference", REFERENCE), ("candidate", ROOT), ("candidate-cpp", ROOT)):
        for fatal in ((False,) if name == "candidate-cpp" else (False, True)):
            binary = OUT / (name + ("-fatal" if fatal else ""))
            command = ["g++", "-std=c++20", "-O0", "-ffunction-sections", "-fdata-sections", "-DFMT_HEADER_ONLY", "-I", str(source / "src"), str(ROOT / "tools/integer_probe.cpp"), str(source / "src/core/string_consumer.cpp"), str(source / "src/core/string_builder.cpp"), str(source / "src/core/utf8.cpp"), "-Wl,--gc-sections", "-o", str(binary)]
            if fatal:
                command.append("-DSTRGEN")
            if name == "candidate":
                command.extend(["-DWITH_RUST", str(archive), "-ldl", "-lpthread", "-lm"])
            with (OUT / (binary.name + "-compile.log")).open("w") as output:
                subprocess.run(command, env=env, stdout=output, stderr=subprocess.STDOUT, check=True)
            binaries[name, fatal] = binary
    corpus = list(cases())
    records = [f"{width} {signed} {base} {clamp} {src.hex() or '-'}\n".encode() for width, signed, base, clamp, src in corpus]
    (OUT / "corpus.txt").write_bytes(b"".join(records))
    results = {}
    for name in ("reference", "candidate"):
        result = subprocess.run([str(binaries[name, False])], input=b"".join(records), env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
        (OUT / (name + ".txt")).write_bytes(result.stdout)
        results[name] = result.stdout.splitlines()
    assert len(results["reference"]) == len(corpus)
    assert len(results["candidate"]) == len(corpus)
    failures = [(index, records[index].decode().strip(), expected.decode(), actual.decode()) for index, (expected, actual) in enumerate(zip(results["reference"], results["candidate"])) if expected != actual]
    (OUT / "failures.json").write_text(json.dumps(failures, indent=2) + "\n")
    if failures:
        raise RuntimeError(f"{len(failures)} integer discrepancies; see {OUT / 'failures.json'}")
    fatal_cases = []
    # These have errors in ReadIntegerBase, with prefix-relative and range spans.
    for case in ((8, 1, 0, 0, b"-0xffTAIL"), (8, 1, 0, 0, b"-0x100TAIL"), (32, 1, 0, 0, b"0x-"), (32, 0, 10, 0, b"-1"), (16, 1, 0, 0, b"0x\x00\xff\n"), (64, 0, 10, 0, b"9" * 80 + b"\x00\xff\n!")):
        width, signed, base, clamp, src = case
        record = f"{width} {signed} {base} {clamp} {src.hex()}\n".encode()
        outputs = [subprocess.run([str(binaries[name, True])], input=record, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE) for name in ("reference", "candidate")]
        assert outputs[0].returncode == outputs[1].returncode == 2
        assert outputs[0].stdout == outputs[1].stdout and outputs[0].stderr == outputs[1].stderr
        fatal_cases.append({"input": record.decode().strip(), "exit_code": 2, "stdout": outputs[0].stdout.decode()})
    builders = compare_builders(env, binaries)
    generators = compare_generators(env)
    MIGRATION["ensure_reference"]()
    report = {"baseline": MIGRATION["BASELINE"], "candidate_commit": MIGRATION["git"]("rev-parse", "HEAD"), "candidate_status": MIGRATION["git"]("status", "--porcelain"), "cases": len(corpus), "fatal_cases": fatal_cases, "generators": generators, "builders": builders, "reference_sources_sha256": {name: hashlib.sha256((REFERENCE / name).read_bytes()).hexdigest() for name in ("src/core/string_consumer.hpp", "src/core/string_consumer.cpp", "src/core/string_builder.hpp", "src/core/string_builder.cpp", "src/core/utf8.hpp", "src/core/utf8.cpp")}, "passed": True}
    (OUT / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"Integer comparisons passed: {len(corpus)} API cases, {len(fatal_cases)} fatal diagnostic cases, {builders['cases']} builder formats + {builders['fixed_alias_sink_cases']} fixed sink/alias cases; {OUT / 'report.json'}")


if __name__ == "__main__":
    main()
