#!/usr/bin/env python3
"""Compare actual original/Rust X25519 endpoints, wire bytes and state transitions."""

from collections import Counter
import hashlib
import json
from pathlib import Path
import runpy
import subprocess

ROOT = Path(__file__).resolve().parents[1]
MIGRATION = runpy.run_path(str(ROOT / "tools/migration.py"))
REFERENCE = MIGRATION["REFERENCE"].resolve()
OUT = ROOT / ".local/auth-comparison"
SECRET_SERVER = bytes(range(32)).hex()
SECRET_CLIENT = bytes(range(32, 64)).hex()


def hex_bytes(value):
    return value.hex() or "-"


def wire(payload):
    return ((len(payload) + 3).to_bytes(2, "little") + b"\x00" + payload).hex()


class Endpoint:
    def __init__(self, binary, role, transcript, env):
        self.role, self.transcript = role, transcript
        self.process = subprocess.Popen([str(binary)], env=env, stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    def call(self, command):
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        result = self.process.stdout.readline().rstrip("\n")
        if not result or result.startswith("error "):
            raise RuntimeError(f"{self.role} command {command}: {result}")
        self.transcript.append([self.role, command, result])
        return result

    def close(self):
        self.process.stdin.close()
        error = self.process.stderr.read()
        code = self.process.wait()
        if code or error:
            raise RuntimeError(f"{self.role} endpoint exited {code}: {error}")


def scenario(server_binary, client_binary, env, extra_server, extra_client, mode):
    transcript = []
    server = Endpoint(server_binary, "server", transcript, env)
    client = Endpoint(client_binary, "client", transcript, env)
    try:
        server.call(f"new {SECRET_SERVER} 73")
        client.call(f"new {SECRET_CLIENT} 191")
        request = server.call("request").split()[1]
        assert client.call(f"recv-request {request}") == "1 0"
        response = client.call(f"response {hex_bytes(extra_client)}").split()[1]
        result = server.call(f"recv-response {response} {hex_bytes(extra_server)}")
        success = extra_server == extra_client
        assert result == ("0 0" if success else "1 0")
        server.call("state"); client.call("state")
        if mode == "failures":
            # Covered after a valid exchange so prior peer/key preservation is defined.
            for payload in (b"", bytes(55), bytes(57)):
                server.call(f"recv-response {wire(payload)} {hex_bytes(extra_server)}")
                server.call("state")
                client.call(f"recv-request {wire(payload)}")
                client.call("state")
            for point in (3, 35, 51, len(bytes.fromhex(response)) - 1):
                damaged = bytearray.fromhex(response); damaged[point] ^= 1
                server.call(f"recv-response {damaged.hex()} {hex_bytes(extra_server)}")
                server.call("state")
                server.call(f"recv-response {response} {hex_bytes(extra_server)}")
            for low_order in (bytes(32), b"\x01" + bytes(31)):
                bad_request = bytearray.fromhex(request); bad_request[3:35] = low_order
                client.call(f"recv-request {bad_request.hex()}")
                assert client.call(f"response {hex_bytes(extra_client)}").split()[0] == "0"
                client.call("state")
                bad_response = bytearray.fromhex(response); bad_response[3:35] = low_order
                server.call(f"recv-response {bad_response.hex()} {hex_bytes(extra_server)}")
                server.call("state")
            client.call(f"recv-request {request}")
            response = client.call(f"response {hex_bytes(extra_client)}").split()[1]
            assert server.call(f"recv-response {response} {hex_bytes(extra_server)}") == "0 0"
            # Nonzero short nonce input is undefined in pinned Packet::Recv_bytes (#41).
            before = client.call("nonce")
            assert client.call(f"recv-nonce {wire(b'')}") == "0 0"
            assert client.call("nonce") == before
        nonce = server.call("nonce").split()[1]
        assert client.call(f"recv-nonce {nonce}") == "1 0"
        if mode == "failures":
            nonce_payload = bytes.fromhex(nonce)[3:]
            assert client.call(f"recv-nonce {wire(nonce_payload + b'xyz')}") == "1 3"
        if success:
            for side in (0, 1):
                sender, receiver = (client, server) if side == 0 else (server, client)
                sender.call(f"init-stream {side}"); receiver.call(f"init-stream {side}")
                for length in (0, 1, 8, 15, 16, 17, 63, 64, 65, 127, 256):
                    plaintext = bytes((i * 17 + length) & 255 for i in range(length))
                    encrypted = sender.call(f"encrypt {hex_bytes(plaintext)}").split()
                    mac, ciphertext = encrypted[1:3]
                    if mode == "failures":
                        tampered = bytearray.fromhex(mac); tampered[0] ^= 128
                        failed = receiver.call(f"decrypt {ciphertext} {tampered.hex()}").split()
                        assert failed[0] == "0" and failed[2] == ciphertext
                    if mode == "failures" and length:
                        damaged = bytearray.fromhex(ciphertext); damaged[-1] ^= 1
                        assert receiver.call(f"decrypt {damaged.hex()} {mac}").split()[0] == "0"
                    decrypted = receiver.call(f"decrypt {ciphertext} {mac}").split()
                    assert decrypted[0] == "1" and decrypted[2] == hex_bytes(plaintext)
                    assert decrypted[3:] == encrypted[3:] and decrypted[4] == "0"
                    if mode == "failures":
                        assert receiver.call(f"decrypt {ciphertext} {mac}").split()[0] == "0"
                sender.call("copy-stream")
                first = sender.call("encrypt 010203")
                sender.call("swap-stream")
                assert sender.call("encrypt 010203") == first
                sender.call("assign-stream"); sender.call("swap-stream")
        for endpoint in (server, client):
            endpoint.call("copy"); assert endpoint.call("move-copy") == "1"
            endpoint.call("assign")
        # Separate public derived-key facade: exact halves, rejected peer preservation, copies.
        server_pub = bytes.fromhex(request)[3:35].hex()
        client.call(f"derive {SECRET_CLIENT} {server_pub} 0 {hex_bytes(extra_client)}")
        client.call("copy-keys")
        client.call(f"derive {SECRET_CLIENT} {'00' * 32} 0 {hex_bytes(extra_client)}")
        client.call("copy-keys")
        alias = client.call(f"alias-keys {SECRET_CLIENT} {server_pub}").split()
        assert alias[0:2] == ["1", "1"] and alias[3] == "1"
        assert alias[2] != alias[4]
        for operation in ("packet", "log", "output"):
            assert server.call(f"throw-operations {operation}") == "1 1 1"
            assert client.call(f"throw-operations {operation}") == "1 1 1"
        for after in (0, 1):
            assert server.call(f"throw-construction {SECRET_SERVER} {after}") == "1 1"
            assert client.call(f"throw-construction {SECRET_CLIENT} {after}") == "1 1"
        assert server.call("drop").split()[0] == "1"
        assert client.call("drop").split()[0] == "1"
    finally:
        server.close(); client.close()
    return transcript


def curve_interoperability(binaries, env, name):
    transcript = []
    endpoints = {label: Endpoint(binary, label, transcript, env) for label, binary in binaries.items()}
    cases = 0
    try:
        seed = bytes((i * 37 + 17) & 255 for i in range(32)).hex()
        for variant in (0, 1, 2):
            for size in (0, 1, 127, 128, 129):
                message = bytes((i * 37 + 91) & 255 for i in range(size))
                signed = {label: endpoint.call(f"curve-sign {variant} {seed} {hex_bytes(message)}").split()
                          for label, endpoint in endpoints.items()}
                expected = signed["reference"]
                if any(value != expected for value in signed.values()) or expected[2] != "00" * 32:
                    raise RuntimeError(f"Curve signing/seed wipe discrepancy: {name}, {variant}, {size}")
                for signer, checker in (("reference", "candidate"), ("candidate", "reference"),
                                        ("reference", "candidate-cpp"), ("candidate-cpp", "reference")):
                    public_key, signature, _ = signed[signer]
                    result = endpoints[checker].call(f"curve-check {variant} {signature} {public_key} {hex_bytes(message)}")
                    bad = bytearray.fromhex(signature); bad[3] ^= 0x40
                    failed = endpoints[checker].call(f"curve-check {variant} {bad.hex()} {public_key} {hex_bytes(message)}")
                    if result != "0" or failed != "-1":
                        raise RuntimeError(f"Mixed curve signature discrepancy: {name}, {signer}, {checker}, {variant}, {size}")
                cases += 1
    finally:
        for endpoint in endpoints.values():
            endpoint.close()
    path = OUT / f"curve-interop-{name}.json"
    path.write_text(json.dumps(transcript, indent=2) + "\n")
    return {"cases": cases, "endpoint_records": len(transcript), "sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "passed": True}


def main():
    MIGRATION["ensure_reference"]()
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "report.json").unlink(missing_ok=True)
    env = MIGRATION["environment"]()
    archive = MIGRATION["rust_archive"](ROOT / "build-rust")
    fixture = ROOT / "tools/migration/auth-comparison.cpp"
    commands, binaries, ladder_shims = [], {}, {}
    sources = ("src/network/network_crypto.cpp", "src/network/network_crypto_internal.h",
               "src/network/core/packet.cpp", "src/3rdparty/monocypher/monocypher.cpp",
               "src/3rdparty/monocypher/monocypher.h", "src/3rdparty/monocypher/monocypher-ed25519.cpp", "src/3rdparty/monocypher/monocypher-ed25519.h", "src/string.cpp", "src/core/string_builder.cpp", "src/core/string_inplace.cpp", "src/core/utf8.cpp")
    for label, source in (("reference", REFERENCE), ("candidate", ROOT), ("candidate-cpp", ROOT)):
        binary = OUT / label
        # Include unchanged actual vendor source and expose only its private
        # coarse helper. No copied field/ladder oracle or altered vendor body.
        shim = OUT / f"{label}-ladder.cpp"
        shim.write_text(f'#include "{source / "src/3rdparty/monocypher/monocypher.cpp"}"\n'
                        '''extern "C" void FixtureLadder(uint8_t *out, const uint8_t *scalar, const uint8_t *point, int32_t bits) {
#ifdef WITH_RUST
    openttd_rust_x25519_ladder(&rust_x25519_leaves, out, scalar, point, bits);
#else
    scalarmult(out, scalar, point, bits);
#endif
}
''')
        ladder_shims[label] = {"path": str(shim), "sha256": hashlib.sha256(shim.read_bytes()).hexdigest()}
        command = ["g++", "-std=c++20", "-O2", "-DUNIX", "-DFMT_HEADER_ONLY", "-ffunction-sections", "-fdata-sections",
                   "-I", str(source / "src"), str(fixture),
                   *[str(shim if name == "src/3rdparty/monocypher/monocypher.cpp" else source / name)
                     for name in sources if name.endswith(".cpp") and name != "src/network/network_crypto.cpp"],
                   "-Wl,--gc-sections", "-o", str(binary)]
        if label == "candidate":
            command.extend(["-DWITH_RUST", str(archive), "-ldl", "-lpthread", "-lm"])
        commands.append(command)
        with (OUT / f"{label}-compile.log").open("wb") as log:
            subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
        binaries[label] = binary
    scenarios = [(b"", b"", "normal"), (b"password", b"password", "normal"),
                 (b"a\x00b\xff", b"a\x00b\xff", "failures"), (b"password", b"wrong", "normal"),
                 (b"password", b"", "normal")]
    comparisons, counts, hashes = 0, Counter(), {}
    for index, (server_extra, client_extra, mode) in enumerate(scenarios):
        expected = scenario(binaries["reference"], binaries["reference"], env, server_extra, client_extra, mode)
        counts.update(row[1].split()[0] for row in expected)
        for server_label, client_label in (("reference", "candidate"), ("candidate", "reference"),
                                          ("candidate", "candidate"), ("candidate-cpp", "candidate-cpp")):
            name = f"scenario-{index}-{server_label}-{client_label}"
            actual = scenario(binaries[server_label], binaries[client_label], env, server_extra, client_extra, mode)
            (OUT / f"{name}.json").write_text(json.dumps({"expected": expected, "actual": actual}, indent=2) + "\n")
            if actual != expected:
                raise RuntimeError(f"Mixed authentication discrepancy: {name}; see retained transcript")
            comparisons += len(actual)
            hashes[name] = hashlib.sha256(json.dumps(actual).encode()).hexdigest()
    # Only uncovered primitive variants/alias/buffer/padding/state cases. Expected
    # bytes always come from the actual pinned vendor binary, never from Rust.
    primitive_outputs = {}
    for label in ("reference", "candidate", "candidate-cpp"):
        result = subprocess.run([str(binaries[label]), "--primitives"], env=env,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (OUT / f"{label}-primitives.out").write_bytes(result.stdout)
        (OUT / f"{label}-primitives.err").write_bytes(result.stderr)
        if result.returncode or result.stderr:
            raise RuntimeError(f"{label} direct primitives failed; see retained output")
        primitive_outputs[label] = result.stdout
        if label != "reference" and result.stdout != primitive_outputs["reference"]:
            expected, actual = primitive_outputs["reference"].splitlines(), result.stdout.splitlines()
            failures = [{"line": i + 1,
                         "expected": expected[i].decode() if i < len(expected) else "<missing>",
                         "actual": actual[i].decode() if i < len(actual) else "<missing>"}
                        for i in range(max(len(expected), len(actual)))
                        if (expected[i] if i < len(expected) else None) != (actual[i] if i < len(actual) else None)]
            (OUT / "primitive-failures.json").write_text(json.dumps(failures, indent=2) + "\n")
            raise RuntimeError(f"{label} primitive discrepancies: {len(failures)}")
    curve_interop = curve_interoperability(binaries, env, "native")
    # Instrument the complete C++ fixture, Packet, vendor and facade boundary;
    # stable Rust allocations are leak-observable, Rust accesses are not ASan-instrumented.
    sanitizer = next(list(command) for command in commands if "-DWITH_RUST" in command)
    sanitizer[sanitizer.index("-o") + 1] = str(OUT / "candidate-sanitized")
    sanitizer.extend(["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g", "-no-pie"])
    commands.append(sanitizer)
    with (OUT / "candidate-sanitized-compile.log").open("wb") as log:
        subprocess.run(sanitizer, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
    sanitized_env = env.copy()
    sanitized_env["ASAN_OPTIONS"] = "detect_leaks=1:abort_on_error=1"
    sanitized_env["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
    for server_label, client_label in (("reference", "candidate-sanitized"), ("candidate-sanitized", "reference")):
        server_binary = binaries[server_label] if server_label == "reference" else OUT / server_label
        client_binary = binaries[client_label] if client_label == "reference" else OUT / client_label
        actual = scenario(server_binary, client_binary, sanitized_env, b"a\x00b\xff", b"a\x00b\xff", "failures")
        expected = scenario(binaries["reference"], binaries["reference"], env, b"a\x00b\xff", b"a\x00b\xff", "failures")
        if actual != expected:
            raise RuntimeError("Sanitizer output changed")
    result = subprocess.run([str(OUT / "candidate-sanitized"), "--primitives"], env=sanitized_env,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    (OUT / "candidate-sanitized-primitives.out").write_bytes(result.stdout)
    (OUT / "candidate-sanitized-primitives.err").write_bytes(result.stderr)
    if result.returncode or result.stderr or result.stdout != primitive_outputs["reference"]:
        raise RuntimeError("Direct primitive C++ boundary sanitizer failed; see retained output")
    sanitized_binaries = dict(binaries); sanitized_binaries["candidate"] = OUT / "candidate-sanitized"
    curve_interop["cpp_boundary_sanitizers"] = curve_interoperability(sanitized_binaries, sanitized_env, "sanitized")
    primitives = {"records": len(primitive_outputs["reference"].splitlines()),
                  "record_counts": dict(Counter(row.split()[0].decode() for row in primitive_outputs["reference"].splitlines())),
                  "output_sha256": {label: hashlib.sha256(output).hexdigest() for label, output in primitive_outputs.items()},
                  "all_context_bytes_prefilled": True, "cpp_boundary_sanitizers_passed": True, "passed": True}
    primitives["blake2b_records"] = sum(count for kind, count in primitives["record_counts"].items() if kind.startswith("blake-"))
    primitives["x25519_records"] = sum(count for kind, count in primitives["record_counts"].items() if kind.startswith("x25519-"))
    primitives["curve_records"] = sum(count for kind, count in primitives["record_counts"].items() if kind.startswith("curve-"))
    primitives["prior_cipher_mac_records"] = primitives["records"] - primitives["blake2b_records"] - primitives["x25519_records"] - primitives["curve_records"]
    MIGRATION["ensure_reference"]()
    report = {"baseline": MIGRATION["BASELINE"], "candidate_commit": MIGRATION["git"]("rev-parse", "HEAD"),
              "candidate_status": MIGRATION["git"]("status", "--porcelain"), "rust_archive": str(archive),
              "commands": commands, "ladder_shims": ladder_shims, "curve_interop": curve_interop, "scenario_count": len(scenarios), "compared_endpoint_records": comparisons, "primitives": primitives,
              "original_record_counts": dict(counts), "transcript_sha256": hashes,
              "reference_source_sha256": {name: hashlib.sha256((REFERENCE / name).read_bytes()).hexdigest() for name in sources},
              "fixture_sha256": hashlib.sha256(fixture.read_bytes()).hexdigest(),
              "sanitizer_scope": {"cpp_vendor_packet_fixture_and_facade": True, "rust_accesses_instrumented": False, "rust_allocation_leaks_checked": True, "passed": True},
              "limits": ["Prescribed entropy verifies call order/bytes, not operating-system RNG quality",
                         "Nonempty-short enable nonce is undefined in pinned Packet and excluded (#41)",
                         "Observed outer session/shared/hash wipes are supplemented by prefilled Poly1305/BLAKE2b final context checks; compiler spills are not fully observable",
                         "Protocol primitives are real bundled Monocypher, not mock crypto"], "passed": True}
    (OUT / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"Authentication comparisons passed: {comparisons} mixed endpoint records + {primitives['records']} bounded primitive records; {OUT / 'report.json'}")


if __name__ == "__main__":
    main()
