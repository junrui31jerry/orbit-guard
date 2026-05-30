from contextlib import asynccontextmanager

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from orbitguard_api.config import PREDICTION_HOURS
from orbitguard_api.models import ApiStatus, RiskEvent, RiskMode, Summary
from orbitguard_api.store import ScreeningStore


def create_app(*, load_live_on_start: bool = False) -> FastAPI:
    store = ScreeningStore()
    store.load_snapshot_if_available()

    @asynccontextmanager
    async def lifespan(_app: FastAPI):
        if load_live_on_start:
            await store.refresh()
        yield

    app = FastAPI(title="OrbitGuard LEO Watch API", lifespan=lifespan)
    app.add_middleware(
        CORSMiddleware,
        allow_origins=["http://127.0.0.1:3000", "http://localhost:3000"],
        allow_credentials=False,
        allow_methods=["*"],
        allow_headers=["*"],
    )

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
