"""Landscape scheduler comparison for map phases unreachable by scenarios."""

import subprocess


def scheduler_gap():
    """Compare unreachable map phases and commit-after-batch with unchanged C++."""
    import migration

    root = migration.ROOT
    out = migration.LOCAL / "landscape-loop-gap"
    out.mkdir(parents=True, exist_ok=True)
    source = (migration.REFERENCE / "src/landscape.cpp").read_text()
    start = source.index("void RunTileLoop()")
    end = source.index("\nvoid InitializeLandscape()", start)
    (out / "landscape-loop-reference.inc").write_text(source[start:end])
    binary = out / "landscape-loop-gap"
    subprocess.run(
        [
            "c++",
            "-std=c++20",
            "-O2",
            "-I" + str(root / "src"),
            "-I" + str(out),
            str(root / "tools/simulation/landscape-loop-reference.cpp"),
            str(migration.rust_archive(root / "build-rust")),
            "-ldl",
            "-lpthread",
            "-lm",
            "-o",
            str(binary),
        ],
        cwd=root,
        env=migration.environment(),
        check=True,
    )
    subprocess.run([str(binary)], cwd=root, check=True)
