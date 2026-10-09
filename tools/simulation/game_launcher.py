#!/usr/bin/env python3
"""Run a harness game with its original synchronous thread-failure fallback."""

import _thread
import os
import sys


def main():
    if sys.platform != "linux":
        sys.exit("deterministic simulation requires Linux RLIMIT_NPROC")

    import resource

    # This process immediately execs the game; the parent and other scenarios
    # retain their limits. Linux counts threads against the user's process limit.
    resource.setrlimit(resource.RLIMIT_NPROC, (0, 0))
    try:
        _thread.start_new_thread(lambda: None, ())
    except RuntimeError:
        pass
    else:
        # Root and privileged capabilities can bypass this limit. Never silently
        # run threaded jobs and reintroduce wall-time-dependent pause iterations.
        sys.exit("RLIMIT_NPROC did not prevent threads; run as an unprivileged user")
    os.execvpe(sys.argv[1], sys.argv[1:], os.environ)


if __name__ == "__main__":
    main()
