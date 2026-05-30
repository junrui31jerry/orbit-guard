from datetime import datetime, timedelta, timezone

from orbitguard_api.models import ActivityStatus, OrbitObject
from orbitguard_api.orbits import altitude_bounds_km, is_leo_object, propagate_object


def make_object(mean_motion: float, eccentricity: float = 0.0004) -> OrbitObject:
    return OrbitObject(
        catalog_id="1",
        name="TEST",
        object_type="payload",
        activity_status=ActivityStatus.ACTIVE,
        epoch=datetime(2026, 5, 29, tzinfo=timezone.utc),
        mean_motion=mean_motion,
        eccentricity=eccentricity,
        inclination_deg=51.6,
        raan_deg=20.0,
        arg_perigee_deg=80.0,
        mean_anomaly_deg=10.0,
    )


def test_altitude_bounds_classify_leo():
    obj = make_object(mean_motion=15.5)
    perigee, apogee = altitude_bounds_km(obj)

    assert perigee < 2000
    assert apogee < 2000
    assert is_leo_object(obj)


def test_altitude_bounds_reject_high_orbit():
    obj = make_object(mean_motion=2.0)

    assert not is_leo_object(obj)


def test_propagate_object_returns_position_vector():
    obj = make_object(mean_motion=15.5)
    timestamp = obj.epoch + timedelta(minutes=10)

    sample = propagate_object(obj, timestamp)

    assert sample.timestamp == timestamp
    assert abs(sample.x_km) > 1
    assert abs(sample.y_km) > 1
    assert abs(sample.z_km) > 1
