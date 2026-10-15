#!/usr/bin/env python3
"""Run the real #249 CLI for every seeded category, upstream scope and ratchet."""

import re
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
        '{"commit":"%s"}' % git("rev-parse", "HEAD")
    )
    (root / "tools/shared-accessors.py").write_text(SCRIPT.read_text())
    canonical = "int Canonical(Vehicle *v) { return v->owner; }"
    for shared in ["services.h", "engine_services.h", "presentation_services.h"]:
        (root / "src/rust" / shared).write_text(canonical)
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
                + f"{prefix}(const Vehicle *const v, Station *s, BaseStation *b, Company *c) noexcept {{ return {expression}; }}"
            )
            if prefix == "[]" and expression == "Map::Size()":
                port.write_text(original + "[] { return Map::Size(); }")
            result = check()
            assert result.returncode == 1 and "new shared accessor:" in result.stdout
    fingerprint = result.stdout.split("new shared accessor: ")[1].splitlines()[0]
    baseline.write_text(fingerprint + "\n")
    assert "addition forbidden" in check().stdout and check().returncode == 1
    git("commit", "-qam", "Change: Grandfather fixture fingerprint")
    base = git("rev-parse", "HEAD")
    assert check().returncode == 0
    copy = port.read_text()[len(original) :]
    for changed in [original + copy * 2, original + copy.replace("money", "owner")]:
        port.write_text(changed)
        assert "new shared accessor:" in check().stdout and check().returncode == 1
    port.write_text(original)
    assert "remove retired" in check().stdout
    baseline.write_text("")
    assert check().returncode == 0
    owner = root / "src/owner.rs"
    owner.write_text('#[unsafe(no_mangle)] pub extern "C" fn openttd_rust_dead() {}')
    caller = root / "tools/caller.py"
    caller.write_text("")
    git("add", ".")
    assert "orphan | openttd_rust_dead" in check().stdout
    port.write_text(original + "\nvoid openttd_rust_dead();\n")
    assert "orphan | openttd_rust_dead" in check().stdout
    for target, call in [
        (port, "void Call() { openttd_rust_dead(); }"),
        (owner, "fn call() { openttd_rust_dead(); }"),
        (caller, "source = 'openttd_rust_dead();'"),
    ]:
        before = target.read_text()
        target.write_text(before + call)
        assert check().returncode == 0, check().stdout
        target.write_text(before)
    caller.write_text("source = 'openttd_rust_dead();'")
    duplicate = "\nvoid Callback() noexcept {}\nOpenTTDFixtureServices one{.a = Callback,\n};\nOpenTTDFixtureServices two{.b = Callback,\n};\n"
    write = "\nvoid Write(uint8_t value) { _current_company = value; }\n"
    port.write_text(port.read_text() + duplicate + write)
    result = check()
    assert "binding | Callback | 2" in result.stdout and "write |" in result.stdout
    fingerprints = re.findall(r"new shared accessor: (.*)", result.stdout)
    baseline.write_text("\n".join(fingerprints) + "\n")
    assert "addition forbidden" in check().stdout
    baseline.write_text("")
    extra = "\nOpenTTDFixtureServices extra{.c = Callback,\n};\n"
    port.write_text(port.read_text() + extra)
    git("commit", "-qam", "Add: Actual boundary seed source")
    base = git("rev-parse", "HEAD")
    port.write_text(port.read_text().removesuffix(extra))
    baseline.write_text("\n".join(fingerprints) + "\n")
    assert check().returncode == 0, check().stdout
    baseline.write_text(baseline.read_text() + "binding | Invented | 2\n")
    assert "addition forbidden" in check().stdout
    baseline.write_text("\n".join(fingerprints) + "\n")
    git("commit", "-qam", "Add: Boundary baseline")
    base = git("rev-parse", "HEAD")
    port.write_text(port.read_text() + extra)
    result = check()
    assert "new shared accessor:" in result.stdout
    added = result.stdout.split("new shared accessor: ")[1].splitlines()[0]
    baseline.write_text(baseline.read_text() + added + "\n")
    assert "addition forbidden" in check().stdout
    port.write_text(original)
    owner.write_text("")
    assert "remove retired" in check().stdout
    baseline.write_text("")
    assert check().returncode == 0
    baseline.unlink()
    assert check().returncode != 0
