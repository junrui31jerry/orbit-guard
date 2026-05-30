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
