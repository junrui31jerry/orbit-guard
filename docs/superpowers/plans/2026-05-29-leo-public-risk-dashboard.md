# LEO Public Risk Dashboard Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a local OrbitGuard web dashboard that screens public CelesTrak LEO data for 72-hour close-approach candidates and visualizes selected events in 3D.

**Architecture:** Add a new Web-focused app area beside the existing C++ prototype. A Python FastAPI backend fetches CelesTrak public data, stores snapshot fallback data, runs SGP4-based LEO screening, and serves ranked risk events. A React/Three.js frontend displays data-source status, Conservative/Explorer leaderboards, event details, and a 3D Earth view.

**Tech Stack:** Python 3.11+, FastAPI, Pydantic, httpx, sgp4, pytest, React, Vite, TypeScript, Three.js, Vitest.

---

## Scope Check

This plan implements the MVP from `docs/superpowers/specs/2026-05-29-leo-public-risk-dashboard-design.md`.

The current C++/raylib files remain as the legacy prototype. This plan creates new source under `apps/` and shared data under `data/`.

## File Structure

- Create `apps/leo-watch-api/pyproject.toml`: backend package metadata and dependencies.
- Create `apps/leo-watch-api/orbitguard_api/main.py`: FastAPI app factory and API routes.
- Create `apps/leo-watch-api/orbitguard_api/models.py`: Pydantic API/domain models.
- Create `apps/leo-watch-api/orbitguard_api/config.py`: source URLs, thresholds, and runtime paths.
- Create `apps/leo-watch-api/orbitguard_api/celestrak.py`: CelesTrak fetch and snapshot fallback.
- Create `apps/leo-watch-api/orbitguard_api/orbits.py`: OMM-to-SGP4 conversion, LEO filtering, propagation helpers.
- Create `apps/leo-watch-api/orbitguard_api/risk.py`: pair screening, refinement, ranking, and risk labels.
- Create `apps/leo-watch-api/orbitguard_api/store.py`: cached in-memory screening run state.
- Create `apps/leo-watch-api/tests/`: backend tests and fixtures.
- Create `data/snapshots/leo-watch-snapshot.json`: deterministic fallback snapshot used by tests and demos.
- Create `apps/leo-watch-dashboard/package.json`: frontend package metadata and scripts.
- Create `apps/leo-watch-dashboard/src/`: React dashboard, API client, and Three.js scene.
- Modify `.gitignore`: ignore Python/Node build artifacts while keeping snapshots.

## Chosen MVP Constants

- CelesTrak live sources:
  - Active payloads: `https://celestrak.org/NORAD/elements/gp.php?GROUP=active&FORMAT=json`
  - Debris candidates: `https://celestrak.org/NORAD/elements/gp.php?NAME=DEB&FORMAT=json`
  - Rocket body candidates: `https://celestrak.org/NORAD/elements/gp.php?NAME=R/B&FORMAT=json`
- LEO filter:
  - perigee altitude at least `120 km`
  - apogee altitude at most `2000 km`
- Prediction window:
  - `72 hours`
- Coarse screening:
  - `300 second` sample step
- Refinement:
  - refine candidates inside `100 km` coarse miss distance
  - ternary-search around the best coarse sample to approximately `10 second` precision
- Result caps:
  - return top `100` events per mode
- Conservative thresholds:
  - Critical: miss distance `<= 5 km` and TCA `<= 24 h`
  - Warning: miss distance `<= 15 km` and TCA `<= 72 h`
  - Watch: miss distance `<= 30 km` and TCA `<= 72 h`
- Explorer thresholds:
  - Critical: miss distance `<= 10 km` and TCA `<= 24 h`
  - Warning: miss distance `<= 50 km` and TCA `<= 72 h`
  - Watch: miss distance `<= 100 km` and TCA `<= 72 h`

---

### Task 1: Create Backend Skeleton

**Files:**
- Modify: `.gitignore`
- Create: `apps/leo-watch-api/pyproject.toml`
- Create: `apps/leo-watch-api/orbitguard_api/__init__.py`
- Create: `apps/leo-watch-api/orbitguard_api/config.py`
- Create: `apps/leo-watch-api/orbitguard_api/models.py`
- Create: `apps/leo-watch-api/orbitguard_api/main.py`
- Create: `apps/leo-watch-api/tests/test_status.py`

- [ ] **Step 1: Extend `.gitignore` for Web/Python artifacts**

Add:

```gitignore
# Python and Web dashboard artifacts
__pycache__/
*.pyc
.pytest_cache/
.ruff_cache/
.mypy_cache/
.venv/
node_modules/
dist/
coverage/
```

- [ ] **Step 2: Create backend package metadata**

Create `apps/leo-watch-api/pyproject.toml`:

```toml
[project]
name = "orbitguard-leo-watch-api"
version = "0.1.0"
description = "OrbitGuard LEO public close-approach screening API"
requires-python = ">=3.11"
dependencies = [
    "fastapi>=0.115",
    "httpx>=0.27",
    "pydantic>=2.8",
    "sgp4>=2.23",
    "uvicorn>=0.30",
]

[project.optional-dependencies]
dev = [
    "pytest>=8.2",
    "pytest-asyncio>=0.23",
]

[tool.pytest.ini_options]
testpaths = ["tests"]
pythonpath = ["."]
```

- [ ] **Step 3: Create backend package marker**

Create `apps/leo-watch-api/orbitguard_api/__init__.py`:

```python
"""OrbitGuard LEO public risk dashboard API."""
```

- [ ] **Step 4: Create backend config**

Create `apps/leo-watch-api/orbitguard_api/config.py`:

```python
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[3]
DATA_DIR = PROJECT_ROOT / "data"
SNAPSHOT_DIR = DATA_DIR / "snapshots"
SNAPSHOT_PATH = SNAPSHOT_DIR / "leo-watch-snapshot.json"

CELESTRAK_ACTIVE_URL = "https://celestrak.org/NORAD/elements/gp.php?GROUP=active&FORMAT=json"
CELESTRAK_DEBRIS_URL = "https://celestrak.org/NORAD/elements/gp.php?NAME=DEB&FORMAT=json"
CELESTRAK_ROCKET_BODY_URL = "https://celestrak.org/NORAD/elements/gp.php?NAME=R/B&FORMAT=json"

PREDICTION_HOURS = 72
COARSE_STEP_SECONDS = 300
REFINE_STEP_SECONDS = 10
REFINE_DISTANCE_KM = 100.0
MAX_EVENTS = 100

EARTH_RADIUS_KM = 6378.137
EARTH_MU_KM3_S2 = 398600.4418
LEO_MIN_PERIGEE_ALTITUDE_KM = 120.0
LEO_MAX_APOGEE_ALTITUDE_KM = 2000.0
```

- [ ] **Step 5: Create initial API models**

Create `apps/leo-watch-api/orbitguard_api/models.py`:

```python
from datetime import datetime, timezone
from enum import StrEnum

from pydantic import BaseModel, Field


class DataMode(StrEnum):
    LIVE = "live"
    SNAPSHOT = "snapshot"
    NONE = "none"


class RiskMode(StrEnum):
    CONSERVATIVE = "conservative"
    EXPLORER = "explorer"


class RiskLevel(StrEnum):
    CRITICAL = "critical"
    WARNING = "warning"
    WATCH = "watch"


class ApiStatus(BaseModel):
    product: str = "OrbitGuard LEO Watch"
    data_mode: DataMode = DataMode.NONE
    source: str = "CelesTrak public GP data"
    last_updated: datetime | None = None
    prediction_window_hours: int = 72
    active_risk_mode: RiskMode = RiskMode.CONSERVATIVE
    disclaimer: str = "Public-data close-approach screening, not official collision probability."


class ObjectSummary(BaseModel):
    catalog_id: str
    name: str
    object_type: str
    activity_status: str


class RiskEvent(BaseModel):
    event_id: str
    primary_object: ObjectSummary
    secondary_object: ObjectSummary
    tca: datetime
    miss_distance_km: float = Field(ge=0.0)
    relative_velocity_km_s: float = Field(ge=0.0)
    risk_level: RiskLevel
    confidence: str
    risk_mode: RiskMode
    data_mode: DataMode
    source_updated_at: datetime | None = None
    explanation: str


class Summary(BaseModel):
    data_mode: DataMode
    last_updated: datetime | None = None
    objects_screened: int = 0
    active_satellites: int = 0
    inactive_objects: int = 0
    candidates: int = 0
    critical: int = 0
    warning: int = 0
    watch: int = 0


class PositionSample(BaseModel):
    timestamp: datetime
    x_km: float
    y_km: float
    z_km: float


class TrajectoryResponse(BaseModel):
    event_id: str
    tca: datetime
    sample_step_seconds: int
    primary_samples: list[PositionSample]
    secondary_samples: list[PositionSample]
    closest_approach_sample: PositionSample
```

- [ ] **Step 6: Create minimal FastAPI app**

Create `apps/leo-watch-api/orbitguard_api/main.py`:

```python
from fastapi import FastAPI

from orbitguard_api.config import PREDICTION_HOURS
from orbitguard_api.models import ApiStatus


def create_app() -> FastAPI:
    app = FastAPI(title="OrbitGuard LEO Watch API")

    @app.get("/api/status", response_model=ApiStatus)
    def status() -> ApiStatus:
        return ApiStatus(prediction_window_hours=PREDICTION_HOURS)

    return app


app = create_app()
```

- [ ] **Step 7: Write status endpoint test**

Create `apps/leo-watch-api/tests/test_status.py`:

```python
from fastapi.testclient import TestClient

from orbitguard_api.main import create_app


def test_status_endpoint_describes_public_screening():
    client = TestClient(create_app())

    response = client.get("/api/status")

    assert response.status_code == 200
    payload = response.json()
    assert payload["product"] == "OrbitGuard LEO Watch"
    assert payload["data_mode"] == "none"
    assert payload["prediction_window_hours"] == 72
    assert "not official collision probability" in payload["disclaimer"]
```

- [ ] **Step 8: Install backend dependencies**

Run:

```powershell
$PY = 'C:\Users\user\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $PY -m pip install -e .\apps\leo-watch-api[dev]
```

Expected: dependencies install successfully.

- [ ] **Step 9: Run backend skeleton test**

Run:

```powershell
$PY = 'C:\Users\user\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $PY -m pytest apps\leo-watch-api\tests\test_status.py -q
```

Expected: `1 passed`.

- [ ] **Step 10: Commit backend skeleton**

Run:

```powershell
git add .gitignore apps/leo-watch-api
git commit -m "feat: scaffold leo watch api"
```

---

### Task 2: Add Domain Models And Snapshot Fixture

**Files:**
- Modify: `apps/leo-watch-api/orbitguard_api/models.py`
- Create: `data/snapshots/leo-watch-snapshot.json`
- Create: `apps/leo-watch-api/tests/test_models.py`

- [ ] **Step 1: Add raw GP and orbit object models**

Append to `apps/leo-watch-api/orbitguard_api/models.py`:

```python
class ActivityStatus(StrEnum):
    ACTIVE = "active"
    INACTIVE = "inactive"


class OrbitObject(BaseModel):
    catalog_id: str
    name: str
    object_type: str
    activity_status: ActivityStatus
    epoch: datetime
    mean_motion: float
    eccentricity: float
    inclination_deg: float
    raan_deg: float
    arg_perigee_deg: float
    mean_anomaly_deg: float
    bstar: float = 0.0
    mean_motion_dot: float = 0.0
    mean_motion_ddot: float = 0.0
    source: str = "CelesTrak"
    perigee_altitude_km: float | None = None
    apogee_altitude_km: float | None = None
    data_quality_flags: list[str] = Field(default_factory=list)


class ScreeningRun(BaseModel):
    data_mode: DataMode
    source_updated_at: datetime
    objects: list[OrbitObject]
```

- [ ] **Step 2: Create deterministic snapshot fixture**

Create `data/snapshots/leo-watch-snapshot.json`:

```json
{
  "data_mode": "snapshot",
  "source_updated_at": "2026-05-29T00:00:00Z",
  "objects": [
    {
      "catalog_id": "25544",
      "name": "ISS (ZARYA)",
      "object_type": "payload",
      "activity_status": "active",
      "epoch": "2026-05-29T00:00:00Z",
      "mean_motion": 15.49,
      "eccentricity": 0.0004,
      "inclination_deg": 51.64,
      "raan_deg": 20.0,
      "arg_perigee_deg": 80.0,
      "mean_anomaly_deg": 10.0,
      "bstar": 0.0001,
      "mean_motion_dot": 0.0,
      "mean_motion_ddot": 0.0,
      "source": "fixture"
    },
    {
      "catalog_id": "90001",
      "name": "FIXTURE DEB",
      "object_type": "debris",
      "activity_status": "inactive",
      "epoch": "2026-05-29T00:00:00Z",
      "mean_motion": 15.50,
      "eccentricity": 0.0005,
      "inclination_deg": 51.65,
      "raan_deg": 20.1,
      "arg_perigee_deg": 80.0,
      "mean_anomaly_deg": 10.2,
      "bstar": 0.0001,
      "mean_motion_dot": 0.0,
      "mean_motion_ddot": 0.0,
      "source": "fixture"
    }
  ]
}
```

- [ ] **Step 3: Test snapshot model validation**

Create `apps/leo-watch-api/tests/test_models.py`:

```python
import json
from pathlib import Path

from orbitguard_api.models import ActivityStatus, DataMode, ScreeningRun


def test_snapshot_fixture_loads_as_screening_run():
    snapshot_path = Path("data/snapshots/leo-watch-snapshot.json")
    payload = json.loads(snapshot_path.read_text(encoding="utf-8"))

    run = ScreeningRun.model_validate(payload)

    assert run.data_mode == DataMode.SNAPSHOT
    assert len(run.objects) == 2
    assert run.objects[0].activity_status == ActivityStatus.ACTIVE
    assert run.objects[1].activity_status == ActivityStatus.INACTIVE
```

- [ ] **Step 4: Run model tests**

Run:

```powershell
$PY = 'C:\Users\user\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $PY -m pytest apps\leo-watch-api\tests\test_models.py -q
```

Expected: `1 passed`.

- [ ] **Step 5: Commit models and fixture**

Run:

```powershell
git add apps/leo-watch-api/orbitguard_api/models.py apps/leo-watch-api/tests/test_models.py data/snapshots/leo-watch-snapshot.json
git commit -m "feat: add leo watch data models"
```

---

### Task 3: Implement CelesTrak Fetch And Snapshot Fallback

**Files:**
- Create: `apps/leo-watch-api/orbitguard_api/celestrak.py`
- Create: `apps/leo-watch-api/tests/test_celestrak.py`
- Modify: `apps/leo-watch-api/orbitguard_api/main.py`

- [ ] **Step 1: Write fallback tests**

Create `apps/leo-watch-api/tests/test_celestrak.py`:

```python
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
            "MEAN_MOTION_DDOT": 0.0
        }
    ]

    objects = normalize_gp_records(records, activity_status=ActivityStatus.ACTIVE, object_type="payload")

    assert objects[0].catalog_id == "123"
    assert objects[0].activity_status == ActivityStatus.ACTIVE
    assert objects[0].object_type == "payload"


@pytest.mark.asyncio
async def test_fetch_failure_uses_snapshot(tmp_path, monkeypatch):
    snapshot = tmp_path / "snapshot.json"
    snapshot.write_text(json.dumps({
        "data_mode": "snapshot",
        "source_updated_at": "2026-05-29T00:00:00Z",
        "objects": []
    }), encoding="utf-8")

    async def failing_get(*args, **kwargs):
        raise httpx.ConnectError("offline")

    monkeypatch.setattr(httpx.AsyncClient, "get", failing_get)

    run = await fetch_or_load_snapshot(snapshot_path=snapshot)

    assert run.data_mode == DataMode.SNAPSHOT
    assert run.source_updated_at == datetime(2026, 5, 29, tzinfo=timezone.utc)
```

- [ ] **Step 2: Implement CelesTrak client**

Create `apps/leo-watch-api/orbitguard_api/celestrak.py`:

```python
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
        objects.append(OrbitObject(
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
        ))
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
        *normalize_gp_records(rocket_body_records, activity_status=ActivityStatus.INACTIVE, object_type="rocket_body"),
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
```

- [ ] **Step 3: Run CelesTrak tests**

Run:

```powershell
$PY = 'C:\Users\user\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $PY -m pytest apps\leo-watch-api\tests\test_celestrak.py -q
```

Expected: `2 passed`.

- [ ] **Step 4: Commit fetch and fallback**

Run:

```powershell
git add apps/leo-watch-api/orbitguard_api/celestrak.py apps/leo-watch-api/tests/test_celestrak.py
git commit -m "feat: fetch celestrak data with snapshot fallback"
```

---

### Task 4: Implement LEO Filtering And SGP4 Propagation

**Files:**
- Create: `apps/leo-watch-api/orbitguard_api/orbits.py`
- Create: `apps/leo-watch-api/tests/test_orbits.py`

- [ ] **Step 1: Write orbit tests**

Create `apps/leo-watch-api/tests/test_orbits.py`:

```python
from datetime import datetime, timezone, timedelta

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
```

- [ ] **Step 2: Implement orbit helpers**

Create `apps/leo-watch-api/orbitguard_api/orbits.py`:

```python
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
```

- [ ] **Step 3: Run orbit tests**

Run:

```powershell
$PY = 'C:\Users\user\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $PY -m pytest apps\leo-watch-api\tests\test_orbits.py -q
```

Expected: `3 passed`.

- [ ] **Step 4: Commit orbit propagation**

Run:

```powershell
git add apps/leo-watch-api/orbitguard_api/orbits.py apps/leo-watch-api/tests/test_orbits.py
git commit -m "feat: add leo filtering and sgp4 propagation"
```

---

### Task 5: Implement Risk Screening And Ranking

**Files:**
- Create: `apps/leo-watch-api/orbitguard_api/risk.py`
- Create: `apps/leo-watch-api/tests/test_risk.py`

- [ ] **Step 1: Write risk engine tests**

Create `apps/leo-watch-api/tests/test_risk.py`:

```python
from datetime import datetime, timezone, timedelta

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


def test_screen_run_pairs_active_against_inactive(monkeypatch):
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
```

- [ ] **Step 2: Implement risk engine**

Create `apps/leo-watch-api/orbitguard_api/risk.py`:

```python
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


def _relative_velocity_km_s(first_now: PositionSample, first_next: PositionSample, second_now: PositionSample, second_next: PositionSample) -> float:
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
            events.append(RiskEvent(
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
                explanation=f"{primary.name} approaches {secondary.name} within {miss_distance:.1f} km in {hours_to_tca:.1f} hours.",
            ))

    priority = {RiskLevel.CRITICAL: 0, RiskLevel.WARNING: 1, RiskLevel.WATCH: 2}
    events.sort(key=lambda event: (priority[event.risk_level], event.tca, event.miss_distance_km))
    return events[:MAX_EVENTS]
```

- [ ] **Step 3: Run risk tests**

Run:

```powershell
$PY = 'C:\Users\user\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $PY -m pytest apps\leo-watch-api\tests\test_risk.py -q
```

Expected: `3 passed`.

- [ ] **Step 4: Commit risk engine**

Run:

```powershell
git add apps/leo-watch-api/orbitguard_api/risk.py apps/leo-watch-api/tests/test_risk.py
git commit -m "feat: screen leo close approach candidates"
```

---

### Task 6: Serve Screening Results Through FastAPI

**Files:**
- Create: `apps/leo-watch-api/orbitguard_api/store.py`
- Modify: `apps/leo-watch-api/orbitguard_api/main.py`
- Create: `apps/leo-watch-api/tests/test_api_events.py`

- [ ] **Step 1: Add API tests**

Create `apps/leo-watch-api/tests/test_api_events.py`:

```python
from fastapi.testclient import TestClient

from orbitguard_api.main import create_app


def test_summary_endpoint_returns_counts():
    client = TestClient(create_app(load_live_on_start=False))

    response = client.get("/api/summary")

    assert response.status_code == 200
    payload = response.json()
    assert payload["data_mode"] in {"snapshot", "none"}
    assert "objects_screened" in payload


def test_events_endpoint_accepts_risk_mode():
    client = TestClient(create_app(load_live_on_start=False))

    response = client.get("/api/events?mode=explorer")

    assert response.status_code == 200
    assert isinstance(response.json(), list)
```

- [ ] **Step 2: Implement store**

Create `apps/leo-watch-api/orbitguard_api/store.py`:

```python
from orbitguard_api.celestrak import fetch_or_load_snapshot, load_snapshot
from orbitguard_api.models import DataMode, RiskEvent, RiskMode, ScreeningRun, Summary
from orbitguard_api.risk import screen_run


class ScreeningStore:
    def __init__(self) -> None:
        self.run: ScreeningRun | None = None
        self.events_by_mode: dict[RiskMode, list[RiskEvent]] = {
            RiskMode.CONSERVATIVE: [],
            RiskMode.EXPLORER: [],
        }

    def load_snapshot_if_available(self) -> None:
        try:
            self.set_run(load_snapshot())
        except FileNotFoundError:
            self.run = None

    async def refresh(self) -> None:
        self.set_run(await fetch_or_load_snapshot())

    def set_run(self, run: ScreeningRun) -> None:
        self.run = run
        self.events_by_mode = {
            RiskMode.CONSERVATIVE: screen_run(run, mode=RiskMode.CONSERVATIVE, now=run.source_updated_at),
            RiskMode.EXPLORER: screen_run(run, mode=RiskMode.EXPLORER, now=run.source_updated_at),
        }

    def summary(self) -> Summary:
        if self.run is None:
            return Summary(data_mode=DataMode.NONE)

        conservative_events = self.events_by_mode[RiskMode.CONSERVATIVE]
        return Summary(
            data_mode=self.run.data_mode,
            last_updated=self.run.source_updated_at,
            objects_screened=len(self.run.objects),
            active_satellites=sum(1 for obj in self.run.objects if obj.activity_status == "active"),
            inactive_objects=sum(1 for obj in self.run.objects if obj.activity_status == "inactive"),
            candidates=len(conservative_events),
            critical=sum(1 for event in conservative_events if event.risk_level == "critical"),
            warning=sum(1 for event in conservative_events if event.risk_level == "warning"),
            watch=sum(1 for event in conservative_events if event.risk_level == "watch"),
        )

    def events(self, mode: RiskMode) -> list[RiskEvent]:
        return self.events_by_mode[mode]
```

- [ ] **Step 3: Update FastAPI app**

Replace `apps/leo-watch-api/orbitguard_api/main.py` with:

```python
from fastapi import FastAPI

from orbitguard_api.config import PREDICTION_HOURS
from orbitguard_api.models import ApiStatus, RiskEvent, RiskMode, Summary
from orbitguard_api.store import ScreeningStore


def create_app(*, load_live_on_start: bool = False) -> FastAPI:
    app = FastAPI(title="OrbitGuard LEO Watch API")
    store = ScreeningStore()
    store.load_snapshot_if_available()

    @app.on_event("startup")
    async def startup() -> None:
        if load_live_on_start:
            await store.refresh()

    @app.get("/api/status", response_model=ApiStatus)
    def status() -> ApiStatus:
        summary = store.summary()
        return ApiStatus(
            data_mode=summary.data_mode,
            last_updated=summary.last_updated,
            prediction_window_hours=PREDICTION_HOURS,
        )

    @app.get("/api/summary", response_model=Summary)
    def summary() -> Summary:
        return store.summary()

    @app.get("/api/events", response_model=list[RiskEvent])
    def events(mode: RiskMode = RiskMode.CONSERVATIVE) -> list[RiskEvent]:
        return store.events(mode)

    @app.post("/api/refresh", response_model=Summary)
    async def refresh() -> Summary:
        await store.refresh()
        return store.summary()

    return app


app = create_app(load_live_on_start=False)
```

- [ ] **Step 4: Run API tests**

Run:

```powershell
$PY = 'C:\Users\user\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $PY -m pytest apps\leo-watch-api\tests\test_api_events.py apps\leo-watch-api\tests\test_status.py -q
```

Expected: all tests pass.

- [ ] **Step 5: Start API manually**

Run:

```powershell
$PY = 'C:\Users\user\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $PY -m uvicorn orbitguard_api.main:app --app-dir apps\leo-watch-api --reload --port 8000
```

Expected: API starts at `http://127.0.0.1:8000`.

- [ ] **Step 6: Commit API routes**

Run:

```powershell
git add apps/leo-watch-api/orbitguard_api/main.py apps/leo-watch-api/orbitguard_api/store.py apps/leo-watch-api/tests/test_api_events.py
git commit -m "feat: expose leo watch api endpoints"
```

---

### Task 7: Scaffold React Dashboard

**Files:**
- Create: `apps/leo-watch-dashboard/package.json`
- Create: `apps/leo-watch-dashboard/index.html`
- Create: `apps/leo-watch-dashboard/src/main.tsx`
- Create: `apps/leo-watch-dashboard/src/App.tsx`
- Create: `apps/leo-watch-dashboard/src/api.ts`
- Create: `apps/leo-watch-dashboard/src/types.ts`
- Create: `apps/leo-watch-dashboard/src/styles.css`
- Create: `apps/leo-watch-dashboard/src/App.test.tsx`

- [ ] **Step 1: Create frontend package metadata**

Create `apps/leo-watch-dashboard/package.json`:

```json
{
  "name": "orbitguard-leo-watch-dashboard",
  "version": "0.1.0",
  "private": true,
  "type": "module",
  "scripts": {
    "dev": "vite --host 127.0.0.1 --port 3000",
    "build": "tsc && vite build",
    "test": "vitest run"
  },
  "dependencies": {
    "@vitejs/plugin-react": "^4.3.0",
    "vite": "^5.4.0",
    "typescript": "^5.5.0",
    "react": "^18.3.0",
    "react-dom": "^18.3.0",
    "three": "^0.165.0"
  },
  "devDependencies": {
    "@testing-library/jest-dom": "^6.4.0",
    "@testing-library/react": "^15.0.0",
    "@types/react": "^18.3.0",
    "@types/react-dom": "^18.3.0",
    "@types/three": "^0.165.0",
    "jsdom": "^24.1.0",
    "vitest": "^1.6.0"
  }
}
```

- [ ] **Step 2: Create frontend HTML entry**

Create `apps/leo-watch-dashboard/index.html`:

```html
<!doctype html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>OrbitGuard LEO Watch</title>
  </head>
  <body>
    <div id="root"></div>
    <script type="module" src="/src/main.tsx"></script>
  </body>
</html>
```

- [ ] **Step 3: Create frontend types**

Create `apps/leo-watch-dashboard/src/types.ts`:

```typescript
export type DataMode = "live" | "snapshot" | "none";
export type RiskMode = "conservative" | "explorer";
export type RiskLevel = "critical" | "warning" | "watch";

export interface ApiStatus {
  product: string;
  data_mode: DataMode;
  source: string;
  last_updated: string | null;
  prediction_window_hours: number;
  active_risk_mode: RiskMode;
  disclaimer: string;
}

export interface Summary {
  data_mode: DataMode;
  last_updated: string | null;
  objects_screened: number;
  active_satellites: number;
  inactive_objects: number;
  candidates: number;
  critical: number;
  warning: number;
  watch: number;
}

export interface ObjectSummary {
  catalog_id: string;
  name: string;
  object_type: string;
  activity_status: string;
}

export interface RiskEvent {
  event_id: string;
  primary_object: ObjectSummary;
  secondary_object: ObjectSummary;
  tca: string;
  miss_distance_km: number;
  relative_velocity_km_s: number;
  risk_level: RiskLevel;
  confidence: string;
  risk_mode: RiskMode;
  data_mode: DataMode;
  source_updated_at: string | null;
  explanation: string;
}
```

- [ ] **Step 4: Create API client**

Create `apps/leo-watch-dashboard/src/api.ts`:

```typescript
import type { ApiStatus, RiskEvent, RiskMode, Summary } from "./types";

const API_BASE = import.meta.env.VITE_API_BASE ?? "http://127.0.0.1:8000";

async function getJson<T>(path: string): Promise<T> {
  const response = await fetch(`${API_BASE}${path}`);
  if (!response.ok) {
    throw new Error(`API request failed: ${response.status}`);
  }
  return response.json() as Promise<T>;
}

export function fetchStatus(): Promise<ApiStatus> {
  return getJson<ApiStatus>("/api/status");
}

export function fetchSummary(): Promise<Summary> {
  return getJson<Summary>("/api/summary");
}

export function fetchEvents(mode: RiskMode): Promise<RiskEvent[]> {
  return getJson<RiskEvent[]>(`/api/events?mode=${mode}`);
}
```

- [ ] **Step 5: Create dashboard UI**

Create `apps/leo-watch-dashboard/src/App.tsx`:

```tsx
import { useEffect, useState } from "react";

import { fetchEvents, fetchStatus, fetchSummary } from "./api";
import type { ApiStatus, RiskEvent, RiskMode, Summary } from "./types";
import "./styles.css";

function dataModeLabel(mode: string): string {
  if (mode === "live") return "Live CelesTrak data";
  if (mode === "snapshot") return "Snapshot fallback";
  return "No data loaded";
}

export default function App() {
  const [mode, setMode] = useState<RiskMode>("conservative");
  const [status, setStatus] = useState<ApiStatus | null>(null);
  const [summary, setSummary] = useState<Summary | null>(null);
  const [events, setEvents] = useState<RiskEvent[]>([]);
  const [selected, setSelected] = useState<RiskEvent | null>(null);

  useEffect(() => {
    Promise.all([fetchStatus(), fetchSummary(), fetchEvents(mode)]).then(([nextStatus, nextSummary, nextEvents]) => {
      setStatus(nextStatus);
      setSummary(nextSummary);
      setEvents(nextEvents);
      setSelected(nextEvents[0] ?? null);
    });
  }, [mode]);

  return (
    <main className="app-shell">
      <header className="topbar">
        <div>
          <p className="eyebrow">Public LEO close-approach screening</p>
          <h1>OrbitGuard LEO Watch</h1>
        </div>
        <div className={`data-badge ${status?.data_mode ?? "none"}`}>
          {dataModeLabel(status?.data_mode ?? "none")}
        </div>
      </header>

      <section className="notice">
        <span>{status?.prediction_window_hours ?? 72}h prediction window</span>
        <span>Last updated: {status?.last_updated ?? "not loaded"}</span>
        <span>{status?.disclaimer}</span>
      </section>

      <section className="summary-grid">
        <div><strong>{summary?.objects_screened ?? 0}</strong><span>Objects screened</span></div>
        <div><strong>{summary?.critical ?? 0}</strong><span>Critical</span></div>
        <div><strong>{summary?.warning ?? 0}</strong><span>Warning</span></div>
        <div><strong>{summary?.watch ?? 0}</strong><span>Watch</span></div>
      </section>

      <section className="workspace">
        <div className="leaderboard">
          <div className="mode-switch">
            <button className={mode === "conservative" ? "active" : ""} onClick={() => setMode("conservative")}>Conservative</button>
            <button className={mode === "explorer" ? "active" : ""} onClick={() => setMode("explorer")}>Explorer</button>
          </div>
          {events.map((event) => (
            <button key={event.event_id} className={`event-row ${event.risk_level}`} onClick={() => setSelected(event)}>
              <span>{event.primary_object.name}</span>
              <span>{event.secondary_object.name}</span>
              <strong>{event.miss_distance_km.toFixed(1)} km</strong>
            </button>
          ))}
          {events.length === 0 && <p className="empty">No candidates in this mode.</p>}
        </div>

        <div className="event-panel">
          <div className="earth-placeholder">3D Earth view will render here</div>
          {selected && (
            <article>
              <h2>{selected.primary_object.name} vs {selected.secondary_object.name}</h2>
              <p>{selected.explanation}</p>
              <dl>
                <dt>TCA</dt><dd>{selected.tca}</dd>
                <dt>Relative velocity</dt><dd>{selected.relative_velocity_km_s.toFixed(2)} km/s</dd>
                <dt>Confidence</dt><dd>{selected.confidence}</dd>
              </dl>
            </article>
          )}
        </div>
      </section>
    </main>
  );
}
```

- [ ] **Step 6: Create React entry**

Create `apps/leo-watch-dashboard/src/main.tsx`:

```tsx
import React from "react";
import ReactDOM from "react-dom/client";

import App from "./App";

ReactDOM.createRoot(document.getElementById("root")!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>
);
```

- [ ] **Step 7: Create dashboard CSS**

Create `apps/leo-watch-dashboard/src/styles.css`:

```css
* { box-sizing: border-box; }
body { margin: 0; background: #071018; color: #edf7ff; font-family: Segoe UI, system-ui, sans-serif; }
button { font: inherit; }
.app-shell { min-height: 100vh; padding: 28px; background: radial-gradient(circle at 8% 0, #17364c 0, #071018 42%, #05080d 100%); }
.topbar { display: flex; align-items: center; justify-content: space-between; gap: 16px; }
.eyebrow { margin: 0 0 6px; color: #9fb2c5; }
h1 { margin: 0; font-size: 34px; letter-spacing: 0; }
.data-badge { border: 1px solid #244055; border-radius: 6px; padding: 10px 12px; background: #101b27; color: #bfe9ff; }
.data-badge.live { background: #12351f; color: #7ee787; }
.data-badge.snapshot { background: #3b2d0e; color: #ffd166; }
.notice { display: flex; flex-wrap: wrap; gap: 12px; margin-top: 20px; color: #a8b8c7; }
.notice span { border: 1px solid #244055; border-radius: 6px; background: #101b27; padding: 8px 10px; }
.summary-grid { display: grid; grid-template-columns: repeat(4, 1fr); gap: 12px; margin-top: 20px; }
.summary-grid div { border: 1px solid #244055; border-radius: 8px; background: #101b27; padding: 16px; }
.summary-grid strong { display: block; font-size: 26px; }
.summary-grid span { color: #a8b8c7; }
.workspace { display: grid; grid-template-columns: 420px 1fr; gap: 18px; margin-top: 20px; }
.leaderboard, .event-panel { border: 1px solid #244055; border-radius: 8px; background: #101b27; padding: 16px; }
.mode-switch { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; margin-bottom: 12px; }
.mode-switch button, .event-row { border: 1px solid #244055; border-radius: 6px; background: #0b1520; color: #edf7ff; padding: 10px; cursor: pointer; }
.mode-switch button.active { border-color: #56c6ff; color: #bfe9ff; }
.event-row { width: 100%; display: grid; grid-template-columns: 1fr 1fr 90px; gap: 8px; align-items: center; margin-bottom: 8px; text-align: left; }
.event-row.critical { border-color: #ff6b6b; }
.event-row.warning { border-color: #ffd166; }
.event-row.watch { border-color: #7ee787; }
.empty { color: #a8b8c7; }
.earth-placeholder { min-height: 360px; display: flex; align-items: center; justify-content: center; border: 1px solid #244055; border-radius: 8px; background: #08121b; color: #9fb2c5; }
dl { display: grid; grid-template-columns: max-content 1fr; gap: 8px 16px; }
dt { color: #9fb2c5; }
dd { margin: 0; }
@media (max-width: 900px) { .summary-grid, .workspace { grid-template-columns: 1fr; } .topbar { align-items: flex-start; flex-direction: column; } }
```

- [ ] **Step 8: Add smoke UI test**

Create `apps/leo-watch-dashboard/src/App.test.tsx`:

```tsx
import { render, screen } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";

import App from "./App";

vi.mock("./api", () => ({
  fetchStatus: () => Promise.resolve({
    product: "OrbitGuard LEO Watch",
    data_mode: "snapshot",
    source: "CelesTrak public GP data",
    last_updated: "2026-05-29T00:00:00Z",
    prediction_window_hours: 72,
    active_risk_mode: "conservative",
    disclaimer: "Public-data close-approach screening, not official collision probability."
  }),
  fetchSummary: () => Promise.resolve({
    data_mode: "snapshot",
    last_updated: "2026-05-29T00:00:00Z",
    objects_screened: 2,
    active_satellites: 1,
    inactive_objects: 1,
    candidates: 1,
    critical: 0,
    warning: 1,
    watch: 0
  }),
  fetchEvents: () => Promise.resolve([])
}));

describe("App", () => {
  it("renders the public screening disclaimer", async () => {
    render(<App />);

    expect(await screen.findByText(/OrbitGuard LEO Watch/)).toBeInTheDocument();
    expect(await screen.findByText(/not official collision probability/)).toBeInTheDocument();
  });
});
```

- [ ] **Step 9: Install frontend dependencies**

Run:

```powershell
cd apps\leo-watch-dashboard
npm install
```

Expected: dependencies install and `package-lock.json` is created.

- [ ] **Step 10: Run frontend tests**

Run:

```powershell
cd apps\leo-watch-dashboard
npm test
```

Expected: App test passes.

- [ ] **Step 11: Commit frontend scaffold**

Run:

```powershell
git add apps/leo-watch-dashboard
git commit -m "feat: scaffold leo watch dashboard"
```

---

### Task 8: Add Three.js Event Visualization

**Files:**
- Create: `apps/leo-watch-dashboard/src/EarthView.tsx`
- Modify: `apps/leo-watch-dashboard/src/App.tsx`
- Modify: `apps/leo-watch-dashboard/src/styles.css`
- Create: `apps/leo-watch-dashboard/src/EarthView.test.tsx`

- [ ] **Step 1: Create EarthView component**

Create `apps/leo-watch-dashboard/src/EarthView.tsx`:

```tsx
import { useEffect, useRef } from "react";
import * as THREE from "three";

export default function EarthView() {
  const mountRef = useRef<HTMLDivElement | null>(null);

  useEffect(() => {
    if (!mountRef.current) return;

    const width = mountRef.current.clientWidth || 640;
    const height = 360;
    const scene = new THREE.Scene();
    const camera = new THREE.PerspectiveCamera(45, width / height, 0.1, 1000);
    camera.position.set(0, 0, 7);

    const renderer = new THREE.WebGLRenderer({ antialias: true });
    renderer.setSize(width, height);
    renderer.setClearColor(0x08121b);
    mountRef.current.appendChild(renderer.domElement);

    const earth = new THREE.Mesh(
      new THREE.SphereGeometry(1.8, 48, 32),
      new THREE.MeshBasicMaterial({ color: 0x2d74b8, wireframe: false })
    );
    scene.add(earth);

    const leoShell = new THREE.LineLoop(
      new THREE.BufferGeometry().setFromPoints(
        Array.from({ length: 160 }, (_, index) => {
          const angle = (Math.PI * 2 * index) / 160;
          return new THREE.Vector3(Math.cos(angle) * 2.6, Math.sin(angle) * 2.6, 0);
        })
      ),
      new THREE.LineBasicMaterial({ color: 0x56c6ff })
    );
    scene.add(leoShell);

    renderer.render(scene, camera);

    return () => {
      renderer.dispose();
      mountRef.current?.replaceChildren();
    };
  }, []);

  return <div className="earth-view" ref={mountRef} aria-label="3D Earth event view" />;
}
```

- [ ] **Step 2: Use EarthView in App**

In `apps/leo-watch-dashboard/src/App.tsx`, add:

```tsx
import EarthView from "./EarthView";
```

Replace:

```tsx
<div className="earth-placeholder">3D Earth view will render here</div>
```

with:

```tsx
<EarthView />
```

- [ ] **Step 3: Update EarthView CSS**

In `apps/leo-watch-dashboard/src/styles.css`, replace `.earth-placeholder` with:

```css
.earth-view {
  min-height: 360px;
  border: 1px solid #244055;
  border-radius: 8px;
  background: #08121b;
  overflow: hidden;
}
.earth-view canvas {
  display: block;
  width: 100%;
}
```

- [ ] **Step 4: Add EarthView render test**

Create `apps/leo-watch-dashboard/src/EarthView.test.tsx`:

```tsx
import { render, screen } from "@testing-library/react";
import { describe, expect, it } from "vitest";

import EarthView from "./EarthView";

describe("EarthView", () => {
  it("mounts the 3D Earth container", () => {
    render(<EarthView />);

    expect(screen.getByLabelText("3D Earth event view")).toBeInTheDocument();
  });
});
```

- [ ] **Step 5: Run frontend tests and build**

Run:

```powershell
cd apps\leo-watch-dashboard
npm test
npm run build
```

Expected: tests and build pass.

- [ ] **Step 6: Commit EarthView**

Run:

```powershell
git add apps/leo-watch-dashboard/src
git commit -m "feat: add 3d earth event view"
```

---

### Task 9: End-To-End Local Demo Verification

**Files:**
- Create: `docs/leo-watch-local-demo.md`
- Read/Run: backend and frontend apps

- [ ] **Step 1: Create local demo instructions**

Create `docs/leo-watch-local-demo.md`:

```markdown
# OrbitGuard LEO Watch Local Demo

## Backend

```powershell
$PY = 'C:\Users\user\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $PY -m pip install -e .\apps\leo-watch-api[dev]
& $PY -m uvicorn orbitguard_api.main:app --app-dir apps\leo-watch-api --reload --port 8000
```

Open:

- `http://127.0.0.1:8000/api/status`
- `http://127.0.0.1:8000/api/summary`
- `http://127.0.0.1:8000/api/events?mode=conservative`

## Frontend

```powershell
cd apps\leo-watch-dashboard
npm install
npm run dev
```

Open:

- `http://127.0.0.1:3000`

## Expected Demo Behavior

- Header says `OrbitGuard LEO Watch`.
- Data badge says either `Live CelesTrak data` or `Snapshot fallback`.
- Disclaimer says results are not official collision probability.
- Summary strip shows screened object and candidate counts.
- Conservative/Explorer buttons switch candidate lists.
- 3D Earth view renders a non-empty Earth scene.
```

- [ ] **Step 2: Run backend full test suite**

Run:

```powershell
$PY = 'C:\Users\user\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $PY -m pytest apps\leo-watch-api\tests -q
```

Expected: all backend tests pass.

- [ ] **Step 3: Run frontend test and build**

Run:

```powershell
cd apps\leo-watch-dashboard
npm test
npm run build
```

Expected: tests and build pass.

- [ ] **Step 4: Start backend**

Run:

```powershell
$PY = 'C:\Users\user\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
Start-Process -WindowStyle Hidden -FilePath $PY -ArgumentList @('-m','uvicorn','orbitguard_api.main:app','--app-dir','apps\leo-watch-api','--port','8000')
```

Expected: backend serves `http://127.0.0.1:8000/api/status`.

- [ ] **Step 5: Start frontend**

Run:

```powershell
cd apps\leo-watch-dashboard
npm run dev
```

Expected: Vite serves `http://127.0.0.1:3000`.

- [ ] **Step 6: Manual browser verification**

Open `http://127.0.0.1:3000`.

Expected:

- Product name is visible.
- Data status badge is visible.
- Official-Pc disclaimer is visible.
- Summary cards render.
- Leaderboard renders or shows a clear empty state.
- 3D Earth view is non-empty.
- Conservative/Explorer mode buttons are visible.

- [ ] **Step 7: Commit demo docs**

Run:

```powershell
git add docs/leo-watch-local-demo.md
git commit -m "docs: add leo watch local demo guide"
```

---

## Self-Review

- Spec coverage: The plan covers LEO-only CelesTrak public data, live plus snapshot data states, 72-hour screening, active-vs-inactive pairing, Conservative/Explorer modes, leaderboard, 3D visualization, disclaimer, API endpoints, tests, and local demo docs.
- Placeholder scan: No red-flag placeholder terms, incomplete steps, or vague deferred-work instructions remain.
- Type consistency: `DataMode`, `RiskMode`, `RiskLevel`, `OrbitObject`, `RiskEvent`, `Summary`, and `TrajectoryResponse` are introduced before use and reused consistently.
- Known risk: The exact `Satrec.sgp4init` field mapping can be finicky. Task 4 includes tests that must pass before later risk tasks proceed. If the library rejects this mapping, replace `_satrec_from_object` with `sgp4.omm.initialize` using the same `OrbitObject` fields and keep the public function signature unchanged.
