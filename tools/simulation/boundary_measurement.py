#!/usr/bin/env python3
"""#274 diagnostic runner; execute only in a root-scheduled quiet-host window.

cpu: canonical interleaved benchmark invocations plus exact-child wait4 CPU.
profile: canonical one-pair benchmark with Callgrind inserted after the launcher.
All scenario preparation, locks, runtime capture and comparisons remain in core.
"""

import json
import os
import subprocess
import sys
import threading
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import migration  # noqa: E402
from simulation import core  # noqa: E402


def cpu_run(command, *, timeout, **kwargs):
    """Keep the canonical watchdog/reaping policy, measuring this child only."""
    started = time.monotonic()
    with subprocess.Popen(command, **kwargs) as child:
        expired = threading.Event()

        def kill():
            expired.set()
            child.kill()

        watchdog = threading.Timer(timeout, kill)
        try:
            watchdog.start()
            _, status, usage = os.wait4(child.pid, 0)
            child.returncode = os.waitstatus_to_exitcode(status)
        except BaseException:
            child.kill()
            child.wait()
            raise
        finally:
            watchdog.cancel()
            if watchdog.ident is not None:
                watchdog.join()
        if expired.is_set():
            raise subprocess.TimeoutExpired(command, timeout)
        print(
            "CPU "
            + json.dumps(
                dict(
                    command=command,
                    pid=child.pid,
                    user=usage.ru_utime,
                    system=usage.ru_stime,
                    wall=time.monotonic() - started,
                    exit=child.returncode,
                )
            ),
            flush=True,
        )
        return subprocess.CompletedProcess(command, child.returncode)


def main():
    mode = sys.argv.pop(1) if len(sys.argv) > 1 else ""
    if mode not in ("cpu", "profile") or "--benchmark" not in sys.argv:
        raise SystemExit(
            "usage: boundary_measurement.py cpu|profile SCENARIO --benchmark N --jobs 1 [--candidate BINARY]"
        )
    if mode == "cpu":
        core.benchmark_run = cpu_run
    else:
        original = core.benchmark_run
        root = migration.COMMON_LOCAL / "valgrind/usr"
        valgrind = root / "bin/valgrind.bin"
        if not valgrind.is_file():
            raise SystemExit(f"missing {valgrind}")

        def profile(command, **kwargs):
            output = Path(command[command.index("-c") + 1]).parent / "callgrind.out"
            env = dict(kwargs.pop("env"), VALGRIND_LIB=str(root / "libexec/valgrind"))
            env.pop("OPENTTD_WITNESS", None)
            flags = [
                str(valgrind),
                "--tool=callgrind",
                "--dump-instr=yes",
                f"--log-file={output.with_suffix('.log')}",
                f"--callgrind-out-file={output}",
            ]
            return original(command[:2] + flags + command[2:], env=env, **kwargs)

        core.benchmark_run = profile
    return core.main()


if __name__ == "__main__":
    raise SystemExit(main())
