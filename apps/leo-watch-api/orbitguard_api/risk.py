import math
from datetime import datetime, timedelta, timezone

from orbitguard_api.config import COARSE_STEP_SECONDS, MAX_EVENTS, PREDICTION_HOURS
from orbitguard_api.models import (
    ActivityStatus,
    ObjectSummary,
    PositionSample,
    RiskEvent,
    RiskLevel,
    RiskMode,
    ScreeningRun,
)
from orbitguard_api.orbits import is_leo_object, propagate_object


def distance_km(first: PositionSample, second: PositionSample) -> float:
    return math.sqrt(
        (first.x_km - second.x_km) ** 2
        + (first.y_km - second.y_km) ** 2
        + (first.z_km - second.z_km) ** 2
    )


def classify_risk(miss_distance_km: float, hours_to_tca: float, mode: RiskMode) -> RiskLevel | None:
    if mode == RiskMode.CONSERVATIVE:
        if miss_distance_km <= 5.0 and hours_to_tca <= 24.0:
            return RiskLevel.CRITICAL
        if miss_distance_km <= 15.0 and hours_to_tca <= 72.0:
            return RiskLevel.WARNING
        if miss_distance_km <= 30.0 and hours_to_tca <= 72.0:
            return RiskLevel.WATCH
        return None

    if miss_distance_km <= 10.0 and hours_to_tca <= 24.0:
        return RiskLevel.CRITICAL
    if miss_distance_km <= 50.0 and hours_to_tca <= 72.0:
        return RiskLevel.WARNING
    if miss_distance_km <= 100.0 and hours_to_tca <= 72.0:
        return RiskLevel.WATCH
    return None


def _summary(obj) -> ObjectSummary:
    return ObjectSummary(
        catalog_id=obj.catalog_id,
        name=obj.name,
        object_type=obj.object_type,
        activity_status=obj.activity_status.value,
    )


def _confidence(source_updated_at: datetime, tca: datetime) -> str:
    age_hours = max(0.0, (tca - source_updated_at).total_seconds() / 3600.0)
    if age_hours <= 24.0:
        return "high"
    if age_hours <= 48.0:
        return "medium"
    return "low"


def _relative_velocity_km_s(
    first_now: PositionSample,
    first_next: PositionSample,
    second_now: PositionSample,
    second_next: PositionSample,
) -> float:
    dt = max(1.0, (first_next.timestamp - first_now.timestamp).total_seconds())
    vx = (first_next.x_km - first_now.x_km - (second_next.x_km - second_now.x_km)) / dt
    vy = (first_next.y_km - first_now.y_km - (second_next.y_km - second_now.y_km)) / dt
    vz = (first_next.z_km - first_now.z_km - (second_next.z_km - second_now.z_km)) / dt
    return math.sqrt(vx * vx + vy * vy + vz * vz)


def screen_run(
    run: ScreeningRun,
    *,
    mode: RiskMode,
    now: datetime | None = None,
    coarse_step_seconds: int = COARSE_STEP_SECONDS,
) -> list[RiskEvent]:
    start = now or datetime.now(timezone.utc)
    timestamps = [
        start + timedelta(seconds=seconds)
        for seconds in range(0, PREDICTION_HOURS * 3600 + 1, coarse_step_seconds)
    ]

    active = [obj for obj in run.objects if obj.activity_status == ActivityStatus.ACTIVE and is_leo_object(obj)]
    inactive = [obj for obj in run.objects if obj.activity_status == ActivityStatus.INACTIVE and is_leo_object(obj)]

    events: list[RiskEvent] = []
    for primary in active:
        primary_samples = [propagate_object(primary, ts) for ts in timestamps]
        for secondary in inactive:
            secondary_samples = [propagate_object(secondary, ts) for ts in timestamps]
            best_index = min(
                range(len(timestamps)),
                key=lambda index: distance_km(primary_samples[index], secondary_samples[index]),
            )
            miss_distance = distance_km(primary_samples[best_index], secondary_samples[best_index])
            tca = timestamps[best_index]
            hours_to_tca = max(0.0, (tca - start).total_seconds() / 3600.0)
            risk_level = classify_risk(miss_distance, hours_to_tca, mode)
            if risk_level is None:
                continue

            next_index = min(best_index + 1, len(timestamps) - 1)
            relative_velocity = _relative_velocity_km_s(
                primary_samples[best_index],
                primary_samples[next_index],
                secondary_samples[best_index],
                secondary_samples[next_index],
            )
            events.append(
                RiskEvent(
                    event_id=f"{primary.catalog_id}-{secondary.catalog_id}-{int(tca.timestamp())}-{mode.value}",
                    primary_object=_summary(primary),
                    secondary_object=_summary(secondary),
                    tca=tca,
                    miss_distance_km=miss_distance,
                    relative_velocity_km_s=relative_velocity,
                    risk_level=risk_level,
                    confidence=_confidence(run.source_updated_at, tca),
                    risk_mode=mode,
                    data_mode=run.data_mode,
                    source_updated_at=run.source_updated_at,
                    explanation=(
                        f"{primary.name} approaches {secondary.name} within "
                        f"{miss_distance:.1f} km in {hours_to_tca:.1f} hours."
                    ),
                )
            )

    priority = {RiskLevel.CRITICAL: 0, RiskLevel.WARNING: 1, RiskLevel.WATCH: 2}
    events.sort(key=lambda event: (priority[event.risk_level], event.tca, event.miss_distance_km))
    return events[:MAX_EVENTS]
