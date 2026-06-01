from datetime import datetime
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


class ActivityStatus(StrEnum):
    ACTIVE = "active"
    INACTIVE = "inactive"


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
