#!/usr/bin/env python3
"""Run the real #249 CLI for every seeded category, upstream scope and ratchet."""

import json
import subprocess
import tempfile
from pathlib import Path

SCRIPT = Path(__file__).with_name("shared-accessors.py")
EXPRESSIONS = (
    "v->owner|s->index.base()|b->owner|c->money|_settings_game.vehicle.plane_speed|"
    "_settings_client.sound.disaster|TimerGameCalendar::date|TimerGameEconomy::date|"
    "TimerGameTick::counter|_game_mode == GM_EDITOR|_current_company.base()|Map::Size()|"
    "Canonical(v)|to_underlying(Canonical(v))|static_cast<Vehicle *>(context)->owner|"
    "Company::Get(id)->money"
).split("|")

with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    for name in ["src/rust", "tools", "migration"]:
        (root / name).mkdir(parents=True)

    def git(*args):
        return subprocess.check_output(["git", *args], cwd=root, text=True).strip()

    git("init", "-q")
    git("config", "user.name", "Accessor regression")
    git("config", "user.email", "accessors@example.invalid")
    port = root / "src/port.cpp"
    original = "int Inherited(Vehicle *v) { return v->owner; }"
    port.write_text(original)
    git("add", ".")
    git("commit", "-qm", "Add: Reference fixture")
    (root / "migration/baseline.json").write_text(
        json.dumps({"commit": git("rev-parse", "HEAD")})
    )
    (root / "tools/shared-accessors.py").write_text(SCRIPT.read_text())
    (root / "src/rust/services.h").write_text(
        "int Canonical(Vehicle *v) { return v->owner; }"
    )
    baseline = root / "tools/shared-accessor-baseline.txt"
    baseline.write_text("")
    git("add", ".")
    git("commit", "-qm", "Add: Empty duplicate baseline")
    base = git("rev-parse", "HEAD")

    def check():
        return subprocess.run(
            ["python3", "tools/shared-accessors.py", "--base-ref", base],
            cwd=root,
            capture_output=True,
            text=True,
        )

    assert check().returncode == 0
    for expression in EXPRESSIONS:
        for prefix in ["int Renamed", "[]"]:
            port.write_text(
                original
                + f"{prefix}(Vehicle *v, Station *s, BaseStation *b, Company *c) noexcept {{ return {expression}; }}"
            )
            result = check()
            assert result.returncode == 1 and "new shared accessor:" in result.stdout, (
                result
            )
    fingerprint = result.stdout.split("new shared accessor: ")[1].splitlines()[0]
    baseline.write_text(fingerprint + "\n")
    assert "addition forbidden" in check().stdout and check().returncode == 1
    git("commit", "-qam", "Change: Grandfather fixture fingerprint")
    base = git("rev-parse", "HEAD")
    assert check().returncode == 0
    port.write_text(original)
    assert "remove retired" in check().stdout
    baseline.write_text("")
    assert check().returncode == 0
    baseline.unlink()
    assert check().returncode != 0
