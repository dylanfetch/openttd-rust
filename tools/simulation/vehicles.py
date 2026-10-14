"""Base-vehicle scheduler observations on the existing fixed Opus replay (#213)."""

from . import core

OPUS = "play-opus-55-167-002-manual"


def scenarios(soak):
    # Reuse the existing replay and its exact endpoints; do not duplicate it in CI.
    return []


def game_environment(scenario):
    witnessed = scenario.get("name") == OPUS and scenario.get("witnesses")
    return core.WITNESS_ENVIRONMENT if witnessed else {}


def check(scenario, run, mode, role, result):
    if scenario.get("name") != OPUS:
        return
    counts = core.branch_witnesses(
        scenario,
        run,
        role,
        "vehicle",
        (
            "visits.road",
            "primary.road",
            "scheduler_crossings",
            "observed_bytes",
            "day_visits",
            "station_visits",
            "motion_updates",
            "cargo_addresses",
            "cargo_owner",
            "motion_addresses",
            "status",
            "running_sound",
            "periodic_sound",
            "descriptor_bytes",
            "scalar_read_bytes",
            "scalar_write_bytes",
        ),
    )
    if counts is None:
        return
    visits = sum(
        counts[f"visits.{kind}"]
        for kind in ("train", "road", "ship", "aircraft", "effect", "disaster")
    )
    result[f"{mode}_{role}_vehicle_scheduler"] = {
        "outer_visits": visits,
        "counts": counts,
        "crossings_per_outer_visit": counts["scheduler_crossings"] / visits,
        "observed_bytes_per_outer_visit": counts["observed_bytes"] / visits,
        "descriptor_bytes_per_outer_visit": counts["descriptor_bytes"] / visits,
        "scalar_read_bytes_per_outer_visit": counts["scalar_read_bytes"] / visits,
        "scalar_write_bytes_per_outer_visit": counts["scalar_write_bytes"] / visits,
    }
