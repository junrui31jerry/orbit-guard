from datetime import datetime, timezone

from orbitguard_api.models import (
    ActivityStatus,
    DataMode,
    OrbitObject,
    PositionSample,
    RiskLevel,
    RiskMode,
    ScreeningRun,
)
from orbitguard_api.risk import classify_risk, distance_km, screen_run


def test_distance_km_uses_3d_distance():
    first = PositionSample(timestamp=datetime.now(timezone.utc), x_km=0, y_km=0, z_km=0)
    second = PositionSample(timestamp=first.timestamp, x_km=3, y_km=4, z_km=12)

    assert distance_km(first, second) == 13


def test_classify_risk_uses_mode_thresholds():
    assert classify_risk(4.0, 10.0, RiskMode.CONSERVATIVE) == RiskLevel.CRITICAL
    assert classify_risk(40.0, 48.0, RiskMode.CONSERVATIVE) is None
    assert classify_risk(40.0, 48.0, RiskMode.EXPLORER) == RiskLevel.WARNING


def make_object(catalog_id: str, active: bool, mean_anomaly_deg: float) -> OrbitObject:
    return OrbitObject(
        catalog_id=catalog_id,
        name=f"OBJECT-{catalog_id}",
        object_type="payload" if active else "debris",
        activity_status=ActivityStatus.ACTIVE if active else ActivityStatus.INACTIVE,
        epoch=datetime(2026, 5, 29, tzinfo=timezone.utc),
        mean_motion=15.5,
        eccentricity=0.0004,
        inclination_deg=51.6,
        raan_deg=20.0,
        arg_perigee_deg=80.0,
        mean_anomaly_deg=mean_anomaly_deg,
    )


def test_screen_run_pairs_active_against_inactive():
    run = ScreeningRun(
        data_mode=DataMode.SNAPSHOT,
        source_updated_at=datetime(2026, 5, 29, tzinfo=timezone.utc),
        objects=[
            make_object("100", True, 10.0),
            make_object("200", False, 10.1),
            make_object("300", True, 90.0),
        ],
    )

    events = screen_run(run, mode=RiskMode.EXPLORER, now=run.source_updated_at, coarse_step_seconds=1800)

    assert events
    assert events[0].primary_object.activity_status == "active"
    assert events[0].secondary_object.activity_status == "inactive"
    assert events[0].data_mode == DataMode.SNAPSHOT
