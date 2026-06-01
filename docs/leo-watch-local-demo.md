# OrbitGuard LEO Watch Local Demo

## Backend

```powershell
$PY = 'C:\Users\user\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $PY -m pip install -e .\apps\leo-watch-api[dev]
& $PY -m uvicorn orbitguard_api.main:app --app-dir apps\leo-watch-api --reload --port 8000
```

Open:

- `http://127.0.0.1:8000/api/status`
- `http://127.0.0.1:8000/api/summary`
- `http://127.0.0.1:8000/api/events?mode=conservative`

## Frontend

```powershell
cd apps\leo-watch-dashboard
npm install
npm run dev
```

Open:

- `http://127.0.0.1:3000`

## Expected Demo Behavior

- Header says `OrbitGuard LEO Watch`.
- Data badge says either `Live CelesTrak data` or `Snapshot fallback`.
- Disclaimer says results are not official collision probability.
- Summary strip shows screened object and candidate counts.
- Conservative/Explorer buttons switch candidate lists.
- 3D Earth view renders a non-empty Earth scene.
