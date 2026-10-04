"""play saves scenario evidence."""

import hashlib

from . import core
from .core import (
    ROOT,
    SNAPSHOT_TICKS,
    TICKS_PER_DAY,
    decode_element,
    read_save,
    run_game,
)

# Road networks built by LLM players in OpenTTD 15.1 (migration/saves/README.md),
# loaded with their scripts dropped. The first two run by default.
PLAY_SAVES = (
    "opus-55-167-002",
    "grok-159-001",
    "astra-156-003",
    "opus-55-165-002",
    "opus-5-133-009",
    "fable-152-004",
)
# Console commands run from scripts/game_start.scr after loading; the saves were
# written paused. cargodist uses short link graph intervals so jobs recur often.
DISTRIBUTIONS = {
    "manual": ["unpause"],
    "cargodist": [
        "setting linkgraph.distribution_pax 2",
        "setting linkgraph.distribution_mail 2",
        "setting linkgraph.distribution_default 1",
        "setting linkgraph.recalc_interval 4",
        "setting linkgraph.recalc_time 16",
        "unpause",
    ],
}


def scenarios(soak):
    scenarios = []
    years = 6 if soak else 2
    for save in PLAY_SAVES if soak else PLAY_SAVES[:2]:
        for distribution, commands in DISTRIBUTIONS.items():
            scenarios.append(
                {
                    "name": f"play-{save}-{distribution}",
                    "kind": "save",
                    "console": commands,
                    "save": str(ROOT / "migration/saves" / f"{save}.sav"),
                    "ticks": years * 365 * TICKS_PER_DAY + SNAPSHOT_TICKS,
                }
            )
    # Exercise both scalers for pax/mail/armoured, asymmetric default cargo, and
    # limits of the demand/distance rules and both MCF passes. Reload reuses an original snapshot containing
    # outstanding jobs, whose input/settings/join dates are saved in LGRJ.
    # This temperate save uses cargo IDs 0/2 for passengers/mail.
    variants = [
        (
            "symmetric",
            2,
            {
                "demand_distance": 255,
                "demand_size": 100,
                "accuracy": 2,
                "short_path_saturation": 0,
            },
        ),
        (
            "asymmetric",
            1,
            {
                "demand_distance": 0,
                "demand_size": 0,
                "accuracy": 64,
                "short_path_saturation": 250,
            },
        ),
    ]
    for label, distribution, settings in variants:
        commands = [
            f"setting linkgraph.distribution_{cargo} {min(distribution, 1) if cargo == 'default' else distribution}"
            for cargo in ("pax", "mail", "armoured", "default")
        ]
        commands += [
            f"setting linkgraph.{key} {value}" for key, value in settings.items()
        ]
        commands += [
            "setting linkgraph.recalc_interval 4",
            "setting linkgraph.recalc_time 16",
            "unpause",
        ]
        scenarios.append(
            {
                "name": f"play-{PLAY_SAVES[0]}-cargodist-{label}-reload",
                "kind": "save",
                "save": str(ROOT / "migration/saves" / f"{PLAY_SAVES[0]}.sav"),
                "console": commands,
                "reload_chunk": "LGRJ",
                "reload_cargos": [0, 2],
                "ticks": years * 365 * TICKS_PER_DAY + SNAPSHOT_TICKS,
            }
        )
    return scenarios


def prepare(scenario, binaries, builds, out, timeout, env, result):
    if "reload_chunk" in scenario:
        # The fixture comes from the unchanged reference only. Outstanding
        # jobs recompute from their saved graph/settings on load in both.
        setup = dict(scenario, ticks=3 * SNAPSHOT_TICKS)
        with core.MACHINE.hold(alone=False):
            prepared = run_game(
                setup,
                binaries["reference"],
                builds["reference"],
                out / scenario["name"] / "prepare",
                timeout,
                env,
                True,
            )
        if prepared["exit"] != 0:
            raise RuntimeError(f"reload preparation exited with {prepared['exit']}")
        chunk = scenario["reload_chunk"]
        fixture = None
        for path in prepared["snapshots"]:
            saved = read_save(path).get(chunk, {})
            cargos = {
                fields["linkgraph[0]/cargo"]
                for _, body in saved.get("elements", [])
                if (fields := decode_element(saved, body)) is not None
            }
            if (
                saved.get("elements")
                and set(scenario.get("reload_cargos", [])) <= cargos
            ):
                fixture = path
                break
        if fixture is None:
            raise RuntimeError(
                f"reload preparation saved no outstanding {chunk} records for required cargo IDs"
            )
        original = read_save(fixture)
        result["reload_records"] = len(original[chunk]["elements"])
        result["reload_cargos"] = sorted(
            {
                fields["linkgraph[0]/cargo"]
                for _, body in original[chunk]["elements"]
                if (fields := decode_element(original[chunk], body)) is not None
            }
        )
        # A reference-produced revision log makes only the candidate append
        # a load-revision event. Reset GLOG history only in these newly
        # prepared road INPUT fixtures (including settings/GRF/cheat history),
        # so both loaders append one. Output GLOG is still compared normally.
        data = fixture.read_bytes()
        begin, end = original["GLOG"]["span"]
        normalized = fixture.parent / "reload-input.sav"
        normalized_data = data[:begin] + data[end:]
        normalized.write_bytes(normalized_data)
        check = read_save(normalized)
        if set(check) != set(original) - {"GLOG"} or any(
            data[slice(*original[cid]["span"])]
            != normalized_data[slice(*check[cid]["span"])]
            for cid in check
        ):
            raise RuntimeError("reload input changed outside optional GLOG history")
        result["reload_input"] = {
            "reference": str(fixture),
            "normalized": str(normalized),
            "reference_sha256": hashlib.sha256(data).hexdigest(),
            "normalized_sha256": hashlib.sha256(normalized_data).hexdigest(),
            "removed_chunk": "GLOG",
        }
        scenario = dict(scenario, save=str(normalized))
    return scenario


PREPARE_ORDER = 3
