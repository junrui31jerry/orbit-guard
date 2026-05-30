import math
from datetime import datetime

from sgp4.api import Satrec, WGS72, jday

from orbitguard_api.config import (
    EARTH_MU_KM3_S2,
    EARTH_RADIUS_KM,
    LEO_MAX_APOGEE_ALTITUDE_KM,
    LEO_MIN_PERIGEE_ALTITUDE_KM,
)
from orbitguard_api.models import OrbitObject, PositionSample


def altitude_bounds_km(obj: OrbitObject) -> tuple[float, float]:
    mean_motion_rad_s = obj.mean_motion * 2.0 * math.pi / 86400.0
    semi_major_axis_km = (EARTH_MU_KM3_S2 / (mean_motion_rad_s * mean_motion_rad_s)) ** (1.0 / 3.0)
    perigee_km = semi_major_axis_km * (1.0 - obj.eccentricity) - EARTH_RADIUS_KM
    apogee_km = semi_major_axis_km * (1.0 + obj.eccentricity) - EARTH_RADIUS_KM
    return perigee_km, apogee_km


def is_leo_object(obj: OrbitObject) -> bool:
    perigee_km, apogee_km = altitude_bounds_km(obj)
    return perigee_km >= LEO_MIN_PERIGEE_ALTITUDE_KM and apogee_km <= LEO_MAX_APOGEE_ALTITUDE_KM


def _satrec_from_object(obj: OrbitObject) -> Satrec:
    sat = Satrec()
    epoch = obj.epoch
    jd, fr = jday(
        epoch.year,
        epoch.month,
        epoch.day,
        epoch.hour,
        epoch.minute,
        epoch.second + epoch.microsecond / 1_000_000.0,
    )
    sat.sgp4init(
        WGS72,
        "i",
        int(obj.catalog_id),
        jd + fr - 2433281.5,
        obj.bstar,
        obj.mean_motion_dot,
        obj.mean_motion_ddot,
        obj.eccentricity,
        math.radians(obj.arg_perigee_deg),
        math.radians(obj.inclination_deg),
        math.radians(obj.mean_anomaly_deg),
        obj.mean_motion * 2.0 * math.pi / 1440.0,
        math.radians(obj.raan_deg),
    )
    return sat


def propagate_object(obj: OrbitObject, timestamp: datetime) -> PositionSample:
    sat = _satrec_from_object(obj)
    jd, fr = jday(
        timestamp.year,
        timestamp.month,
        timestamp.day,
        timestamp.hour,
        timestamp.minute,
        timestamp.second + timestamp.microsecond / 1_000_000.0,
    )
    error, position, _velocity = sat.sgp4(jd, fr)
    if error != 0:
        raise ValueError(f"SGP4 propagation failed for {obj.catalog_id} with error {error}")
    return PositionSample(timestamp=timestamp, x_km=position[0], y_km=position[1], z_km=position[2])
