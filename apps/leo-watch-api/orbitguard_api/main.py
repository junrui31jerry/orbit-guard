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
