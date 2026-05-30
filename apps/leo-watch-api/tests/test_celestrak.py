import json
from datetime import datetime, timezone

import httpx
import pytest

from orbitguard_api.celestrak import fetch_or_load_snapshot, normalize_gp_records
from orbitguard_api.models import ActivityStatus, DataMode


def test_normalize_gp_records_marks_activity_and_type():
    records = [
        {
            "NORAD_CAT_ID": 123,
            "OBJECT_NAME": "ACTIVE SAT",
            "EPOCH": "2026-05-29T00:00:00.000000",
            "MEAN_MOTION": 15.1,
            "ECCENTRICITY": 0.001,
            "INCLINATION": 53.0,
            "RA_OF_ASC_NODE": 1.0,
            "ARG_OF_PERICENTER": 2.0,
            "MEAN_ANOMALY": 3.0,
            "BSTAR": 0.0001,
            "MEAN_MOTION_DOT": 0.0,
            "MEAN_MOTION_DDOT": 0.0,
        }
    ]

    objects = normalize_gp_records(records, activity_status=ActivityStatus.ACTIVE, object_type="payload")

    assert objects[0].catalog_id == "123"
    assert objects[0].activity_status == ActivityStatus.ACTIVE
    assert objects[0].object_type == "payload"


@pytest.mark.asyncio
async def test_fetch_failure_uses_snapshot(tmp_path, monkeypatch):
    snapshot = tmp_path / "snapshot.json"
    snapshot.write_text(
        json.dumps(
            {
                "data_mode": "snapshot",
                "source_updated_at": "2026-05-29T00:00:00Z",
                "objects": [],
            }
        ),
        encoding="utf-8",
    )

    async def failing_get(*args, **kwargs):
        raise httpx.ConnectError("offline")

    monkeypatch.setattr(httpx.AsyncClient, "get", failing_get)

    run = await fetch_or_load_snapshot(snapshot_path=snapshot)

    assert run.data_mode == DataMode.SNAPSHOT
    assert run.source_updated_at == datetime(2026, 5, 29, tzinfo=timezone.utc)
