#!/usr/bin/env python3
"""Cap the pinned cache SDK's actual compressed archive before it can upload."""

import argparse
import json
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def guarded(binary, arguments, limit, source, destination, receipt=None):
    def confirmed(size):
        if receipt:
            Path(receipt).write_text(
                json.dumps(
                    {"size": size, "cap": limit, "sha": os.environ.get("GITHUB_SHA")}
                )
            )
        print(
            f"SDK archive: {size} bytes <= hard cap {limit} before upload",
            file=sys.stderr,
        )

    # Preserve SDK version detection and backend restore/decompression verbatim.
    if any(
        arg in arguments
        for arg in ("--version", "-V", "-d", "--decompress", "-t", "--test")
    ):
        return subprocess.run(
            [binary, *arguments], stdin=source, stdout=destination, check=False
        ).returncode
    output_file = None
    for index, argument in enumerate(arguments):
        if argument in ("-o", "--output"):
            output_file = arguments[index + 1]
        elif argument.startswith("--output="):
            output_file = argument.split("=", 1)[1]
    if output_file and output_file != "-":
        result = subprocess.run(
            [binary, *arguments], stdin=source, stdout=destination, check=False
        )
        if result.returncode:
            return result.returncode
        if Path(output_file).stat().st_size > limit:
            print(
                f"SDK cache archive exceeds hard cap {limit} bytes; upload prohibited",
                file=sys.stderr,
            )
            return 1
        confirmed(Path(output_file).stat().st_size)
        return 0
    process = subprocess.Popen(
        [binary, *arguments], stdin=source, stdout=subprocess.PIPE
    )
    total = 0
    with process.stdout:
        while chunk := process.stdout.read(64 * 1024):
            total += len(chunk)
            if total > limit:
                process.terminate()
                process.wait()
                print(
                    f"SDK cache archive exceeds hard cap {limit} bytes; upload prohibited",
                    file=sys.stderr,
                )
                return 1
            destination.write(chunk)
    result = process.wait()
    if result == 0:
        confirmed(total)
    return result


def wrappers(python, script, binary, limit, receipt):
    arguments = [
        python,
        script,
        "--binary",
        binary,
        "--limit",
        str(limit),
        "--receipt",
        receipt,
    ]
    shell = "#!/usr/bin/env bash\nexec " + shlex.join(arguments) + ' -- "$@"\n'
    mt_shell = "#!/usr/bin/env bash\nexec " + shlex.join(arguments) + ' -- -T0 "$@"\n'
    # SDK exec on Windows resolves .cmd through PATHEXT; GNU tar uses the
    # extensionless Git Bash shim. Both invoke the same guard and real binary.
    if any(any(char in value for char in '"%\r\n') for value in arguments):
        raise ValueError("Unsupported cache guard launcher path")
    windows = "@" + " ".join(f'"{value}"' for value in arguments) + " -- %*\r\n"
    return {"zstd": shell, "zstdmt": mt_shell, "zstd.cmd": windows}


def install(limit, receipt):
    binary = shutil.which("zstd")
    if not binary:
        raise ValueError("zstd is mandatory for the fixed writer cache version")
    directory = Path(os.environ["RUNNER_TEMP"]) / "cache-archive-guard"
    directory.mkdir(parents=True, exist_ok=True)
    receipt.unlink(missing_ok=True)
    launchers = wrappers(
        Path(sys.executable).as_posix(),
        (ROOT / "tools/cache_zstd.py").as_posix(),
        Path(binary).as_posix(),
        limit,
        receipt.resolve().as_posix(),
    )
    for name, text in launchers.items():
        path = directory / name
        path.write_text(text)
        path.chmod(0o755)
    with open(os.environ["GITHUB_PATH"], "a") as destination:
        destination.write(str(directory) + "\n")
    print(
        f"Guarding actual SDK zstd archive at {limit} bytes before reservation/upload"
    )


def sdk_tar():
    if sys.platform == "win32":
        binary = Path(os.environ["PROGRAMFILES"]) / "Git/usr/bin/tar.exe"
        if not binary.is_file():
            raise ValueError("Fixed Windows archive contract requires Git GNU tar")
        return str(binary)
    return (shutil.which("gtar") if sys.platform == "darwin" else None) or shutil.which(
        "tar"
    )


def probe(limit, receipt):
    # Probe the same GNU/BSD tar compressor lookup that the pinned SDK uses,
    # before save@v4 can reserve or upload. The private probe drains tar input
    # and returns its configuration, never an archive or a size witness.
    with tempfile.TemporaryDirectory() as temporary:
        directory = Path(temporary)
        (directory / "probe").write_text("guard routing\n")
        (directory / "manifest.txt").write_text("probe\n")
        command = [
            sdk_tar(),
            "--posix",
            "-cf",
            "probe.json",
            "--exclude",
            "probe.json",
            "-P",
            "-C",
            str(directory),
            "--files-from",
            "manifest.txt",
        ]
        if sys.platform == "win32":
            command.append("--force-local")
        elif sys.platform == "darwin" and Path(command[0]).name == "gtar":
            command.append("--delay-directory-restore")
        compressor = "zstd -T0" if sys.platform == "win32" else "zstdmt"
        command += ["--use-compress-program", compressor + " --cache-guard-tar-probe"]
        subprocess.run(command, cwd=directory, check=True, capture_output=True)
        witness = json.loads((directory / "probe.json").read_text())
        if witness["cap"] != limit or Path(witness["receipt"]) != receipt.resolve():
            raise ValueError("SDK tar compressor did not select this family's guard")
    print("Verified pinned SDK tar selects the archive guard before upload")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True)
    parser.add_argument("--limit", type=int, required=True)
    parser.add_argument("--receipt")
    parser.add_argument("arguments", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    arguments = args.arguments[1:] if args.arguments[:1] == ["--"] else args.arguments
    if any(
        value in arguments
        for value in ("--cache-guard-probe", "--cache-guard-tar-probe")
    ):
        if "--cache-guard-tar-probe" in arguments:
            while sys.stdin.buffer.read(64 * 1024):
                pass
        print(
            json.dumps(
                {
                    "cap": args.limit,
                    "receipt": args.receipt,
                    "sha": os.environ.get("GITHUB_SHA"),
                }
            )
        )
        return
    raise SystemExit(
        guarded(
            args.binary,
            arguments,
            args.limit,
            sys.stdin.buffer,
            sys.stdout.buffer,
            args.receipt,
        )
    )


if __name__ == "__main__":
    main()
