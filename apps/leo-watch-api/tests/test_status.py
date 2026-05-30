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
