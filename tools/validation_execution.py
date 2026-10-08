"""Small observable subprocess and file-lock helpers for validation tools."""

import codecs
import errno
import json
import os
import select
import signal
import socket
import subprocess
import sys
import time
import uuid
from contextlib import contextmanager
from pathlib import Path

HEARTBEAT_SECONDS = 15


def run_logged(
    command,
    *,
    cwd,
    env,
    log,
    phase,
    heartbeat=HEARTBEAT_SECONDS,
    stream=False,
    timeout=None,
):
    """Retain complete output and report bounded progress even for quiet commands."""
    log = Path(log)
    print(f"{phase}: {' '.join(map(str, command))}; log: {log}", flush=True)
    started = time.monotonic()
    position = 0
    decoder = codecs.getincrementaldecoder("utf-8")("replace")
    next_heartbeat = started + heartbeat
    with log.open("w") as output:
        reader, writer = os.pipe() if os.name == "posix" else (None, None)
        supervised = [sys.executable, __file__, str(reader), *map(str, command)]
        process = subprocess.Popen(
            supervised if reader is not None else command,
            cwd=cwd,
            env=env,
            stdout=output,
            stderr=subprocess.STDOUT,
            start_new_session=os.name == "posix",
            pass_fds=(reader,) if reader is not None else (),
        )
        if reader is not None:
            os.close(reader)
        try:
            while True:
                try:
                    code = process.wait(timeout=min(0.2, heartbeat))
                except subprocess.TimeoutExpired:
                    code = None
                now = time.monotonic()
                if code is None and timeout is not None and now - started >= timeout:
                    raise subprocess.TimeoutExpired(command, timeout)
                if stream:
                    with log.open("rb") as reader:
                        reader.seek(position)
                        text = decoder.decode(reader.read(), final=code is not None)
                        position = reader.tell()
                    if text:
                        print(text, end="", flush=True)
                if code is not None:
                    break
                if now >= next_heartbeat:
                    print(
                        f"{phase}: running {now - started:.1f}s; log bytes: {log.stat().st_size}; log: {log}",
                        flush=True,
                    )
                    next_heartbeat = now + heartbeat
        finally:
            # The supervisor owns the command's session. EOF kills its group even
            # when this driver dies from SIGKILL and cannot run Python cleanup.
            if writer is not None:
                os.close(writer)
            elif process.poll() is None:
                process.kill()
            process.wait()
    print(
        f"{phase}: exit {code} after {time.monotonic() - started:.1f}s; log: {log}",
        flush=True,
    )
    return code


@contextmanager
def file_lock(path, *, shared=False, label=None, heartbeat=HEARTBEAT_SECONDS):
    """Coordinate cooperating processes and expose waiting/holder metadata.

    POSIX shared locks let ordinary games overlap. Windows uses exclusive byte
    locks for both modes. Metadata is diagnostic only; OS locks enforce exclusion.
    """
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    holders = path.with_name(path.name + ".holders")
    holders.mkdir(exist_ok=True)
    label = label or str(path)
    if os.name == "nt":
        import msvcrt

        def acquire(handle):
            handle.seek(0)
            msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)

        def release(handle):
            handle.seek(0)
            msvcrt.locking(handle.fileno(), msvcrt.LK_UNLCK, 1)
    else:
        import fcntl

        def acquire(handle):
            fcntl.flock(
                handle, (fcntl.LOCK_SH if shared else fcntl.LOCK_EX) | fcntl.LOCK_NB
            )

        def release(handle):
            fcntl.flock(handle, fcntl.LOCK_UN)

    with path.open("a+b") as handle:
        if os.name == "nt" and path.stat().st_size == 0:
            handle.write(b"\0")
            handle.flush()
        started = time.monotonic()
        next_notice = started
        waited = False
        while True:
            try:
                acquire(handle)
                break
            except OSError as error:
                if error.errno not in (errno.EAGAIN, errno.EACCES, errno.EDEADLK):
                    raise
                waited = True
                now = time.monotonic()
                if now >= next_notice:
                    owners = []
                    for owner in holders.glob("*.json"):
                        try:
                            data = json.loads(owner.read_text())
                            if (
                                os.name == "posix"
                                and data["host"] == socket.gethostname()
                            ):
                                os.kill(data["pid"], 0)
                            owners.append(data)
                        except (OSError, ValueError, KeyError):
                            continue
                    print(
                        f"{label}: waiting {now - started:.1f}s; holders: {json.dumps(owners)}; lock: {path}",
                        flush=True,
                    )
                    next_notice = now + heartbeat
                time.sleep(min(0.1, heartbeat))
        owner = holders / f"{os.getpid()}-{uuid.uuid4().hex}.json"
        try:
            owner.write_text(
                json.dumps(
                    {
                        "pid": os.getpid(),
                        "host": socket.gethostname(),
                        "label": label,
                        "mode": "shared" if shared else "exclusive",
                        "started_at": time.time(),
                    }
                )
            )
            if waited:
                print(
                    f"{label}: acquired after {time.monotonic() - started:.1f}s; lock: {path}",
                    flush=True,
                )
            yield
        finally:
            owner.unlink(missing_ok=True)
            release(handle)


if __name__ == "__main__":
    reader = int(sys.argv[1])
    process = subprocess.Popen(sys.argv[2:])
    while process.poll() is None:
        ready, _, _ = select.select([reader], [], [], 0.1)
        if ready and not os.read(reader, 1):
            os.killpg(os.getpgrp(), signal.SIGKILL)
    sys.exit(
        process.returncode if process.returncode >= 0 else 128 - process.returncode
    )
