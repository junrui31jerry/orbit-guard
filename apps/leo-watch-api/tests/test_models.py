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
