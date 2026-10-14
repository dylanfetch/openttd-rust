#!/usr/bin/env python3
"""Pack compiler files into one measured, portable payload; never cache results."""

import argparse
import gzip
import hashlib
import io
import json
import os
import re
import shutil
import subprocess
import tarfile
from pathlib import Path

import cache_zstd

ROOT = Path(__file__).resolve().parents[1]
POLICY = json.loads((ROOT / "migration/cache-policy.json").read_text())
MIB = 1024**2
FIT_RETRIES = 3


class PayloadTooLarge(ValueError):
    def __init__(self, size, limit, family):
        super().__init__(
            f"Measured payload envelope {size} exceeds {family} cap {limit}"
        )
        self.size = size
        self.limit = limit


def payload_path(family):
    if family not in POLICY["families"]:
        raise ValueError(f"Unknown cache family: {family}")
    return Path(f".local/cache-payload/{family}.tgz")


def envelope(size):
    # Early packing estimate: reserve 1% plus 1 MiB for the outer archive.
    # cache_zstd independently caps the ACTUAL SDK-compressed tar before upload,
    # including all tar metadata and compression framing; this is not the bound.
    return size + (size + 99) // 100 + MIB


def digest(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def metadata(path):
    with tarfile.open(path, "r:gz") as archive:
        members = archive.getmembers()
        headers = [member for member in members if member.name == "manifest.json"]
        if len(headers) != 1 or not headers[0].isfile() or headers[0].size > 16384:
            raise ValueError("Missing canonical cache manifest")
        manifest = json.load(archive.extractfile(headers[0]))
        roots = manifest.get("roots")
        if (
            manifest.get("schema") != 1
            or manifest.get("family") not in POLICY["families"]
            or not isinstance(manifest.get("compatibility"), str)
            or not re.fullmatch(r"[0-9a-f]{40}", manifest.get("sha", ""))
            or type(roots) is not int
            or roots != (2 if manifest["family"] == "native" else 1)
        ):
            raise ValueError("Invalid protected payload manifest")
        content = hashlib.sha256()
        seen = set()
        for member in sorted(members, key=lambda member: member.name):
            if member.name == "manifest.json":
                continue
            parts = Path(member.name).parts
            index = next(
                (i for i in range(roots) if parts and parts[0] == f"root{i}"), None
            )
            if (
                index is None
                or ".." in parts
                or Path(member.name).is_absolute()
                or member.name in seen
            ):
                raise ValueError(f"Invalid payload member: {member.name}")
            seen.add(member.name)
            if member.isfile():
                with archive.extractfile(member) as source:
                    digest_value = hashlib.file_digest(source, "sha256").digest()
                content.update(f"{index}/{Path(*parts[1:]).as_posix()}\0".encode())
                content.update(digest_value)
            elif not member.isdir():
                raise ValueError(f"Unsupported payload member: {member.name}")
        if manifest.get("content_hash") != content.hexdigest():
            raise ValueError("Compiler payload content hash does not match manifest")
    return dict(manifest, hash=digest(path), bytes=path.stat().st_size)


def pack(path, paths, family, compatibility, sha, legacy=None):
    content = hashlib.sha256()
    for index, root in enumerate(paths):
        if not root.is_dir():
            raise ValueError(f"Missing compiler cache directory: {root}")
        for child in sorted(
            root.rglob("*"), key=lambda child: child.relative_to(root).as_posix()
        ):
            if child.is_symlink() or not (child.is_file() or child.is_dir()):
                raise ValueError(f"Unsupported cache member: {child}")
            if child.is_file():
                content.update(
                    f"{index}/{child.relative_to(root).as_posix()}\0".encode()
                )
                content.update(bytes.fromhex(digest(child)))
    manifest = {
        "schema": 1,
        "family": family,
        "compatibility": compatibility,
        "sha": sha,
        "roots": len(paths),
        "content_hash": content.hexdigest(),
        "legacy": legacy or [],
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.unlink(missing_ok=True)
    with path.open("wb") as destination:
        with gzip.GzipFile(
            fileobj=destination, mode="wb", compresslevel=1, mtime=0
        ) as compressed:
            with tarfile.open(
                fileobj=compressed, mode="w|", format=tarfile.PAX_FORMAT
            ) as archive:
                data = json.dumps(manifest, sort_keys=True).encode()
                header = tarfile.TarInfo("manifest.json")
                header.size = len(data)
                archive.addfile(header, io.BytesIO(data))
                for index, root in enumerate(paths):
                    archive.add(root, arcname=f"root{index}", recursive=True)
    item = metadata(path)
    bound = envelope(item["bytes"])
    limit = POLICY["families"][family] * MIB
    if bound > limit:
        raise PayloadTooLarge(bound, limit, family)
    return dict(item, envelope=bound)


def allocated_size(root):
    size = 0
    for child in root.rglob("*"):
        if child.is_file():
            stat = child.stat()
            # ccache accounts disk blocks. Windows lacks st_blocks; its rounded
            # file size is only a trim heuristic, never archive admission.
            size += (
                stat.st_blocks * 512
                if hasattr(stat, "st_blocks")
                else ((stat.st_size + 4095) // 4096) * 4096
            )
    return size


def pack_to_fit(path, paths, family, compatibility, sha, legacy=None):
    for attempt in range(FIT_RETRIES + 1):
        try:
            return pack(path, paths, family, compatibility, sha, legacy)
        except PayloadTooLarge as error:
            if attempt == FIT_RETRIES:
                path.unlink(missing_ok=True)
                raise
            # Scale current local allocations by the measured inner-envelope
            # ratio, with 5% headroom. Repack rather than assume cleanup fits;
            # the SDK's actual compressed-stream guard remains authoritative.
            for root in paths:
                target_kib = max(
                    1,
                    allocated_size(root)
                    * max(1, error.limit - MIB)
                    * 95
                    // (max(1, error.size - MIB) * 100 * 1024),
                )
                print(
                    f"Local fit attempt {attempt + 1}/{FIT_RETRIES}: "
                    f"{error.size} > {error.limit}; trim {root} to {target_kib} KiB"
                )
                subprocess.run(
                    [os.environ.get("CCACHE_EXE", "ccache"), "--cleanup"],
                    env=dict(
                        os.environ,
                        CCACHE_DIR=str(root.resolve()),
                        CCACHE_CONFIGPATH=str(ROOT / "migration/ccache.conf"),
                        CCACHE_MAXSIZE=f"{target_kib}KiB",
                    ),
                    check=True,
                )


def unpack(path, paths, family, compatibility):
    manifest = metadata(path)
    if (
        manifest.get("schema") != 1
        or manifest.get("family") != family
        or manifest.get("compatibility") != compatibility
        or manifest.get("roots") != len(paths)
    ):
        return False
    with tarfile.open(path, "r:gz") as archive:
        for member in archive.getmembers():
            if member.name == "manifest.json":
                continue
            parts = Path(member.name).parts
            root = next((i for i in range(len(paths)) if parts[0] == f"root{i}"), None)
            if root is None or ".." in parts or Path(member.name).is_absolute():
                raise ValueError(f"Invalid payload member: {member.name}")
            target = paths[root].joinpath(*parts[1:])
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
            elif member.isfile():
                target.parent.mkdir(parents=True, exist_ok=True)
                with (
                    archive.extractfile(member) as source,
                    target.open("wb") as destination,
                ):
                    shutil.copyfileobj(source, destination)
                target.chmod(member.mode & 0o777)
            else:
                raise ValueError(f"Unsupported payload member: {member.name}")
    return True


def output(**values):
    with open(os.environ["GITHUB_OUTPUT"], "a") as destination:
        for key, value in values.items():
            destination.write(
                f"{key}={str(value).lower() if isinstance(value, bool) else value}\n"
            )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "operation",
        choices=("identify", "remember", "unpack", "pack", "guard", "verify"),
    )
    parser.add_argument("--slot", choices=("0", "1"))
    args = parser.parse_args()
    family = os.environ["CACHE_FAMILY"]
    compatibility = os.environ["CACHE_PREFIX"]
    paths = [Path(value) for value in os.environ["CACHE_PATHS"].splitlines() if value]
    payload = payload_path(family)
    state_dir = Path(f".local/cache-state/{family}")
    state_dir.mkdir(parents=True, exist_ok=True)
    if args.operation == "identify":
        output(path=payload.as_posix(), family=family)
    elif args.operation == "remember":
        if payload.exists():
            item = metadata(payload)
            item["key"] = os.environ["MATCHED_KEY"]
            (state_dir / f"{args.slot}.json").write_text(json.dumps(item))
            payload.replace(state_dir / f"{args.slot}.tgz")
    elif args.operation == "unpack":
        restored = False
        for slot in ("0", "1"):
            candidate = state_dir / f"{slot}.tgz"
            if candidate.exists() and unpack(candidate, paths, family, compatibility):
                restored = True
        output(restored=restored)
    elif args.operation == "pack":
        publication = json.loads((state_dir / "publication.json").read_text())
        item = pack_to_fit(
            payload,
            paths,
            family,
            compatibility,
            os.environ["GITHUB_SHA"],
            publication["legacy"],
        )
        (state_dir / "upload.json").write_text(json.dumps(item))
        cache_zstd.install(POLICY["families"][family] * MIB, state_dir / "outer.json")
        output(hash=item["hash"], envelope=item["envelope"])
        print(json.dumps(item, sort_keys=True))
    elif args.operation == "guard":
        cache_zstd.probe(POLICY["families"][family] * MIB, state_dir / "outer.json")
    elif args.operation == "verify":
        expected = json.loads((state_dir / "upload.json").read_text())
        actual = metadata(payload)
        if any(actual[key] != expected[key] for key in actual):
            raise ValueError("Saved slot does not contain this exact protected payload")
        print(
            f"Restored and verified protected payload {actual['hash']} ({actual['bytes']} bytes)"
        )


if __name__ == "__main__":
    main()
