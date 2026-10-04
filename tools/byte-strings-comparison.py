#!/usr/bin/env python3
"""Compare concrete byte-string gaps with the pristine native original APIs."""

import difflib
import hashlib
import json
from pathlib import Path
import runpy
import subprocess

ROOT = Path(__file__).resolve().parents[1]
MIGRATION = runpy.run_path(str(ROOT / "tools/migration.py"))
OUT = ROOT / ".local/byte-strings-comparison"


def token(value):
    return value.hex() or "-"


def corpus():
    yield "null -"
    values = [
        b"",
        b"a",
        b"A",
        b"aa",
        b"aA",
        b"b",
        b"ababa",
        b"aba",
        b"abab",
        b"BAB",
        b"absent",
        b"abc",
        b"c",
        b"\x00",
        b"a\x00B",
        b"A\x00b",
        b"\x00b",
        b"x\x00a\x00Bz",
    ]
    for left in values:
        for right in values:
            yield f"pair {token(left)} {token(right)}"
    for byte in range(256):
        value = bytes([byte])
        for right in (b"\x00", b"A", b"\x7f", b"\xff", value):
            yield f"pair {token(value)} {token(right)}"
        for offset in (0, 1, 2, 3):
            yield f"lower {token(b'A' + value + b'Z')} {offset}"
        yield f"encode {token(value)}"
        for pair in (b"1" + value, value + b"1"):
            yield f"decode {token(pair)} 1"
    for value in (b"", b"lower", b"UPPER", b"Aa\x00Z", bytes(range(256))):
        yield f"encode {token(value)}"
        for offset in dict.fromkeys((0, len(value) // 2, len(value))):
            yield f"lower {token(value)} {offset}"
    for value in (
        b"",
        b"1",
        b"12",
        b"123",
        b"1234",
        b"12g4",
        b"123g",
        b"g234",
        b"1g34",
        b"123456789abcdef0",
        b"123456789ABCDEF0",
        b"123456g8",
        b"12\x0034",
        b"12\xff4",
    ):
        for length in range(6):
            yield f"decode {token(value)} {length}"
    all_hex = b"".join(f"{byte:02X}".encode() for byte in range(256))
    yield f"decode {token(all_hex)} 256"
    for value in (b"12345678", b"1234g678", b"1234567g", b"g2345678", b"1234567"):
        backing = b"\xa5\xa5" + value + b"\xa5" * 12
        for offset in range(11):
            for length in (len(value) // 2, len(value) // 2 + 1):
                yield f"overlap {token(backing)} 2 {len(value)} {offset} {length}"
    values = [
        b"",
        b" ",
        b" \t\v\f\r ",
        b"\n",
        b" \n ",
        b"  a  b  ",
        b"\x00",
        b"\x00a\x00",
        b"\xff\x00a\xff",
        b"\xff\xff",
        b"aaaa",
        b"xabxx",
    ]
    sets = [
        b"",
        b" \t\v\f\r",
        b"\x00",
        b"\xff",
        b"\xff\x00",
        b"a",
        b"x",
        bytes(range(256)),
    ]
    for value in values:
        yield f"inplace {token(value)}"
        for set_ in sets:
            yield f"trim {token(value)} {token(set_)}"


def main():
    MIGRATION["ensure_reference"]()
    reference = MIGRATION["REFERENCE"].resolve()
    # #22 owns this validated CMake-cache archive locator for every probe.
    archive = MIGRATION["rust_archive"](ROOT / "build-rust")
    env = MIGRATION["environment"]()
    OUT.mkdir(parents=True, exist_ok=True)
    records = list(dict.fromkeys(corpus()))
    data = ("\n".join(records) + "\n").encode()
    (OUT / "corpus.txt").write_bytes(data)
    locales = subprocess.check_output(["locale", "-a"], env=env, text=True).splitlines()
    if "C" not in locales:
        raise RuntimeError("Native C locale is unavailable")
    report = {
        "baseline": MIGRATION["BASELINE"],
        "candidate_commit": MIGRATION["git"]("rev-parse", "HEAD"),
        "candidate_status": MIGRATION["git"](
            "status", "--porcelain", "--untracked-files=all"
        ),
        "cases": len(records),
        "archive": str(archive),
        "locales": locales,
        "commands": [],
        "reference_sources_sha256": {},
        "results": {},
        "passed": False,
    }
    for name in (
        "src/string.cpp",
        "src/string_func.h",
        "src/core/string_consumer.cpp",
        "src/tests/string_func.cpp",
    ):
        report["reference_sources_sha256"][name] = hashlib.sha256(
            (reference / name).read_bytes()
        ).hexdigest()
    outputs = {}

    def run(command, name, input_=None, extra_env=None):
        result = subprocess.run(
            command,
            cwd=ROOT,
            env=extra_env or env,
            input=input_,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        (OUT / f"{name}.stdout").write_bytes(result.stdout)
        (OUT / f"{name}.stderr").write_bytes(result.stderr)
        report["commands"].append(
            {
                "name": name,
                "argv": list(map(str, command)),
                "exit_code": result.returncode,
            }
        )
        if result.returncode:
            raise RuntimeError(f"{name} failed; see {OUT / (name + '.stderr')}")
        return result.stdout

    try:
        for signedness, flags in (
            ("native", []),
            ("unsigned-char", ["-funsigned-char"]),
        ):
            for label, source in (
                ("reference", reference),
                ("candidate", ROOT),
                ("candidate-cpp", ROOT),
            ):
                binary = OUT / f"{label}-{signedness}"
                command = [
                    "c++",
                    "-std=c++20",
                    "-O2",
                    "-DFMT_HEADER_ONLY",
                    "-ffunction-sections",
                    "-fdata-sections",
                    "-I",
                    str(source / "src"),
                    str(ROOT / "tools/migration/byte-strings-comparison.cpp"),
                    str(source / "src/string.cpp"),
                    str(source / "src/core/string_consumer.cpp"),
                    "-Wl,--gc-sections",
                    "-o",
                    str(binary),
                    *flags,
                ]
                if label == "candidate":
                    command.extend(
                        [
                            "-DWITH_RUST",
                            str(archive),
                            *MIGRATION["rust_configuration"](ROOT / "build-rust")[
                                "native_libraries"
                            ],
                        ]
                    )
                run(command, f"compile-{binary.name}")
                for index, locale in enumerate(locales):
                    name = f"{binary.name}-locale-{index}"
                    output = run([str(binary), locale], name, data)
                    outputs[label, signedness, locale] = output
                    if (
                        label != "reference"
                        and output != outputs["reference", signedness, locale]
                    ):
                        diff = "".join(
                            difflib.unified_diff(
                                outputs["reference", signedness, locale]
                                .decode()
                                .splitlines(True),
                                output.decode().splitlines(True),
                                fromfile="reference",
                                tofile=label,
                            )
                        )
                        (OUT / f"{name}.diff").write_text(diff)
                        raise RuntimeError(
                            f"Byte-string mismatch: {OUT / (name + '.diff')}"
                        )
                    report["results"][name] = {
                        "stdout_sha256": hashlib.sha256(output).hexdigest(),
                        "lines": len(output.splitlines()),
                        "wide_length_result": output.splitlines()[1].decode(),
                    }
        report["locale_mapping_differs_from_C"] = {
            locale: any(
                outputs["reference", sign, locale].split(b"\n", 1)[1]
                != outputs["reference", sign, "C"].split(b"\n", 1)[1]
                for sign in ("native", "unsigned-char")
            )
            for locale in locales
        }
        sanitizer = OUT / "candidate-sanitized"
        command = [
            "c++",
            "-std=c++20",
            "-O1",
            "-DFMT_HEADER_ONLY",
            "-ffunction-sections",
            "-fdata-sections",
            "-I",
            str(ROOT / "src"),
            str(ROOT / "tools/migration/byte-strings-comparison.cpp"),
            str(ROOT / "src/string.cpp"),
            str(ROOT / "src/core/string_consumer.cpp"),
            str(ROOT / "src/core/string_builder.cpp"),
            str(ROOT / "src/core/string_inplace.cpp"),
            str(ROOT / "src/core/utf8.cpp"),
            "-Wl,--gc-sections",
            "-DWITH_RUST",
            str(archive),
            *MIGRATION["rust_configuration"](ROOT / "build-rust")["native_libraries"],
            "-fsanitize=address,undefined",
            "-fno-omit-frame-pointer",
            "-no-pie",
            "-o",
            str(sanitizer),
        ]
        run(command, "compile-sanitized")
        sanitizer_env = env | {
            "ASAN_OPTIONS": "detect_leaks=1:halt_on_error=1",
            "UBSAN_OPTIONS": "halt_on_error=1",
        }
        actual = run([str(sanitizer), "C"], "sanitized-C", data, sanitizer_env)
        if actual != outputs["reference", "native", "C"]:
            raise RuntimeError("Sanitized byte-string corpus output differs")
        report["sanitizer_scope"] = {
            "cpp_fixture_and_facade_instrumented": True,
            "rust_memory_accesses_instrumented": False,
            "passed": True,
        }
        if MIGRATION["git"](
            "status", "--porcelain", "--untracked-files=all", cwd=reference
        ):
            raise RuntimeError("Reference became dirty during byte-string comparison")
        report["passed"] = True
    finally:
        (OUT / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        f"Byte-string comparison passed: {len(records)} cases, {len(locales)} locales, native/unsigned char; {OUT / 'report.json'}"
    )


if __name__ == "__main__":
    main()
