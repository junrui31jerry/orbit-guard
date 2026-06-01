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
