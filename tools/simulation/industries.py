"""Industry production/history and builder evidence on existing reference inputs."""

import shutil
from pathlib import Path

from . import disasters, economy
from .core import SNAPSHOT_TICKS, TICKS_PER_DAY, read_save, run_game

AI_FOLDER = "industry-scenario-ai"


def uses_ai(scenario):
    return scenario.get("industry") == "lumber"


def install(scenario, run_dir):
    if uses_ai(scenario):
        shutil.copytree(scenario["scenario_ai"], run_dir / "ai/industry-scenarios")


def game_args(scenario):
    return ["-d", "script=2"] if uses_ai(scenario) else []


def scenarios(soak):
    result = [
        dict(
            scenario,
            name="industry-" + scenario["economy"],
            industry="cargo",
            ticks=scenario["ticks"]
            if scenario["economy"] == "stockpile"
            else (16 if soak else 6) * SNAPSHOT_TICKS,
        )
        for scenario in economy.scenarios(soak)
    ]
    for climate in ("temperate", "arctic", "tropic", "toyland", "lumber"):
        result.append(
            {
                "name": "industry-" + climate,
                "kind": "generate",
                "seed": 777 if climate in ("tropic", "lumber") else 12345,
                "map_log2": 8,
                "land_generator": 1,
                "industry": climate,
                "industry_soak": soak,
                "ticks": (6 if soak else 2) * 365 * TICKS_PER_DAY,
                "console": ["setting difficulty.disasters 0"],
                "settings": {
                    "game_creation": {
                        "landscape": "tropic" if climate == "lumber" else climate
                    },
                    "difficulty": {
                        "industry_density": 4,
                        "max_loan": 1000000 if climate == "lumber" else 300000,
                    },
                },
            }
        )
    for mode in ("original", "frozen", "control", "closure", "farm-cleanup", "builder"):
        result.append(
            {
                "name": "industry-" + mode,
                "kind": "generate",
                "seed": 12345,
                "map_log2": 7,
                "land_generator": 1,
                "industry": mode,
                "industry_soak": soak,
                "ticks": (6 if soak else 2) * 365 * TICKS_PER_DAY,
                "settings": {"difficulty": {"industry_density": 4}},
                "console": ["setting difficulty.disasters 0"],
            }
        )
    for name, density in (("fund-only", 0), ("custom-forced", 6)):
        result.append(
            {
                "name": "industry-generation-" + name,
                "kind": "generate",
                "seed": 12345,
                "map_log2": 7,
                "land_generator": 1,
                "industry": "generation",
                "ticks": 2 * SNAPSHOT_TICKS,
                "settings": {
                    "difficulty": {"industry_density": density},
                    "game_creation": {"custom_industry_number": 0},
                },
                "console": ["setting difficulty.disasters 0"],
                "industry_generation_empty": density == 0,
            }
        )
    return result


def prepare(scenario, binaries, builds, out, timeout, env, result):
    mode = scenario.get("industry")
    if mode == "lumber":
        setup = dict(
            scenario,
            ticks=3 * SNAPSHOT_TICKS,
            console=[*scenario["console"], "start_ai MigrationIndustries"],
        )
        run = run_game(
            setup,
            binaries["reference"],
            builds["reference"],
            out / scenario["name"] / "input" / "reference",
            timeout,
            env,
        )
        if run["exit"] or not any(
            "INDUSTRY-LUMBER ready true" in line for line in run["log"]
        ):
            raise RuntimeError("reference setup AI did not build the lumber mill")
        source = run["snapshots"][-1]
        target = out / scenario["name"] / "input" / "lumber.sav"
        result["industry_input"] = disasters.patch(source, target, {})
        return dict(scenario, kind="save", save=str(target))
    if mode not in (
        "original",
        "frozen",
        "control",
        "closure",
        "farm-cleanup",
        "builder",
    ):
        return scenario
    folder = out / scenario["name"] / "input"
    setup = dict(scenario, ticks=2 * SNAPSHOT_TICKS)
    run = run_game(
        setup,
        binaries["reference"],
        builds["reference"],
        folder / "reference",
        timeout,
        env,
    )
    if run["exit"] or not run["snapshots"]:
        raise RuntimeError("industry reference setup failed")
    source = run["snapshots"][-1]
    before = read_save(source)
    industries = economy.rows(before, "INDY")
    chosen = next(
        index
        for index, row in industries.items()
        if row.get("produced[0]/cargo", 255) < 64
    )
    changes = {}
    console = list(scenario.get("console", []))
    if mode in ("original", "frozen"):
        console.append("setting economy.type " + ("0" if mode == "original" else "2"))
    if mode == "control":
        changes["INDY"] = {index: {"ctlflags": 15} for index in industries}
    if mode in ("closure", "farm-cleanup"):
        if mode == "farm-cleanup":
            chosen = next(
                index for index, row in industries.items() if row["type"] == 9
            )
            result["industry_field_input"] = len(industry_fields(before, chosen))
            if not result["industry_field_input"]:
                raise RuntimeError("reference farm has no associated fields to remove")
        changes["INDY"] = {chosen: {"prod_level": 0}}
        console.append("setting difficulty.industry_density 0")
    if mode == "builder":
        changes["IBLD"] = {0: {"wanted_inds": 200 << 16}}
        # Disable current target selection optimizations and force an immediate
        # deficit reassessment. The reference computes all actual probabilities.
        changes["ITBL"] = {
            index: {"target_count": 0, "wait_count": 0, "max_wait": 1}
            for index in economy.rows(before, "ITBL")
        }
        changes["ECMY"] = {0: {"industry_daily_change_counter": 512 << 16}}
    target = folder / "input.sav"
    target.parent.mkdir(parents=True, exist_ok=True)
    result["industry_input"] = disasters.patch(source, target, changes)
    result["industry_chosen"] = chosen
    return dict(scenario, kind="save", save=str(target), console=console)


def check(scenario, run, mode, role, result):
    kind = scenario.get("industry")
    if kind is None or not run["snapshots"]:
        return
    saved = [read_save(path) for path in run["snapshots"]]
    rows = [economy.rows(chunks, "INDY") for chunks in saved]
    facts = {
        "industries": len(rows[-1]),
        "produced_history": max(
            (
                value
                for sample in rows
                for row in sample.values()
                for key, value in row.items()
                if key.endswith("/production")
            ),
            default=0,
        ),
        "transported_history": max(
            (
                value
                for sample in rows
                for row in sample.values()
                for key, value in row.items()
                if key.endswith("/transported")
            ),
            default=0,
        ),
        "accepted_history": max(
            (
                value
                for sample in rows
                for row in sample.values()
                for key, value in row.items()
                if key.startswith("accepted[") and key.endswith("/accepted")
            ),
            default=0,
        ),
        "valid_history": max(
            (row["valid_history"] for sample in rows for row in sample.values()),
            default=0,
        ),
        "builder_backoff": max(
            row["max_wait"]
            for chunks in saved
            for row in economy.rows(chunks, "ITBL").values()
        ),
    }
    facts["year_history"] = bool(facts["valid_history"] & (1 << 42))
    if kind in ("temperate", "arctic"):

        def fields(chunks):
            return {
                tile
                for tile, value in enumerate(chunks["MAPT"]["raw"])
                if value >> 4 == 0 and (chunks["MAP5"]["raw"][tile] >> 2) & 7 == 3
            }

        facts["new_field_tiles"] = (
            len(set.union(*(fields(c) for c in saved[1:])) - fields(saved[0]))
            if len(saved) > 1
            else 0
        )
        facts["field_tiles"] = len(fields(saved[-1]))
        if len(saved) > 1 and facts["new_field_tiles"] == 0:
            result["problems"].append(f"{mode}/{role}: no new farm-field tiles")
    if kind == "lumber":
        facts["lumber_harvest"] = any(
            row["type"] == 25
            and row.get("produced[0]/rate") == 0
            and any(
                value > 0
                for key, value in row.items()
                if key.startswith("produced[0]/history[")
                and key.endswith("/production")
            )
            for sample in rows
            for row in sample.values()
        )
        if not facts["lumber_harvest"]:
            result["problems"].append(f"{mode}/{role}: no actual lumber harvest")
    result[f"{mode}_{role}_industry"] = facts
    if kind == "generation":
        if bool(facts["industries"] == 0) != scenario["industry_generation_empty"]:
            result["problems"].append(
                f"{mode}/{role}: generation density branch missing"
            )
    if kind not in ("cargo", "generation") and facts["produced_history"] == 0:
        result["problems"].append(f"{mode}/{role}: no industry production history")
    if scenario.get("industry_soak") and not facts["year_history"]:
        result["problems"].append(f"{mode}/{role}: no yearly industry history")
    if kind == "cargo" and scenario["economy"] != "stockpile":
        if facts["transported_history"] == 0 or facts["accepted_history"] == 0:
            result["problems"].append(f"{mode}/{role}: no transported/accepted history")
    if kind in ("closure", "farm-cleanup"):
        chosen = result["industry_chosen"]
        if chosen in rows[-1]:
            result["problems"].append(f"{mode}/{role}: selected industry did not close")
        if kind == "farm-cleanup":
            facts["remaining_associated_fields"] = len(
                industry_fields(saved[-1], chosen)
            )
            if facts["remaining_associated_fields"]:
                result["problems"].append(
                    f"{mode}/{role}: farm fields remained associated"
                )
    if kind == "control":
        before = economy.rows(read_save(Path(scenario["save"])), "INDY")
        for index, row in rows[-1].items():
            if index in before and row["prod_level"] != before[index]["prod_level"]:
                result["problems"].append(
                    f"{mode}/{role}: controlled production changed"
                )
    if kind == "builder":
        before = economy.rows(read_save(Path(scenario["save"])), "INDY")
        facts["new_industries"] = len(set(rows[-1]) - set(before))
        if facts["new_industries"] == 0 or facts["builder_backoff"] <= 1:
            result["problems"].append(f"{mode}/{role}: no builder success/backoff")


def industry_fields(chunks, industry):
    """Canonical packed field association, exposed by the existing save decoder."""
    import struct

    return {
        tile
        for tile, value in enumerate(chunks["MAPT"]["raw"])
        if value >> 4 == 0
        and (chunks["MAP5"]["raw"][tile] >> 2) & 7 == 3
        and struct.unpack_from(">H", chunks["MAP2"]["raw"], tile * 2)[0] == industry
    }


def production_gap():
    """Absent NewGRFs: unchanged version, repetition and native arithmetic policy."""
    import importlib.util
    import subprocess

    from .core import ROOT

    spec = importlib.util.spec_from_file_location(
        "migration", ROOT / "tools/migration.py"
    )
    migration = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(migration)
    out = ROOT / ".local/industry-production-gap"
    out.mkdir(parents=True, exist_ok=True)
    source = subprocess.check_output(
        ["git", "show", migration.BASELINE["commit"] + ":src/newgrf_industries.cpp"],
        cwd=ROOT,
    ).decode()
    start = source.index("void IndustryProductionCallback(Industry *ind, int reason)")
    end = source.index("/**\n * Check whether an industry temporarily", start)
    (out / "industry-production-reference.inc").write_text(source[start:end])
    binary = out / "probe"
    subprocess.run(
        [
            "c++",
            "-std=c++20",
            "-O2",
            "-fno-strict-overflow",
            "-DPOINTER_IS_64BIT",
            "-I" + str(ROOT / "src"),
            "-I" + str(out),
            str(ROOT / "tools/migration/industry-production-gap.cpp"),
            str(migration.rust_archive(ROOT / "build-rust")),
            "-ldl",
            "-lpthread",
            "-lm",
            "-o",
            str(binary),
        ],
        cwd=ROOT,
        env=migration.environment(),
        check=True,
    )
    subprocess.run([str(binary)], cwd=ROOT, check=True)


def lifecycle_gap():
    """Absent custom industry GRFs: unchanged tile/parent RNG and animation."""
    import importlib.util
    import subprocess

    from .core import ROOT

    spec = importlib.util.spec_from_file_location(
        "migration", ROOT / "tools/migration.py"
    )
    migration = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(migration)
    out = ROOT / ".local/industry-lifecycle-gap"
    out.mkdir(parents=True, exist_ok=True)
    source = subprocess.check_output(
        ["git", "show", migration.BASELINE["commit"] + ":src/newgrf_industrytiles.cpp"],
        cwd=ROOT,
        text=True,
    )
    signatures = (
        "static bool DoTriggerIndustryTileAnimation(",
        "bool TriggerIndustryTileAnimation_ConstructionStageChanged(",
        "bool TriggerIndustryTileAnimation(",
        "bool TriggerIndustryAnimation(",
        "static void DoTriggerIndustryTileRandomisation(",
        "static void DoReseedIndustry(",
        "void TriggerIndustryTileRandomisation(",
        "void TriggerIndustryRandomisation(",
    )
    bodies = []
    for signature in signatures:
        start = source.index(signature)
        end = source.index("{", start) + 1
        depth = 1
        while depth:
            if source[end] == "{":
                depth += 1
            elif source[end] == "}":
                depth -= 1
            end += 1
        bodies.append(source[start:end])
    (out / "industry-lifecycle-reference.inc").write_text("\n\n".join(bodies))
    tile_source = subprocess.check_output(
        ["git", "show", migration.BASELINE["commit"] + ":src/industry_cmd.cpp"],
        cwd=ROOT,
        text=True,
    )
    animation = []
    for name in (
        "AnimateSugarSieve",
        "AnimateToffeeQuarry",
        "AnimateBubbleCatcher",
        "AnimatePowerPlantSparks",
        "AnimateToyFactory",
        "AnimatePlasticFountain",
        "AnimateOilWell",
        "AnimateMineTower",
        "AnimateTile_Industry",
    ):
        start = tile_source.index("static void " + name + "(")
        end = tile_source.index("{", start) + 1
        depth = 1
        while depth:
            if tile_source[end] == "{":
                depth += 1
            elif tile_source[end] == "}":
                depth -= 1
            end += 1
        animation.append(tile_source[start:end])
    drawing = subprocess.check_output(
        ["git", "show", migration.BASELINE["commit"] + ":src/table/industry_land.h"],
        cwd=ROOT,
        text=True,
    )
    start = drawing.index("static const uint8_t _industry_anim_offs_toffee[]")
    end = drawing.index("};", start) + 2
    animation.insert(0, drawing[start:end])
    (out / "industry-animation-reference.inc").write_text("\n\n".join(animation))
    graphics = subprocess.check_output(
        ["git", "show", migration.BASELINE["commit"] + ":src/industry_map.h"],
        cwd=ROOT,
        text=True,
    )
    start = graphics.index("enum IndustryGraphics :")
    end = graphics.index("};", start) + 2
    (out / "industry-graphics-reference.inc").write_text(graphics[start:end])
    binary = out / "probe"
    subprocess.run(
        [
            "c++",
            "-std=c++20",
            "-O2",
            "-fno-strict-overflow",
            "-DPOINTER_IS_64BIT",
            "-I" + str(ROOT / "src"),
            "-I" + str(out),
            str(ROOT / "tools/migration/industry-lifecycle-gap.cpp"),
            str(migration.rust_archive(ROOT / "build-rust")),
            "-ldl",
            "-lpthread",
            "-lm",
            "-o",
            str(binary),
        ],
        cwd=ROOT,
        env=migration.environment(),
        check=True,
    )
    subprocess.run([str(binary)], cwd=ROOT, check=True)


if __name__ == "__main__":
    production_gap()
    lifecycle_gap()
