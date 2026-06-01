import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import httpx

from orbitguard_api.config import (
    CELESTRAK_ACTIVE_URL,
    CELESTRAK_DEBRIS_URL,
    CELESTRAK_ROCKET_BODY_URL,
    SNAPSHOT_PATH,
)
from orbitguard_api.models import ActivityStatus, DataMode, OrbitObject, ScreeningRun


def _parse_epoch(value: str) -> datetime:
    normalized = value.replace("Z", "+00:00")
    parsed = datetime.fromisoformat(normalized)
    if parsed.tzinfo is None:
        return parsed.replace(tzinfo=timezone.utc)
    return parsed.astimezone(timezone.utc)


def normalize_gp_records(
    records: list[dict[str, Any]],
    *,
    activity_status: ActivityStatus,
    object_type: str,
) -> list[OrbitObject]:
    objects: list[OrbitObject] = []
    for record in records:
        objects.append(
            OrbitObject(
                catalog_id=str(record["NORAD_CAT_ID"]),
                name=str(record["OBJECT_NAME"]).strip(),
                object_type=object_type,
                activity_status=activity_status,
                epoch=_parse_epoch(str(record["EPOCH"])),
                mean_motion=float(record["MEAN_MOTION"]),
                eccentricity=float(record["ECCENTRICITY"]),
                inclination_deg=float(record["INCLINATION"]),
                raan_deg=float(record["RA_OF_ASC_NODE"]),
                arg_perigee_deg=float(record["ARG_OF_PERICENTER"]),
                mean_anomaly_deg=float(record["MEAN_ANOMALY"]),
                bstar=float(record.get("BSTAR") or 0.0),
                mean_motion_dot=float(record.get("MEAN_MOTION_DOT") or 0.0),
                mean_motion_ddot=float(record.get("MEAN_MOTION_DDOT") or 0.0),
            )
        )
    return objects


async def _fetch_json(client: httpx.AsyncClient, url: str) -> list[dict[str, Any]]:
    response = await client.get(url, timeout=30.0)
    response.raise_for_status()
    payload = response.json()
    if not isinstance(payload, list):
        raise ValueError(f"CelesTrak response was not a list for {url}")
    return payload


async def fetch_live_objects() -> ScreeningRun:
    async with httpx.AsyncClient() as client:
        active_records = await _fetch_json(client, CELESTRAK_ACTIVE_URL)
        debris_records = await _fetch_json(client, CELESTRAK_DEBRIS_URL)
        rocket_body_records = await _fetch_json(client, CELESTRAK_ROCKET_BODY_URL)

    objects = [
        *normalize_gp_records(active_records, activity_status=ActivityStatus.ACTIVE, object_type="payload"),
        *normalize_gp_records(debris_records, activity_status=ActivityStatus.INACTIVE, object_type="debris"),
        *normalize_gp_records(
            rocket_body_records,
            activity_status=ActivityStatus.INACTIVE,
            object_type="rocket_body",
        ),
    ]
    return ScreeningRun(data_mode=DataMode.LIVE, source_updated_at=datetime.now(timezone.utc), objects=objects)


def load_snapshot(snapshot_path: Path = SNAPSHOT_PATH) -> ScreeningRun:
    return ScreeningRun.model_validate_json(snapshot_path.read_text(encoding="utf-8"))


def save_snapshot(run: ScreeningRun, snapshot_path: Path = SNAPSHOT_PATH) -> None:
    snapshot_path.parent.mkdir(parents=True, exist_ok=True)
    snapshot_path.write_text(run.model_dump_json(indent=2), encoding="utf-8")


async def fetch_or_load_snapshot(snapshot_path: Path = SNAPSHOT_PATH) -> ScreeningRun:
    try:
        run = await fetch_live_objects()
        save_snapshot(run, snapshot_path)
        return run
    except Exception:
        if snapshot_path.exists():
            snapshot = load_snapshot(snapshot_path)
            return snapshot.model_copy(update={"data_mode": DataMode.SNAPSHOT})
        raise
