"""Build-produced executable identity; checkout HEAD alone is not provenance."""

import hashlib
import json
import os
import subprocess
import tempfile
from pathlib import Path

IDENTITY_NAME = "candidate-build-identity.json"


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def source_identity(root):
    def git(*args):
        return subprocess.check_output(["git", *args], cwd=root)

    commit = git("rev-parse", "HEAD").decode().strip()
    status = git("status", "--porcelain", "--untracked-files=all").decode().strip()
    names = git("ls-files", "--cached", "--others", "--exclude-standard", "-z").split(
        b"\0"
    )
    hasher = hashlib.sha256(commit.encode())
    for name in sorted(set(filter(None, names))):
        path = Path(root) / os.fsdecode(name)
        hasher.update(len(name).to_bytes(8, "big") + name)
        if path.is_symlink():
            data = b"link\0" + os.fsencode(os.readlink(path))
        elif path.is_file():
            data = b"file\0" + path.read_bytes()
        else:
            data = b"missing\0"
        hasher.update(hashlib.sha256(data).digest())
    return {
        "source_commit": commit,
        "source_status": status,
        "source_digest": hasher.hexdigest(),
    }


def write_identity(build, binary, before, after, configuration):
    cache_digest = digest(build / "CMakeCache.txt")
    identity = {
        "schema_version": 1,
        **before,
        "source_stable": before == after,
        "configuration_digest": hashlib.sha256(
            json.dumps(configuration, sort_keys=True).encode()
        ).hexdigest(),
        "configuration_cache_sha256": cache_digest,
        "binary_sha256": digest(binary),
    }
    with tempfile.NamedTemporaryFile(mode="w", dir=build, delete=False) as output:
        temporary = Path(output.name)
        json.dump(identity, output, indent=2)
        output.write("\n")
    temporary.replace(build / IDENTITY_NAME)
    return identity


def read_identity(build, binary):
    """Associate only an exact executable and unchanged configured build inputs."""
    try:
        identity = json.loads((build / IDENTITY_NAME).read_text())
        if (
            identity["schema_version"] != 1
            or identity["binary_sha256"] != digest(binary)
            or identity["configuration_cache_sha256"]
            != digest(build / "CMakeCache.txt")
        ):
            return None
        required = (
            "source_commit",
            "source_status",
            "source_digest",
            "configuration_digest",
            "binary_sha256",
        )
        if not all(
            isinstance(identity[field], str) for field in required
        ) or not isinstance(identity["source_stable"], bool):
            return None
        return identity
    except (OSError, ValueError, KeyError, TypeError):
        return None
