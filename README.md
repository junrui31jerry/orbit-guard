# OrbitGuard

OrbitGuard is a space-risk visualization project that evolved from an interactive simulator into a more realistic low Earth orbit monitoring demo.

The project has two connected stages:

- **Version 1: OrbitGuard Simulator** - a C++ / raylib prototype for visualizing satellites, orbit layers, Earth warning events, avoidance movement, and risk prediction ideas.
- **Version 2: LEO Watch Dashboard** - a web-based monitoring dashboard that screens public orbital data for close-approach candidates and displays the results with an API, ranked event list, and 3D Earth view.

This progression is the main idea of the project: start with a visual prototype that explains the concept, then upgrade it into a data-driven system that is closer to a real satellite monitoring workflow.

## Version 1: Simulator Prototype

The first version focuses on showing the concept of orbital risk through an interactive local application.

It includes:

- C++17 simulation code.
- raylib-based visual rendering.
- A main menu and multiple simulator modes.
- Player satellite state and navigation behavior.
- Earth event warnings and impact-style event flow.
- Risk prediction logic for future close approaches.
- Smoke tests for simulation, orbit, mission, and risk behavior.

This version is useful for explaining the idea visually: a user can see satellites, warning states, and avoidance concepts instead of only reading data tables.

### Run Version 1

On Windows with Embarcadero Dev-C++ / TDM-GCC and the bundled raylib files:

```powershell
.\compile_orbit_guard.bat
```

If compilation succeeds, run:

```powershell
.\OrbitGuard.exe
```

## Version 2: LEO Watch Dashboard

The second version upgrades the project into a public-data screening dashboard.

It includes:

- FastAPI backend under `apps/leo-watch-api`.
- CelesTrak public GP data fetching with snapshot fallback.
- Orbit object modeling and LEO filtering.
- SGP4-based propagation helpers.
- 72-hour close-approach screening.
- Conservative and Explorer risk modes.
- React / Vite frontend under `apps/leo-watch-dashboard`.
- Three.js 3D Earth event visualization.
- Local demo guide in `docs/leo-watch-local-demo.md`.

This version is designed to feel more like a real monitoring tool. It does not claim to produce official collision probability. It is a public-data close-approach screening demo.

### Run Version 2 Backend

```powershell
$PY = 'C:\Users\user\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $PY -m pip install -e .\apps\leo-watch-api[dev]
& $PY -m uvicorn orbitguard_api.main:app --app-dir apps\leo-watch-api --reload --port 8000
```

Useful API endpoints:

- `http://127.0.0.1:8000/api/status`
- `http://127.0.0.1:8000/api/summary`
- `http://127.0.0.1:8000/api/events?mode=conservative`
- `http://127.0.0.1:8000/api/events?mode=explorer`

### Run Version 2 Frontend

```powershell
cd apps\leo-watch-dashboard
npm install
npm run dev
```

Open:

```text
http://127.0.0.1:3000
```

Expected behavior:

- The page title is `OrbitGuard LEO Watch`.
- The data badge shows either live CelesTrak data or snapshot fallback.
- The dashboard states that the results are not official collision probability.
- Conservative mode shows stricter risk filtering.
- Explorer mode shows broader close-approach candidates.
- The 3D Earth view renders a non-empty scene.

## Project Structure

```text
apps/
  leo-watch-api/          FastAPI backend for public LEO screening
  leo-watch-dashboard/    React, Vite, and Three.js dashboard
data/
  snapshots/              Deterministic fallback data for demos and tests
docs/
  leo-watch-local-demo.md Local backend/frontend demo instructions
include/                  C++ simulator headers
src/                      C++ simulator implementation
tests/                    C++ smoke tests
```

## Verification

The project was locally verified with:

- Backend pytest: 13 passed.
- Frontend Vitest: 2 passed.
- Frontend production build: passed.
- Local browser demo: verified.

The local demo confirmed that the dashboard loads, the 3D Earth canvas renders, and the Conservative / Explorer mode switch works.

## Learning Reflection

OrbitGuard shows a project moving through two levels of maturity.

The first version answers: **Can the idea be visualized and understood?**

The second version answers: **Can the idea be connected to public orbital data and presented as a monitoring workflow?**

That evolution is the core of the assignment. The project begins as a concept simulator and grows into a more realistic LEO risk dashboard while keeping both versions visible in the repository history.
