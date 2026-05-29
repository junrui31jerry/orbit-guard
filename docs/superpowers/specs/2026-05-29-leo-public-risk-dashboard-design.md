# OrbitGuard LEO Public Risk Dashboard Design

## Goal

Reposition OrbitGuard as a public satellite-data risk monitoring and visualization tool:

> OrbitGuard helps the public, students, and researchers understand which real low Earth orbit satellites are approaching potentially risky conjunction candidates, using open satellite data and clear visual explanations.

The first version is not an official collision-probability system. It is a public close-approach screening dashboard. It should make real-world LEO conjunction candidates understandable while clearly communicating data limits.

## Product Direction

The selected direction is the C-path:

- Start with a useful public-data dashboard.
- Add 3D Earth visualization for intuitive understanding.
- Design the architecture so a future probability-of-collision module can be added when covariance, hard body radius, or CDM data is available.

The first implementation should be a local web dashboard, not a continuation of the current C++/raylib game prototype.

## Scope

### In Scope For MVP

- LEO-only monitoring.
- CelesTrak public data as the first data source.
- Live data fetch by default.
- Snapshot fallback when live fetch fails.
- A visible data-source state on screen:
  - `Live CelesTrak data`
  - `Snapshot fallback`
  - last updated time
  - 72-hour prediction window
  - "not official collision probability" disclaimer
- 72-hour future screening window.
- Main pairing strategy:
  - active LEO satellites vs inactive LEO objects
  - inactive objects include debris and rocket bodies where source metadata allows classification
- Conservative and Explorer risk modes:
  - Conservative is the default public-facing mode.
  - Explorer surfaces more low-confidence candidates for learning and research.
- Risk event leaderboard with:
  - active satellite name and catalog ID
  - inactive object name and catalog ID
  - object types
  - TCA
  - miss distance
  - relative velocity
  - risk level
  - prediction confidence
  - data source state
- 3D Earth view:
  - visualizes the selected event
  - shows the two object trajectories around TCA
  - highlights closest approach point
- Clear disclaimer that results are public-data screening candidates and do not replace official operator alerts.

### Out Of Scope For MVP

- Official probability of collision.
- Space-Track login or private operator data.
- CDM ingestion.
- Covariance-based Pc calculation.
- Maneuver recommendation.
- Full all-vs-all LEO object screening as the main UI.
- All orbit regimes beyond LEO.
- Real-time notification or alerting service.
- Long-term historical trends.

## Architecture

Use a Python API plus React/Three.js dashboard.

### Data Layer

The data layer fetches CelesTrak public satellite data and stores the latest successful snapshot.

Responsibilities:

- Fetch current public orbital data from CelesTrak.
- Separate live-data failures from parsing or validation failures.
- Persist a known-good snapshot for demos and offline fallback.
- Attach source metadata to every run:
  - source name
  - live or snapshot mode
  - fetch time
  - source epoch range
  - object count

### Risk Engine

The Python risk engine performs LEO filtering, orbital propagation, close-approach screening, and event ranking.

Responsibilities:

- Normalize incoming orbital data into one internal object model.
- Classify active satellite vs inactive object when metadata supports it.
- Filter objects to LEO.
- Propagate object states over 72 hours using SGP4.
- Coarse-screen candidate pairs before detailed closest-approach calculation.
- Refine candidate TCA and miss distance for shortlisted pairs.
- Compute relative velocity at closest approach.
- Assign risk levels based on the selected risk mode.
- Emit deterministic, schema-stable risk events.

### API Layer

FastAPI exposes the computed data to the dashboard.

Initial endpoints:

- `GET /api/status`
  - data mode, last updated time, object counts, prediction window, active risk mode
- `GET /api/summary`
  - total objects screened, total candidates, counts by risk level
- `GET /api/events?mode=conservative|explorer`
  - ranked close-approach candidates
- `GET /api/events/{event_id}`
  - selected event detail
- `GET /api/events/{event_id}/trajectory`
  - position samples for both objects around TCA for 3D rendering
- `POST /api/refresh`
  - request a live data refresh, falling back to snapshot on failure

### UI Layer

React renders the risk dashboard, while Three.js renders the 3D Earth scene.

Primary UI regions:

- Header:
  - product name
  - data-source badge
  - last updated time
  - prediction window
  - official-Pc disclaimer
- Summary strip:
  - objects screened
  - active satellites
  - inactive objects
  - Watch / Warning / Critical counts
- Risk leaderboard:
  - sortable event rows
  - risk level
  - TCA countdown
  - miss distance
  - relative velocity
  - confidence indicator
- Event detail panel:
  - selected pair names and catalog IDs
  - event geometry
  - source epoch and freshness
  - why the event is ranked
- 3D Earth view:
  - Earth
  - LEO shell context
  - selected object trajectories
  - closest approach marker

## Risk Model

The MVP uses close-approach screening, not official Pc.

Each event receives:

- `miss_distance_km`
- `tca`
- `relative_velocity_km_s`
- `source_age_hours`
- `risk_level`
- `confidence`
- `mode`

Risk mode behavior:

- Conservative mode should minimize false alarm framing and only elevate close, near-term candidates.
- Explorer mode should include more candidates and clearly label the output as broader, lower-confidence screening.

Risk labels:

- `Critical`: near-term and very close candidate.
- `Warning`: close enough to warrant attention inside the 72-hour window.
- `Watch`: candidate worth tracking but not strongly framed as dangerous.

Exact thresholds should be implemented as configuration, not hard-coded into UI components.

## Future Pc Module

The design leaves room for a second-layer probability module.

The module can calculate Pc only when sufficient inputs exist:

- miss-distance vector at TCA
- encounter covariance
- hard body radius
- relative encounter geometry

If real covariance or CDM data is unavailable, the system may support an educational scenario mode using assumed covariance, but the UI must label that output as assumed-scenario Pc, not official Pc.

## Data Model

### Orbit Object

Fields:

- `catalog_id`
- `name`
- `object_type`
- `activity_status`
- `source`
- `epoch`
- `tle` or equivalent orbital fields
- `leo_classification`
- `data_quality_flags`

### Risk Event

Fields:

- `event_id`
- `primary_object`
- `secondary_object`
- `tca`
- `miss_distance_km`
- `relative_velocity_km_s`
- `risk_level`
- `confidence`
- `risk_mode`
- `data_mode`
- `source_updated_at`
- `explanation`

### Trajectory Response

Fields:

- `event_id`
- `tca`
- `sample_step_seconds`
- `primary_samples`
- `secondary_samples`
- `closest_approach_sample`

Each sample includes timestamp and Earth-centered position.

## Error Handling

- If live fetch fails and snapshot exists:
  - use snapshot
  - show `Snapshot fallback`
  - show snapshot timestamp
  - keep dashboard usable
- If live fetch fails and no snapshot exists:
  - show a clear no-data state
  - do not fabricate sample risk events
- If parsing fails:
  - report source parsing error
  - keep previous valid snapshot if available
- If propagation fails for individual objects:
  - exclude those objects from screening
  - count them in data-quality warnings
- If API refresh is running:
  - show refresh state without blocking existing results

## Testing Strategy

Backend:

- Deterministic tests using a fixed CelesTrak snapshot fixture.
- LEO filtering tests.
- Object classification tests.
- SGP4 propagation sanity tests.
- Pair screening tests with known artificial close approaches.
- Conservative vs Explorer threshold tests.
- Snapshot fallback tests.
- API schema tests.

Frontend:

- Data-source badge renders live and snapshot states.
- Leaderboard renders risk events and empty states.
- Risk mode switch updates displayed events.
- Selected event updates detail panel.
- Three.js view renders non-empty Earth and trajectory scene.
- Disclaimer remains visible.

End-to-end:

- Start API and dashboard with a fixed snapshot.
- Verify summary counts.
- Select an event.
- Verify event detail and trajectory visualization load.

## Migration From Current Prototype

The current C++/raylib project should be treated as a visual and conceptual prototype, not the production base for the new MVP.

Reusable ideas:

- Earth-centered 3D view.
- Risk panel concept.
- Selected event focus.
- Warning levels.
- Public-facing educational framing.

Not carried forward directly:

- Game modes.
- PlayerSat launch flow.
- Fictional debris events.
- Manual avoidance gameplay.
- Simplified two-body physics as the main real-data engine.

## Success Criteria

The MVP is successful when a user can:

1. Open the local dashboard.
2. See whether the data is live or snapshot fallback.
3. See how many LEO objects were screened.
4. View a ranked list of real close-approach candidates for the next 72 hours.
5. Switch between Conservative and Explorer modes.
6. Click a real event and understand it visually in 3D.
7. Understand that the result is public-data screening, not official collision probability.

## Open Implementation Choices

These choices should be finalized during implementation planning:

- Exact CelesTrak endpoint and source grouping.
- Exact LEO altitude or orbital-period cutoff.
- Initial conservative and explorer thresholds.
- Coarse-screen step size and refinement method.
- Snapshot file format and update cadence.
- Frontend framework scaffold and package choices.
