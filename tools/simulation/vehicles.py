"""Shared scheduler evidence over existing transport/fleet fixtures (#213)."""

from . import core

OPUS = "play-opus-55-167-002-manual"
PREFIXES = (
    "play-",
    "roads-",
    "rail-",
    "water-ferry-",
    "aircraft-",
    "effects-",
    "fleet-",
)


def scenarios(soak):
    # Reuse existing scenarios and their exact endpoints, without extra games.
    return []


def selected(scenario):
    return scenario.get("name", "").startswith(PREFIXES)


def game_environment(scenario):
    return (
        core.WITNESS_ENVIRONMENT
        if selected(scenario) and scenario.get("witnesses")
        else {}
    )


def check(scenario, run, mode, role, result):
    if not selected(scenario):
        return
    required = ["outer_visits", "crossings", "observed_bytes", "scalar_read_bytes"]
    if scenario.get("name") == OPUS:
        required += [
            "visits.road",
            "days",
            "stations",
            "motion",
            "post_tick",
            "cargo_owner",
            "packet_lookups",
            "running_sound",
            "periodic_sound",
            "descriptor_bytes",
            "scalar_write_bytes",
        ]
    counts = core.branch_witnesses(scenario, run, role, "vehicle", required)
    if counts is None:
        return
    visits = counts["outer_visits"]
    # Post-Tick type is sampled only after survival; deleted visits remain in
    # the actual outer denominator without dereferencing a destroyed shell.
    surviving = sum(
        counts[f"visits.{kind}"]
        for kind in (
            "train",
            "road",
            "ship",
            "aircraft",
            "effect",
            "disaster",
            "end",
            "invalid",
        )
    )
    if surviving + counts["deleted"] != visits:
        raise RuntimeError("scheduler survival accounting")
    result[f"{mode}_{role}_vehicle_scheduler"] = {
        "outer_visits": visits,
        "counts": counts,
        "crossings_per_outer_visit": counts["crossings"] / visits,
        "observed_bytes_per_outer_visit": counts["observed_bytes"] / visits,
        "scalar_read_bytes_per_outer_visit": counts["scalar_read_bytes"] / visits,
        "scalar_write_bytes_per_outer_visit": counts["scalar_write_bytes"] / visits,
    }
