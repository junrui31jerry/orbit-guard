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
