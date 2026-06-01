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
