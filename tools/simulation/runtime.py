"""Export frozen game runtimes and once-built scenario adapters for CI workers."""

import hashlib
import json
import shutil
import struct
import subprocess
from pathlib import Path

from build_identity import IDENTITY_NAME

from . import core

MANIFEST = "runtime-manifest.json"


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def elf_content(path):
    """Hash loadable bytes and allocated object sections, including layout.

    Only ELF header section-table location/count/name-index bytes are normalized:
    stripping debug sections changes these non-loader metadata fields. Program
    headers, all other mapped bytes and every allocated section remain exact.
    """
    data = Path(path).read_bytes()
    if data[:4] != b"\x7fELF" or data[4] not in (1, 2) or data[5] not in (1, 2):
        raise RuntimeError("Native export is not a supported ELF file")
    endian = "<" if data[5] == 1 else ">"
    wide = data[4] == 2
    header = struct.unpack_from(
        endian + ("HHIQQQIHHHHHH" if wide else "HHIIIIIHHHHHH"), data, 16
    )
    phoff, shoff, phsize, phcount, shsize, shcount, names_index = (
        header[4],
        header[5],
        header[8],
        header[9],
        header[10],
        header[11],
        header[12],
    )
    load = []
    normalized = bytearray(data)
    for start, end in ((40, 48), (60, 64)) if wide else ((32, 36), (48, 52)):
        normalized[start:end] = bytes(end - start)
    for index in range(phcount):
        values = struct.unpack_from(
            endian + ("IIQQQQQQ" if wide else "IIIIIIII"), data, phoff + index * phsize
        )
        if values[0] != 1:  # PT_LOAD
            continue
        offset, size = (values[2], values[5]) if wide else (values[1], values[4])
        load.append(
            {
                "header": list(values),
                "sha256": hashlib.sha256(
                    normalized[offset : offset + size]
                ).hexdigest(),
            }
        )
    sections = [
        struct.unpack_from(
            endian + ("IIQQQQIIQQ" if wide else "IIIIIIIIII"),
            data,
            shoff + index * shsize,
        )
        for index in range(shcount)
    ]
    names = sections[names_index]
    strings = data[names[4] : names[4] + names[5]]
    allocated = []
    for section in sections:
        if not section[2] & 2:  # SHF_ALLOC, also checks relocatable widget objects.
            continue
        name = strings[section[0] :].split(b"\0", 1)[0].decode()
        body = b"" if section[1] == 8 else data[section[4] : section[4] + section[5]]
        allocated.append(
            {
                "name": name,
                "type": section[1],
                "flags": section[2],
                "address": section[3],
                "size": section[5],
                "alignment": section[8],
                "sha256": hashlib.sha256(body).hexdigest(),
            }
        )
    return {
        "PT_LOAD_without_section_table_header_metadata": load,
        "SHF_ALLOC": allocated,
    }


def strip_debug(source, destination=None):
    """Strip debug data from an exported copy, proving loaded code/data is exact."""
    before = digest(source)
    proof = elf_content(source)
    command = ["objcopy", "--strip-debug", str(source)]
    if destination is not None:
        command.append(str(destination))
    subprocess.run(command, check=True)
    target = destination or source
    if elf_content(target) != proof:
        raise RuntimeError("Debug stripping changed loadable code/data or layout")
    return {
        "source_sha256": before,
        "sha256": digest(target),
        "command": command,
        "content_proof": proof,
    }


def export(destination, migration):
    """Capture under build locks; adapters must come from these exact game objects."""
    from . import fleet, orders

    destination.mkdir(parents=True)
    records = {}
    builds = {
        "reference": migration.REFERENCE_BUILD.resolve(),
        "candidate": core.ROOT / "build-rust",
    }
    for role, build in builds.items():
        lock = (
            migration.reference_lock(shared=True)
            if role == "reference"
            else migration.candidate_lock(build, shared=True)
        )
        with lock:
            source = build / ("openttd" if role == "reference" else "openttd-rust")
            runtime = destination / f"{role}-runtime"
            binary = core._copy_runtime(build, source, runtime)
            if role == "candidate":
                shutil.copy2(build / IDENTITY_NAME, runtime / IDENTITY_NAME)
            ordinary_source_sha256 = digest(binary)
            adapters = {}
            derivations = {}
            for module in (fleet, orders):
                folder = runtime / module.__name__.rsplit(".", 1)[1]
                executable, receipt = module.adapter(
                    build, folder, migration.environment()
                )
                if receipt["ordinary_binary_sha256"] != digest(binary):
                    raise RuntimeError("Exported adapter does not match frozen game")
                for object_file in folder.glob("*.o"):
                    object_file.unlink()
                derivation = strip_debug(executable)
                receipt["source_binary_sha256"] = receipt["binary_sha256"]
                receipt["binary_sha256"] = derivation["sha256"]
                derivations[folder.name] = derivation
                adapters[folder.name] = receipt
            ordinary = strip_debug(binary)
            for receipt in adapters.values():
                receipt["source_ordinary_binary_sha256"] = ordinary_source_sha256
                receipt["ordinary_binary_sha256"] = ordinary["sha256"]
            (runtime / "adapters.json").write_text(
                json.dumps(adapters, indent=2) + "\n"
            )
        records[role] = {
            "binary": str(binary.relative_to(destination)),
            "ordinary_derivation": ordinary,
            "adapter_derivations": derivations,
        }
    files = {
        str(path.relative_to(destination)): digest(path)
        for path in sorted(destination.rglob("*"))
        if path.is_file()
    }
    identity = json.loads(
        (destination / "candidate-runtime" / IDENTITY_NAME).read_text()
    )
    manifest = {
        "schema_version": 1,
        "candidate_commit": migration.git("rev-parse", "HEAD"),
        "candidate_build_identity": identity,
        "baseline": migration.BASELINE,
        "roles": records,
        "files": files,
    }
    (destination / MANIFEST).write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


def verify(destination, migration):
    manifest = json.loads((destination / MANIFEST).read_text())
    if manifest["schema_version"] != 1 or manifest["baseline"] != migration.BASELINE:
        raise RuntimeError("Frozen runtime baseline/schema differs")
    if manifest["candidate_commit"] != migration.git("rev-parse", "HEAD"):
        raise RuntimeError("Frozen runtime belongs to another candidate commit")
    identity = manifest["candidate_build_identity"]
    if (
        identity["source_commit"] != manifest["candidate_commit"]
        or not identity["source_stable"]
    ):
        raise RuntimeError(
            "Frozen candidate source was not stable at the captured commit"
        )
    expected = manifest["files"]
    actual = {
        str(path.relative_to(destination)): digest(path)
        for path in sorted(destination.rglob("*"))
        if path.is_file()
        and path not in (destination / MANIFEST, destination / "shard-plan.json")
    }
    if actual != expected:
        raise RuntimeError("Frozen runtime files differ from the build manifest")
    return manifest


def prepared_adapter(kind, role, binaries, builds, folder):
    """Use a bundle adapter, preserving the original compile/link provenance."""
    runtime = builds[role]
    receipt_file = runtime / "adapters.json"
    if not receipt_file.is_file():
        return None
    receipt = json.loads(receipt_file.read_text())[kind]
    source = runtime / kind / f"openttd-{kind}"
    if receipt["binary_sha256"] != digest(source) or receipt[
        "ordinary_binary_sha256"
    ] != digest(binaries[role]):
        raise RuntimeError(
            "Prepared adapter differs from frozen game or adapter manifest"
        )
    folder.mkdir(parents=True, exist_ok=True)
    binary = folder / source.name
    shutil.copy2(source, binary)
    return binary, receipt
