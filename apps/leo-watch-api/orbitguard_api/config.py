from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[3]
DATA_DIR = PROJECT_ROOT / "data"
SNAPSHOT_DIR = DATA_DIR / "snapshots"
SNAPSHOT_PATH = SNAPSHOT_DIR / "leo-watch-snapshot.json"

CELESTRAK_ACTIVE_URL = "https://celestrak.org/NORAD/elements/gp.php?GROUP=active&FORMAT=json"
CELESTRAK_DEBRIS_URL = "https://celestrak.org/NORAD/elements/gp.php?NAME=DEB&FORMAT=json"
CELESTRAK_ROCKET_BODY_URL = "https://celestrak.org/NORAD/elements/gp.php?NAME=R/B&FORMAT=json"

PREDICTION_HOURS = 72
COARSE_STEP_SECONDS = 300
REFINE_STEP_SECONDS = 10
REFINE_DISTANCE_KM = 100.0
MAX_EVENTS = 100

EARTH_RADIUS_KM = 6378.137
EARTH_MU_KM3_S2 = 398600.4418
LEO_MIN_PERIGEE_ALTITUDE_KM = 120.0
LEO_MAX_APOGEE_ALTITUDE_KM = 2000.0
