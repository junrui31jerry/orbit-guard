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
    Promise.all([fetchStatus(), fetchSummary(), fetchEvents(mode)]).then(
      ([nextStatus, nextSummary, nextEvents]) => {
        setStatus(nextStatus);
        setSummary(nextSummary);
        setEvents(nextEvents);
        setSelected(nextEvents[0] ?? null);
      },
    );
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

      <section className="notice" aria-label="Data source status">
        <span>{status?.prediction_window_hours ?? 72}h prediction window</span>
        <span>Last updated: {status?.last_updated ?? "not loaded"}</span>
        <span>{status?.disclaimer}</span>
      </section>

      <section className="summary-grid" aria-label="Screening summary">
        <div>
          <strong>{summary?.objects_screened ?? 0}</strong>
          <span>Objects screened</span>
        </div>
        <div>
          <strong>{summary?.critical ?? 0}</strong>
          <span>Critical</span>
        </div>
        <div>
          <strong>{summary?.warning ?? 0}</strong>
          <span>Warning</span>
        </div>
        <div>
          <strong>{summary?.watch ?? 0}</strong>
          <span>Watch</span>
        </div>
      </section>

      <section className="workspace">
        <div className="leaderboard">
          <div className="mode-switch" role="group" aria-label="Risk mode">
            <button className={mode === "conservative" ? "active" : ""} onClick={() => setMode("conservative")}>
              Conservative
            </button>
            <button className={mode === "explorer" ? "active" : ""} onClick={() => setMode("explorer")}>
              Explorer
            </button>
          </div>
          {events.map((event) => (
            <button
              key={event.event_id}
              className={`event-row ${event.risk_level}`}
              onClick={() => setSelected(event)}
            >
              <span>{event.primary_object.name}</span>
              <span>{event.secondary_object.name}</span>
              <strong>{event.miss_distance_km.toFixed(1)} km</strong>
            </button>
          ))}
          {events.length === 0 && <p className="empty">No candidates in this mode.</p>}
        </div>

        <div className="event-panel">
          <div className="earth-placeholder" aria-label="3D Earth event view" />
          {selected && (
            <article>
              <h2>
                {selected.primary_object.name} vs {selected.secondary_object.name}
              </h2>
              <p>{selected.explanation}</p>
              <dl>
                <dt>TCA</dt>
                <dd>{selected.tca}</dd>
                <dt>Relative velocity</dt>
                <dd>{selected.relative_velocity_km_s.toFixed(2)} km/s</dd>
                <dt>Confidence</dt>
                <dd>{selected.confidence}</dd>
              </dl>
            </article>
          )}
        </div>
      </section>
    </main>
  );
}
