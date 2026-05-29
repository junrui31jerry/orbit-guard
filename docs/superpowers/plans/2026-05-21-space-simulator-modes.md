# Space Simulator Modes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first playable three-mode OrbitGuard experience with a full Earth space simulator upgrade plus playable Solar System and Black Hole ship prototypes.

**Architecture:** Add a `GameMode` layer above the existing simulation loop, keep Earth orbit logic in the existing `OrbitObject` model, and add small focused state helpers for orbit layers, one active event, avoidance animation, scanning, and free-flight ship modes. Rendering remains raylib immediate-mode drawing, with mode-specific update/draw functions called from `main.cpp`.

**Tech Stack:** C++17, raylib 6.0, Dev-C++/TDM-GCC, existing `compile_orbit_guard.bat`, existing smoke-test style C++ executables, Git for Windows at `C:\Program Files\Git\cmd\git.exe`.

---

## Scope Check

The approved spec contains three simulator modes. This plan implements the first playable milestone:

- Main menu and mode switching.
- Earth space simulator as the complete mode: single `PlayerSat`, LEO/MEO/GEO mission, one active warning event, unknown scan, avoidance animation, Option A background.
- Solar System and Black Hole as playable prototypes: direct ship control and core scene interaction.

Future items such as ship skins, full planet encyclopedia content, and a full post-black-hole world remain outside this milestone.

## File Structure

- Modify `include/orbit_guard.h`: add mode enums, orbit layer helpers, Earth event state, avoidance animation state, scan state, ship state, and new draw/update declarations.
- Modify `src/simulation.cpp`: add orbit layer classification, `PlayerSat` upsert helpers, and reusable background helpers.
- Create `src/earth_events.cpp`: own Earth mission event state, active warning transitions, scan resolution, and avoidance animation math.
- Create `src/ship_modes.cpp`: own simple 3D ship movement plus Solar System and Black Hole scene state helpers.
- Modify `src/risk.cpp`: keep risk analysis compatible with one `PlayerSat` and active unknown objects.
- Modify `src/ui.cpp`: add main menu, warning banners, mission panel changes, selected-only target frame colors, and Option A background drawing.
- Modify `main.cpp`: split the loop by `GameMode`, route input/update/draw to each mode, and preserve existing Earth simulation behavior.
- Modify `compile_orbit_guard.bat`, `Makefile.win`, and `Project orbit guard.dev`: include new `.cpp` files.
- Add tests:
  - `tests/orbit_layer_smoke.cpp`
  - `tests/player_satellite_smoke.cpp`
  - `tests/earth_event_smoke.cpp`
  - `tests/ship_state_smoke.cpp`

---

### Task 1: Add Core Mode and Earth State Types

**Files:**
- Modify: `include/orbit_guard.h`
- Test: `tests/orbit_layer_smoke.cpp`

- [ ] **Step 1: Add the failing orbit-layer smoke test**

Create `tests/orbit_layer_smoke.cpp`:

```cpp
#include "orbit_guard.h"

#include <iostream>

using namespace OrbitGuard;

bool Expect(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

int main()
{
    bool ok = true;
    ok = Expect(ClassifyOrbitLayer(100.0f) == OrbitLayer::LEO, "100 is LEO") && ok;
    ok = Expect(ClassifyOrbitLayer(165.0f) == OrbitLayer::MEO, "165 is MEO") && ok;
    ok = Expect(ClassifyOrbitLayer(260.0f) == OrbitLayer::GEO, "260 is GEO") && ok;
    ok = Expect(ClassifyOrbitLayer(70.0f) == OrbitLayer::BelowOperational, "70 is below operational") && ok;
    ok = Expect(ClassifyOrbitLayer(350.0f) == OrbitLayer::BeyondOperational, "350 is beyond operational") && ok;
    ok = Expect(std::string(OrbitLayerText(OrbitLayer::MEO)) == "MEO", "MEO label") && ok;
    ok = Expect(IsOrbitLayerMatch(OrbitLayer::MEO, 146.0f), "MEO lower bound matches") && ok;
    ok = Expect(IsOrbitLayerMatch(OrbitLayer::MEO, 230.0f), "MEO upper bound matches") && ok;
    ok = Expect(!IsOrbitLayerMatch(OrbitLayer::MEO, 231.0f), "MEO rejects GEO radius") && ok;
    return ok ? 0 : 1;
}
```

- [ ] **Step 2: Run the test and verify it fails**

Run:

```powershell
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' `
  tests\orbit_layer_smoke.cpp `
  src\simulation.cpp `
  -std=c++17 `
  -I include `
  -I raylib\raylib-6.0_win64_mingw-w64\include `
  -o tests\orbit_layer_smoke.exe

& .\tests\orbit_layer_smoke.exe
```

Expected before implementation: compile fails because `OrbitLayer`, `ClassifyOrbitLayer`, `OrbitLayerText`, and `IsOrbitLayerMatch` do not exist.

- [ ] **Step 3: Add declarations and state types**

In `include/orbit_guard.h`, add after `enum class RiskLevel`:

```cpp
enum class GameMode
{
    MainMenu,
    EarthSpace,
    SolarSystem,
    BlackHole
};

enum class OrbitLayer
{
    BelowOperational,
    LEO,
    MEO,
    GEO,
    BeyondOperational
};

enum class ImmediateEventType
{
    None,
    CollisionWarning,
    UnknownObject
};

enum class ImmediateEventPhase
{
    Inactive,
    WaitingForPlayer,
    Animating,
    Resolved
};
```

Add after `struct MissionState`:

```cpp
struct EarthMissionState
{
    OrbitLayer targetLayer = OrbitLayer::MEO;
    bool playerSatDeployed = false;
    bool deploymentComplete = false;
    std::string title = "Deploy PlayerSat to MEO orbit";
    std::string description = "Ground stations need a medium-orbit relay. Adjust Orbit Radius into MEO, then press L.";
    std::string nextAction = "Adjust Orbit Radius into MEO, then press L to deploy PlayerSat.";
};

struct ImmediateEventState
{
    ImmediateEventType type = ImmediateEventType::None;
    ImmediateEventPhase phase = ImmediateEventPhase::Inactive;
    int targetIndex = -1;
    float timer = 0.0f;
    std::string title;
    std::string detail;
    std::string action;
    std::string result;
};

struct AvoidanceAnimationState
{
    bool active = false;
    int objectIndex = -1;
    float elapsed = 0.0f;
    float duration = 1.8f;
    OrbitObject startObject = {};
    OrbitObject endObject = {};
    float beforeDistance = 0.0f;
    float afterDistance = 0.0f;
};

struct UnknownScanState
{
    bool objectActive = false;
    bool scanning = false;
    bool identified = false;
    int objectIndex = -1;
    float progress = 0.0f;
    ObjectType revealedType = ObjectType::Debris;
};

struct ShipState
{
    Vector3 position = {0.0f, 0.0f, 260.0f};
    float yaw = 0.0f;
    float speed = 0.0f;
};
```

Add declarations near the existing simulation declarations:

```cpp
OrbitLayer ClassifyOrbitLayer(float orbitRadius);
const char *OrbitLayerText(OrbitLayer layer);
bool IsOrbitLayerMatch(OrbitLayer layer, float orbitRadius);
Color OrbitLayerColor(OrbitLayer layer);
```

- [ ] **Step 4: Implement orbit-layer helpers**

In `src/simulation.cpp`, add after `CalculateOrbitPosition`:

```cpp
OrbitLayer ClassifyOrbitLayer(float orbitRadius)
{
    if (orbitRadius < 85.0f)
    {
        return OrbitLayer::BelowOperational;
    }
    if (orbitRadius <= 145.0f)
    {
        return OrbitLayer::LEO;
    }
    if (orbitRadius <= 230.0f)
    {
        return OrbitLayer::MEO;
    }
    if (orbitRadius <= 330.0f)
    {
        return OrbitLayer::GEO;
    }
    return OrbitLayer::BeyondOperational;
}

const char *OrbitLayerText(OrbitLayer layer)
{
    switch (layer)
    {
    case OrbitLayer::LEO:
        return "LEO";
    case OrbitLayer::MEO:
        return "MEO";
    case OrbitLayer::GEO:
        return "GEO";
    case OrbitLayer::BelowOperational:
        return "Below operational orbit";
    case OrbitLayer::BeyondOperational:
        return "Beyond operational orbit";
    default:
        return "Unknown orbit";
    }
}

bool IsOrbitLayerMatch(OrbitLayer layer, float orbitRadius)
{
    return ClassifyOrbitLayer(orbitRadius) == layer;
}

Color OrbitLayerColor(OrbitLayer layer)
{
    switch (layer)
    {
    case OrbitLayer::LEO:
        return SKYBLUE;
    case OrbitLayer::MEO:
        return GOLD;
    case OrbitLayer::GEO:
        return VIOLET;
    default:
        return Fade(RAYWHITE, 0.45f);
    }
}
```

- [ ] **Step 5: Run the orbit-layer smoke test**

Run the command from Step 2 again.

Expected: compile succeeds, executable exits with code `0`.

- [ ] **Step 6: Commit**

Run:

```powershell
& 'C:\Program Files\Git\cmd\git.exe' add include\orbit_guard.h src\simulation.cpp tests\orbit_layer_smoke.cpp
& 'C:\Program Files\Git\cmd\git.exe' commit -m "feat: add simulator mode and orbit layer state"
```

Expected: commit succeeds.

---

### Task 2: Convert Launching to a Single PlayerSat

**Files:**
- Modify: `include/orbit_guard.h`
- Modify: `src/simulation.cpp`
- Modify: `main.cpp`
- Test: `tests/player_satellite_smoke.cpp`

- [ ] **Step 1: Add the failing PlayerSat smoke test**

Create `tests/player_satellite_smoke.cpp`:

```cpp
#include "orbit_guard.h"

#include <iostream>
#include <vector>

using namespace OrbitGuard;

bool Expect(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

int main()
{
    bool ok = true;
    std::vector<OrbitObject> objects = CreateDemoObjects();
    LaunchSettings settings;
    settings.orbitRadius = 165.0f;
    settings.inclinationDeg = 12.0f;
    settings.angularSpeed = 0.48f;
    settings.initialAngleDeg = 24.0f;

    int firstIndex = UpsertPlayerSatellite(objects, settings);
    ok = Expect(firstIndex >= 0, "first PlayerSat index exists") && ok;
    ok = Expect(objects[firstIndex].name == "PlayerSat", "player satellite has stable name") && ok;
    ok = Expect(objects[firstIndex].userControlled, "PlayerSat is user controlled") && ok;

    const int countAfterFirst = CountPlayerSatellites(objects);
    settings.orbitRadius = 260.0f;
    int secondIndex = UpsertPlayerSatellite(objects, settings);

    ok = Expect(firstIndex == secondIndex, "upsert reuses same PlayerSat slot") && ok;
    ok = Expect(CountPlayerSatellites(objects) == countAfterFirst, "second launch does not add another PlayerSat") && ok;
    ok = Expect(objects[secondIndex].orbitRadius == 260.0f, "second launch updates PlayerSat orbit radius") && ok;
    return ok ? 0 : 1;
}
```

- [ ] **Step 2: Run the test and verify it fails**

Run:

```powershell
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' `
  tests\player_satellite_smoke.cpp `
  src\simulation.cpp `
  -std=c++17 `
  -I include `
  -I raylib\raylib-6.0_win64_mingw-w64\include `
  -o tests\player_satellite_smoke.exe
```

Expected before implementation: compile fails because `UpsertPlayerSatellite` and `CountPlayerSatellites` do not exist.

- [ ] **Step 3: Add declarations**

In `include/orbit_guard.h`, add after `OrbitObject CreateUserSatellite(LaunchSettings &settings);`:

```cpp
OrbitObject CreatePlayerSatellite(const LaunchSettings &settings);
int FindPlayerSatelliteIndex(const std::vector<OrbitObject> &objects);
int CountPlayerSatellites(const std::vector<OrbitObject> &objects);
int UpsertPlayerSatellite(std::vector<OrbitObject> &objects, const LaunchSettings &settings);
```

- [ ] **Step 4: Implement PlayerSat helpers**

In `src/simulation.cpp`, add after `CreateUserSatellite`:

```cpp
OrbitObject CreatePlayerSatellite(const LaunchSettings &settings)
{
    OrbitObject object;
    object.name = "PlayerSat";
    object.type = ObjectType::Satellite;
    object.orbitRadius = settings.orbitRadius;
    object.inclinationDeg = settings.inclinationDeg;
    object.angularSpeed = settings.angularSpeed;
    object.initialAngleDeg = settings.initialAngleDeg;
    object.angleRad = settings.initialAngleDeg * kDegToRad;
    object.position = CalculateOrbitPosition(object.orbitRadius, object.inclinationDeg, object.angleRad);
    object.color = YELLOW;
    object.userControlled = true;
    return object;
}

int FindPlayerSatelliteIndex(const std::vector<OrbitObject> &objects)
{
    for (int i = 0; i < static_cast<int>(objects.size()); ++i)
    {
        if (objects[i].userControlled && objects[i].name == "PlayerSat")
        {
            return i;
        }
    }
    return -1;
}

int CountPlayerSatellites(const std::vector<OrbitObject> &objects)
{
    int count = 0;
    for (const OrbitObject &object : objects)
    {
        if (object.userControlled && object.name == "PlayerSat")
        {
            count++;
        }
    }
    return count;
}

int UpsertPlayerSatellite(std::vector<OrbitObject> &objects, const LaunchSettings &settings)
{
    const int existingIndex = FindPlayerSatelliteIndex(objects);
    const OrbitObject playerSat = CreatePlayerSatellite(settings);
    if (existingIndex >= 0)
    {
        objects[existingIndex] = playerSat;
        return existingIndex;
    }

    objects.push_back(playerSat);
    return static_cast<int>(objects.size()) - 1;
}
```

- [ ] **Step 5: Change launch handling in `main.cpp`**

In the `KEY_L` branch, replace:

```cpp
objects.push_back(CreateUserSatellite(launchSettings));
selectedObjectIndex = static_cast<int>(objects.size()) - 1;
MarkMissionLaunched(missionState);
exportMessageTimer = 0.0f;
actionMessage = objects.back().name + " launched into the simulation.";
actionMessageTimer = 2.4f;
```

with:

```cpp
selectedObjectIndex = UpsertPlayerSatellite(objects, launchSettings);
MarkMissionLaunched(missionState);
exportMessageTimer = 0.0f;
actionMessage = "PlayerSat deployed into " + std::string(OrbitLayerText(ClassifyOrbitLayer(launchSettings.orbitRadius))) + ".";
actionMessageTimer = 2.4f;
```

- [ ] **Step 6: Change remove handling**

In the `KEY_BACKSPACE` branch, replace the reverse loop with:

```cpp
const int playerIndex = FindPlayerSatelliteIndex(objects);
if (playerIndex >= 0)
{
    actionMessage = "PlayerSat removed.";
    objects.erase(objects.begin() + playerIndex);
    selectedObjectIndex = -1;
    missionState.hasReviewedRisk = false;
    missionState.hasAppliedAvoidance = false;
    missionState.hasSavedMissionReport = false;
    exportMessageTimer = 0.0f;
    actionMessageTimer = 2.4f;
}
```

- [ ] **Step 7: Run PlayerSat test**

Run:

```powershell
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' `
  tests\player_satellite_smoke.cpp `
  src\simulation.cpp `
  -std=c++17 `
  -I include `
  -I raylib\raylib-6.0_win64_mingw-w64\include `
  -o tests\player_satellite_smoke.exe

& .\tests\player_satellite_smoke.exe
```

Expected: exits with code `0`.

- [ ] **Step 8: Commit**

Run:

```powershell
& 'C:\Program Files\Git\cmd\git.exe' add include\orbit_guard.h src\simulation.cpp main.cpp tests\player_satellite_smoke.cpp
& 'C:\Program Files\Git\cmd\git.exe' commit -m "feat: use a single PlayerSat"
```

---

### Task 3: Add Earth Mission and Immediate Event Core

**Files:**
- Modify: `include/orbit_guard.h`
- Create: `src/earth_events.cpp`
- Test: `tests/earth_event_smoke.cpp`

- [ ] **Step 1: Add the failing event smoke test**

Create `tests/earth_event_smoke.cpp`:

```cpp
#include "orbit_guard.h"

#include <iostream>
#include <vector>

using namespace OrbitGuard;

bool Expect(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

int main()
{
    bool ok = true;
    EarthMissionState mission;
    LaunchSettings settings;
    settings.orbitRadius = 165.0f;

    UpdateEarthMissionBeforeLaunch(mission, settings);
    ok = Expect(mission.targetLayer == OrbitLayer::MEO, "default target is MEO") && ok;
    ok = Expect(mission.deploymentComplete == false, "mission is incomplete before launch") && ok;

    std::vector<OrbitObject> objects;
    objects.push_back(CreatePlayerSatellite(settings));
    UpdateEarthMissionAfterLaunch(mission, objects);
    ok = Expect(mission.playerSatDeployed, "PlayerSat deployment is tracked") && ok;
    ok = Expect(mission.deploymentComplete, "MEO deployment completes default mission") && ok;

    ImmediateEventState event;
    StartCollisionWarning(event, 0, "Debris-A17");
    ok = Expect(event.type == ImmediateEventType::CollisionWarning, "collision event type") && ok;
    ok = Expect(event.phase == ImmediateEventPhase::WaitingForPlayer, "collision waits for player") && ok;
    ok = Expect(event.action.find("Press A") != std::string::npos, "collision action mentions A") && ok;

    UnknownScanState scan;
    StartUnknownObjectEvent(event, scan, 1);
    ok = Expect(event.type == ImmediateEventType::UnknownObject, "unknown event type") && ok;
    ok = Expect(scan.objectActive, "unknown object is active") && ok;
    ok = Expect(scan.objectIndex == 1, "unknown index is stored") && ok;

    BeginUnknownScan(event, scan);
    ok = Expect(scan.scanning, "scan starts") && ok;
    UpdateUnknownScan(event, scan, 2.0f);
    ok = Expect(scan.identified, "scan finishes after enough time") && ok;
    ok = Expect(event.phase == ImmediateEventPhase::Resolved, "event resolves after scan") && ok;
    return ok ? 0 : 1;
}
```

- [ ] **Step 2: Add declarations**

In `include/orbit_guard.h`, add after `const char *MissionStepStatusText(...)` declarations:

```cpp
void UpdateEarthMissionBeforeLaunch(EarthMissionState &mission, const LaunchSettings &settings);
void UpdateEarthMissionAfterLaunch(EarthMissionState &mission, const std::vector<OrbitObject> &objects);
void ResetImmediateEvent(ImmediateEventState &event);
void StartCollisionWarning(ImmediateEventState &event, int targetIndex, const std::string &targetName);
void StartUnknownObjectEvent(ImmediateEventState &event, UnknownScanState &scan, int objectIndex);
bool CanStartUnknownScan(const ImmediateEventState &event, const UnknownScanState &scan, int selectedObjectIndex);
void BeginUnknownScan(ImmediateEventState &event, UnknownScanState &scan);
void UpdateUnknownScan(ImmediateEventState &event, UnknownScanState &scan, float deltaTime);
```

- [ ] **Step 3: Implement `src/earth_events.cpp`**

Create `src/earth_events.cpp`:

```cpp
#include "orbit_guard.h"

namespace OrbitGuard
{
namespace
{
constexpr float kScanDuration = 1.6f;
}

void UpdateEarthMissionBeforeLaunch(EarthMissionState &mission, const LaunchSettings &settings)
{
    const OrbitLayer currentLayer = ClassifyOrbitLayer(settings.orbitRadius);
    if (currentLayer == mission.targetLayer)
    {
        mission.nextAction = "Orbit matches " + std::string(OrbitLayerText(mission.targetLayer)) + ". Press L to deploy PlayerSat.";
    }
    else
    {
        mission.nextAction = "Adjust Orbit Radius into " + std::string(OrbitLayerText(mission.targetLayer)) + ", then press L.";
    }
}

void UpdateEarthMissionAfterLaunch(EarthMissionState &mission, const std::vector<OrbitObject> &objects)
{
    const int playerIndex = FindPlayerSatelliteIndex(objects);
    mission.playerSatDeployed = playerIndex >= 0;
    mission.deploymentComplete = mission.playerSatDeployed && IsOrbitLayerMatch(mission.targetLayer, objects[playerIndex].orbitRadius);
    mission.nextAction = mission.deploymentComplete
                             ? "Deployment complete. Monitor PlayerSat for warnings."
                             : "PlayerSat deployed outside target layer. Adjust and redeploy.";
}

void ResetImmediateEvent(ImmediateEventState &event)
{
    event = ImmediateEventState{};
}

void StartCollisionWarning(ImmediateEventState &event, int targetIndex, const std::string &targetName)
{
    event.type = ImmediateEventType::CollisionWarning;
    event.phase = ImmediateEventPhase::WaitingForPlayer;
    event.targetIndex = targetIndex;
    event.timer = 0.0f;
    event.title = "COLLISION WARNING";
    event.detail = targetName + " is approaching PlayerSat";
    event.action = "Press A to perform avoidance burn";
    event.result.clear();
}

void StartUnknownObjectEvent(ImmediateEventState &event, UnknownScanState &scan, int objectIndex)
{
    event.type = ImmediateEventType::UnknownObject;
    event.phase = ImmediateEventPhase::WaitingForPlayer;
    event.targetIndex = objectIndex;
    event.timer = 0.0f;
    event.title = "UNKNOWN OBJECT DETECTED";
    event.detail = "Unknown-01 entered the monitoring zone";
    event.action = "Select Unknown-01 and press C to scan";
    event.result.clear();

    scan.objectActive = true;
    scan.scanning = false;
    scan.identified = false;
    scan.objectIndex = objectIndex;
    scan.progress = 0.0f;
    scan.revealedType = ObjectType::Debris;
}

bool CanStartUnknownScan(const ImmediateEventState &event, const UnknownScanState &scan, int selectedObjectIndex)
{
    return event.type == ImmediateEventType::UnknownObject &&
           event.phase == ImmediateEventPhase::WaitingForPlayer &&
           scan.objectActive &&
           !scan.identified &&
           selectedObjectIndex == scan.objectIndex;
}

void BeginUnknownScan(ImmediateEventState &event, UnknownScanState &scan)
{
    if (event.type != ImmediateEventType::UnknownObject || !scan.objectActive || scan.identified)
    {
        return;
    }

    scan.scanning = true;
    scan.progress = 0.0f;
    event.phase = ImmediateEventPhase::Animating;
    event.action = "Scanning Unknown-01...";
}

void UpdateUnknownScan(ImmediateEventState &event, UnknownScanState &scan, float deltaTime)
{
    if (!scan.scanning || scan.identified)
    {
        return;
    }

    scan.progress += deltaTime / kScanDuration;
    if (scan.progress >= 1.0f)
    {
        scan.progress = 1.0f;
        scan.scanning = false;
        scan.identified = true;
        event.phase = ImmediateEventPhase::Resolved;
        event.result = scan.revealedType == ObjectType::Debris ? "Unknown-01 identified as debris." : "Unknown-01 identified as satellite.";
        event.action = "Scan complete. Continue monitoring PlayerSat.";
    }
}
} // namespace OrbitGuard
```

- [ ] **Step 4: Run event smoke test**

Run:

```powershell
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' `
  tests\earth_event_smoke.cpp `
  src\simulation.cpp `
  src\earth_events.cpp `
  -std=c++17 `
  -I include `
  -I raylib\raylib-6.0_win64_mingw-w64\include `
  -o tests\earth_event_smoke.exe

& .\tests\earth_event_smoke.exe
```

Expected: exits with code `0`.

- [ ] **Step 5: Commit**

Run:

```powershell
& 'C:\Program Files\Git\cmd\git.exe' add include\orbit_guard.h src\earth_events.cpp tests\earth_event_smoke.cpp
& 'C:\Program Files\Git\cmd\git.exe' commit -m "feat: add earth event state"
```

---

### Task 4: Add Avoidance Animation Core

**Files:**
- Modify: `include/orbit_guard.h`
- Modify: `src/earth_events.cpp`
- Modify: `main.cpp`

- [ ] **Step 1: Add declarations**

In `include/orbit_guard.h`, add near event declarations:

```cpp
bool BeginAvoidanceAnimation(AvoidanceAnimationState &animation,
                             ImmediateEventState &event,
                             const std::vector<OrbitObject> &objects,
                             const AvoidancePlan &plan);
void UpdateAvoidanceAnimation(AvoidanceAnimationState &animation,
                              ImmediateEventState &event,
                              std::vector<OrbitObject> &objects,
                              float deltaTime);
```

- [ ] **Step 2: Implement animation helpers**

In `src/earth_events.cpp`, add before the namespace closing brace:

```cpp
bool BeginAvoidanceAnimation(AvoidanceAnimationState &animation,
                             ImmediateEventState &event,
                             const std::vector<OrbitObject> &objects,
                             const AvoidancePlan &plan)
{
    if (!plan.available || plan.objectIndex < 0 || plan.objectIndex >= static_cast<int>(objects.size()))
    {
        event.result = "No safe avoidance burn is available. Adjust orbit or continue monitoring.";
        return false;
    }

    animation.active = true;
    animation.objectIndex = plan.objectIndex;
    animation.elapsed = 0.0f;
    animation.duration = 1.8f;
    animation.startObject = objects[plan.objectIndex];
    animation.endObject = plan.proposedObject;
    animation.beforeDistance = plan.beforeDistance;
    animation.afterDistance = plan.afterDistance;

    event.phase = ImmediateEventPhase::Animating;
    event.action = "Avoidance burn in progress...";
    event.result.clear();
    return true;
}

void UpdateAvoidanceAnimation(AvoidanceAnimationState &animation,
                              ImmediateEventState &event,
                              std::vector<OrbitObject> &objects,
                              float deltaTime)
{
    if (!animation.active || animation.objectIndex < 0 || animation.objectIndex >= static_cast<int>(objects.size()))
    {
        return;
    }

    animation.elapsed += deltaTime;
    const float t = ClampFloat(animation.elapsed / animation.duration, 0.0f, 1.0f);
    OrbitObject &object = objects[animation.objectIndex];
    object.orbitRadius = animation.startObject.orbitRadius + (animation.endObject.orbitRadius - animation.startObject.orbitRadius) * t;
    object.inclinationDeg = animation.startObject.inclinationDeg + (animation.endObject.inclinationDeg - animation.startObject.inclinationDeg) * t;
    object.angularSpeed = animation.startObject.angularSpeed + (animation.endObject.angularSpeed - animation.startObject.angularSpeed) * t;
    RefreshObjectPosition(object);

    if (t >= 1.0f)
    {
        object = animation.endObject;
        RefreshObjectPosition(object);
        animation.active = false;
        event.phase = ImmediateEventPhase::Resolved;
        event.result = "Avoidance success: distance improved from " +
                       FormatFloat(animation.beforeDistance, 1) +
                       " to " +
                       FormatFloat(animation.afterDistance, 1) +
                       ".";
        event.action = "Avoidance complete. Continue monitoring PlayerSat.";
    }
}
```

- [ ] **Step 3: Connect `KEY_A` to animation in `main.cpp`**

Add variables near existing Earth mode variables:

```cpp
EarthMissionState earthMission;
ImmediateEventState activeEvent;
AvoidanceAnimationState avoidanceAnimation;
UnknownScanState unknownScan;
```

In the update section, call:

```cpp
UpdateAvoidanceAnimation(avoidanceAnimation, activeEvent, objects, deltaTime);
UpdateUnknownScan(activeEvent, unknownScan, deltaTime);
```

In the `KEY_A` branch, replace direct `ApplyAvoidancePlan(objects, avoidancePlan);` with:

```cpp
if (activeEvent.type == ImmediateEventType::CollisionWarning)
{
    if (BeginAvoidanceAnimation(avoidanceAnimation, activeEvent, objects, avoidancePlan))
    {
        MarkMissionAvoidanceApplied(missionState);
        actionMessage = "Avoidance burn started.";
    }
    else
    {
        actionMessage = activeEvent.result;
    }
}
else
{
    actionMessage = "No active collision warning.";
}
```

- [ ] **Step 4: Compile the app**

Run:

```powershell
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' `
  main.cpp `
  src\simulation.cpp `
  src\risk.cpp `
  src\mission.cpp `
  src\earth_events.cpp `
  src\ui.cpp `
  -o OrbitGuard.exe `
  -std=c++17 `
  -I include `
  -I raylib\raylib-6.0_win64_mingw-w64\include `
  -L raylib\raylib-6.0_win64_mingw-w64\lib `
  -lraylibdll `
  -lopengl32 `
  -lgdi32 `
  -lwinmm
```

Expected: compile succeeds.

- [ ] **Step 5: Commit**

Run:

```powershell
& 'C:\Program Files\Git\cmd\git.exe' add include\orbit_guard.h src\earth_events.cpp main.cpp
& 'C:\Program Files\Git\cmd\git.exe' commit -m "feat: animate avoidance burns"
```

---

### Task 5: Add Main Menu and GameMode Routing

**Files:**
- Modify: `include/orbit_guard.h`
- Modify: `main.cpp`
- Modify: `src/ui.cpp`

- [ ] **Step 1: Add UI declarations**

In `include/orbit_guard.h`, add near draw declarations:

```cpp
void DrawMainMenu(int selectedMenuItem);
void DrawModeTitle(const char *title, const char *subtitle);
```

- [ ] **Step 2: Implement main menu drawing**

In `src/ui.cpp`, add before `DrawInfoPanel`:

```cpp
void DrawMainMenu(int selectedMenuItem)
{
    DrawStarField();
    DrawRectangle(0, 0, kScreenWidth, kScreenHeight, {2, 5, 12, 190});
    DrawText("OrbitGuard", 96, 96, 54, RAYWHITE);
    DrawText("Space Simulator Platform", 100, 156, 24, Fade(RAYWHITE, 0.72f));

    const char *items[] = {
        "1  Earth Space Simulator",
        "2  Solar System Simulator",
        "3  Black Hole World Simulator"};

    for (int i = 0; i < 3; ++i)
    {
        const int y = 245 + i * 82;
        const bool selected = i == selectedMenuItem;
        DrawRectangle(96, y, 620, 58, selected ? Color{28, 56, 82, 230} : Color{12, 22, 34, 210});
        DrawRectangleLines(96, y, 620, 58, selected ? SKYBLUE : Fade(SKYBLUE, 0.30f));
        DrawText(items[i], 122, y + 17, 22, selected ? RAYWHITE : Fade(RAYWHITE, 0.78f));
    }

    DrawText("Use 1 / 2 / 3 or Up / Down + Enter. Esc quits.", 100, 548, 18, Fade(RAYWHITE, 0.66f));
}

void DrawModeTitle(const char *title, const char *subtitle)
{
    DrawRectangle(20, 18, 430, 64, {4, 8, 14, 190});
    DrawText(title, 38, 30, 26, RAYWHITE);
    DrawText(subtitle, 40, 60, 15, Fade(RAYWHITE, 0.68f));
}
```

- [ ] **Step 3: Route modes in `main.cpp`**

Add variables after window/camera setup:

```cpp
GameMode gameMode = GameMode::MainMenu;
int selectedMenuItem = 0;
```

At the top of the loop, before Earth-mode key handling, add:

```cpp
if (gameMode == GameMode::MainMenu)
{
    if (IsKeyPressed(KEY_DOWN))
    {
        selectedMenuItem = (selectedMenuItem + 1) % 3;
    }
    if (IsKeyPressed(KEY_UP))
    {
        selectedMenuItem = (selectedMenuItem + 2) % 3;
    }
    if (IsKeyPressed(KEY_ONE))
    {
        gameMode = GameMode::EarthSpace;
    }
    if (IsKeyPressed(KEY_TWO))
    {
        gameMode = GameMode::SolarSystem;
    }
    if (IsKeyPressed(KEY_THREE))
    {
        gameMode = GameMode::BlackHole;
    }
    if (IsKeyPressed(KEY_ENTER))
    {
        gameMode = selectedMenuItem == 0 ? GameMode::EarthSpace : (selectedMenuItem == 1 ? GameMode::SolarSystem : GameMode::BlackHole);
    }
    if (IsKeyPressed(KEY_ESCAPE))
    {
        shouldClose = true;
    }

    BeginDrawing();
    ClearBackground({3, 6, 12, 255});
    DrawMainMenu(selectedMenuItem);
    EndDrawing();
    continue;
}

if (IsKeyPressed(KEY_ESCAPE))
{
    gameMode = GameMode::MainMenu;
    continue;
}
```

Wrap existing Earth-specific update/draw code in:

```cpp
if (gameMode == GameMode::EarthSpace)
{
    // existing Earth update and draw code lives here
}
```

- [ ] **Step 4: Compile the app**

Use the app compile command from Task 4 Step 4.

Expected: compile succeeds, app starts at the menu, key `1` enters Earth mode, `Esc` returns to menu.

- [ ] **Step 5: Commit**

Run:

```powershell
& 'C:\Program Files\Git\cmd\git.exe' add include\orbit_guard.h src\ui.cpp main.cpp
& 'C:\Program Files\Git\cmd\git.exe' commit -m "feat: add simulator main menu"
```

---

### Task 6: Update Earth UI, Background, Warnings, and Selection Frames

**Files:**
- Modify: `include/orbit_guard.h`
- Modify: `src/ui.cpp`
- Modify: `main.cpp`

- [ ] **Step 1: Add draw declarations**

In `include/orbit_guard.h`, add:

```cpp
void DrawSolarSystemBackground();
void DrawOrbitLayerBands();
void DrawImmediateEventBanner(const ImmediateEventState &event, const UnknownScanState &scan);
Color SelectedFrameColor(const ImmediateEventState &event,
                         const EarthMissionState &earthMission,
                         const std::vector<OrbitObject> &objects,
                         int selectedObjectIndex);
```

Update `DrawInfoPanel` declaration to include Earth mission and active event:

```cpp
void DrawInfoPanel(const RiskReport &report,
                   const std::vector<OrbitObject> &objects,
                   bool paused,
                   float timeScale,
                   float simulationTime,
                   float exportMessageTimer,
                   const LaunchSettings &launchSettings,
                   const AvoidancePlan &avoidancePlan,
                   const MissionState &missionState,
                   const EarthMissionState &earthMission,
                   const ImmediateEventState &activeEvent,
                   const UnknownScanState &unknownScan,
                   bool showDemoObjects,
                   int selectedObjectIndex,
                   float actionMessageTimer,
                   const std::string &actionMessage);
```

- [ ] **Step 2: Implement Option A background**

In `src/ui.cpp`, add:

```cpp
void DrawSolarSystemBackground()
{
    DrawStarField();
    DrawCircleGradient(88, 128, 92.0f, Color{255, 183, 82, 82}, Color{255, 183, 82, 0});
    DrawCircle(86, 126, 28.0f, Color{255, 213, 128, 170});
    DrawCircleGradient(1130, 148, 26.0f, Color{201, 105, 78, 120}, Color{201, 105, 78, 0});
    DrawCircle(1130, 148, 10.0f, Color{185, 88, 63, 180});
    DrawCircleGradient(1080, 570, 42.0f, Color{82, 148, 215, 70}, Color{82, 148, 215, 0});
}
```

- [ ] **Step 3: Draw orbit layer bands**

In `src/ui.cpp`, add:

```cpp
void DrawOrbitLayerBands()
{
    DrawOrbitPath(85.0f, 0.0f, Fade(SKYBLUE, 0.16f));
    DrawOrbitPath(145.0f, 0.0f, Fade(SKYBLUE, 0.22f));
    DrawOrbitPath(230.0f, 0.0f, Fade(GOLD, 0.22f));
    DrawOrbitPath(330.0f, 0.0f, Fade(VIOLET, 0.20f));
}
```

- [ ] **Step 4: Implement warning banner**

In `src/ui.cpp`, add:

```cpp
void DrawImmediateEventBanner(const ImmediateEventState &event, const UnknownScanState &scan)
{
    if (event.type == ImmediateEventType::None || event.phase == ImmediateEventPhase::Inactive)
    {
        return;
    }

    const Color accent = event.type == ImmediateEventType::CollisionWarning ? RED : SKYBLUE;
    DrawRectangle(310, 24, 590, 86, {7, 12, 20, 235});
    DrawRectangle(310, 24, 8, 86, accent);
    DrawRectangleLines(310, 24, 590, 86, Fade(accent, 0.62f));
    DrawText(event.title.c_str(), 334, 36, 24, accent);
    DrawText(event.detail.c_str(), 336, 64, 16, RAYWHITE);

    std::string action = event.action;
    if (scan.scanning)
    {
        action = "Scanning progress: " + FormatFloat(scan.progress * 100.0f, 0) + "%";
    }
    if (!event.result.empty())
    {
        action = event.result;
    }
    DrawText(action.c_str(), 336, 88, 15, Fade(RAYWHITE, 0.78f));
}
```

- [ ] **Step 5: Implement selected-only frame color**

In `src/ui.cpp`, add:

```cpp
Color SelectedFrameColor(const ImmediateEventState &event,
                         const EarthMissionState &earthMission,
                         const std::vector<OrbitObject> &objects,
                         int selectedObjectIndex)
{
    if (selectedObjectIndex < 0 || selectedObjectIndex >= static_cast<int>(objects.size()))
    {
        return BLANK;
    }

    if (event.targetIndex == selectedObjectIndex)
    {
        if (event.type == ImmediateEventType::CollisionWarning)
        {
            return event.phase == ImmediateEventPhase::Resolved ? LIME : ORANGE;
        }
        if (event.type == ImmediateEventType::UnknownObject)
        {
            return SKYBLUE;
        }
    }

    if (objects[selectedObjectIndex].userControlled && earthMission.playerSatDeployed)
    {
        return YELLOW;
    }

    return BLANK;
}
```

Update `DrawSpaceObject` to accept `Color frameColor` instead of `bool highlighted`, and draw the wire sphere only when `frameColor.a > 0`:

```cpp
void DrawSpaceObject(const OrbitObject &object, Color frameColor)
{
    const float size = object.type == ObjectType::Satellite ? 8.0f : 5.5f;
    // keep existing cube/sphere drawing
    if (frameColor.a > 0)
    {
        DrawSphereWires(object.position, size + 6.0f, 16, 12, frameColor);
    }
}
```

Update the declaration in `include/orbit_guard.h` to match.

- [ ] **Step 6: Use the new Earth visuals in `main.cpp`**

Replace Earth draw background:

```cpp
ClearBackground({3, 6, 12, 255});
DrawStarField();
```

with:

```cpp
ClearBackground({3, 6, 12, 255});
DrawSolarSystemBackground();
```

Inside `BeginMode3D(camera)`, draw layer bands before object paths:

```cpp
DrawOrbitLayerBands();
```

When drawing objects, replace the old highlighted calculation with:

```cpp
const Color frameColor = i == selectedObjectIndex ? SelectedFrameColor(activeEvent, earthMission, objects, selectedObjectIndex) : BLANK;
DrawSpaceObject(objects[i], frameColor);
```

After `EndMode3D()`, call:

```cpp
DrawImmediateEventBanner(activeEvent, unknownScan);
```

- [ ] **Step 7: Compile and manually verify**

Run the app compile command.

Expected:

- Earth mode background resembles Option A.
- LEO/MEO/GEO guides are visible but subtle.
- Only selected objects can show frames.
- Collision and unknown events use screen banners.

- [ ] **Step 8: Commit**

Run:

```powershell
& 'C:\Program Files\Git\cmd\git.exe' add include\orbit_guard.h src\ui.cpp main.cpp
& 'C:\Program Files\Git\cmd\git.exe' commit -m "feat: refresh earth simulator visuals"
```

---

### Task 7: Add Playable Solar System and Black Hole Ship Prototypes

**Files:**
- Modify: `include/orbit_guard.h`
- Create: `src/ship_modes.cpp`
- Modify: `main.cpp`
- Test: `tests/ship_state_smoke.cpp`

- [ ] **Step 1: Add ship-state smoke test**

Create `tests/ship_state_smoke.cpp`:

```cpp
#include "orbit_guard.h"

#include <iostream>

using namespace OrbitGuard;

bool Expect(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

int main()
{
    bool ok = true;
    ShipState ship;
    ship.position = {0.0f, 0.0f, 260.0f};
    ship.yaw = 0.0f;

    MoveShipForward(ship, 1.0f, 60.0f);
    ok = Expect(ship.position.z < 260.0f, "forward movement reduces z at yaw 0") && ok;

    ship.position = {0.0f, 0.0f, 120.0f};
    ok = Expect(IsInsideBlackHoleTransferRange(ship, 130.0f), "ship inside black hole range") && ok;

    ship.position = {0.0f, 0.0f, 260.0f};
    ok = Expect(!IsInsideBlackHoleTransferRange(ship, 130.0f), "ship outside black hole range") && ok;
    return ok ? 0 : 1;
}
```

- [ ] **Step 2: Add declarations**

In `include/orbit_guard.h`, add:

```cpp
void ResetShip(ShipState &ship, Vector3 position, float yaw);
void UpdateShipFromInput(ShipState &ship, float deltaTime);
void MoveShipForward(ShipState &ship, float deltaTime, float speed);
bool IsInsideBlackHoleTransferRange(const ShipState &ship, float eventHorizonRadius);
void DrawShip(const ShipState &ship, Color color);
void DrawSolarSystemMode(const ShipState &ship);
void DrawBlackHoleMode(const ShipState &ship, bool transferred);
```

- [ ] **Step 3: Implement `src/ship_modes.cpp`**

Create `src/ship_modes.cpp`:

```cpp
#include "orbit_guard.h"

#include <cmath>

namespace OrbitGuard
{
void ResetShip(ShipState &ship, Vector3 position, float yaw)
{
    ship.position = position;
    ship.yaw = yaw;
    ship.speed = 0.0f;
}

void MoveShipForward(ShipState &ship, float deltaTime, float speed)
{
    ship.position.x += std::sin(ship.yaw) * speed * deltaTime;
    ship.position.z -= std::cos(ship.yaw) * speed * deltaTime;
}

void UpdateShipFromInput(ShipState &ship, float deltaTime)
{
    if (IsKeyDown(KEY_A))
    {
        ship.yaw -= 1.8f * deltaTime;
    }
    if (IsKeyDown(KEY_D))
    {
        ship.yaw += 1.8f * deltaTime;
    }
    if (IsKeyDown(KEY_W))
    {
        MoveShipForward(ship, deltaTime, 95.0f);
    }
    if (IsKeyDown(KEY_S))
    {
        MoveShipForward(ship, deltaTime, -70.0f);
    }
}

bool IsInsideBlackHoleTransferRange(const ShipState &ship, float eventHorizonRadius)
{
    return Vector3Length(ship.position) <= eventHorizonRadius;
}

void DrawShip(const ShipState &ship, Color color)
{
    const Vector3 nose = {
        ship.position.x + std::sin(ship.yaw) * 18.0f,
        ship.position.y,
        ship.position.z - std::cos(ship.yaw) * 18.0f};
    const Vector3 left = {
        ship.position.x + std::sin(ship.yaw + 2.4f) * 12.0f,
        ship.position.y,
        ship.position.z - std::cos(ship.yaw + 2.4f) * 12.0f};
    const Vector3 right = {
        ship.position.x + std::sin(ship.yaw - 2.4f) * 12.0f,
        ship.position.y,
        ship.position.z - std::cos(ship.yaw - 2.4f) * 12.0f};
    DrawTriangle3D(nose, left, right, color);
    DrawLine3D(left, right, Fade(WHITE, 0.75f));
}

void DrawSolarSystemMode(const ShipState &ship)
{
    DrawSolarSystemBackground();
    DrawModeTitle("Solar System Simulator", "Fly near planets to preview future planet introductions.");
    BeginMode3D(Camera3D{{0.0f, 320.0f, 520.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, 45.0f, CAMERA_PERSPECTIVE});
    DrawGrid(30, 24.0f);
    DrawSphere({0.0f, 0.0f, 0.0f}, 34.0f, GOLD);
    DrawSphere({110.0f, 0.0f, 0.0f}, 10.0f, SKYBLUE);
    DrawSphere({185.0f, 0.0f, 45.0f}, 14.0f, BLUE);
    DrawSphere({275.0f, 0.0f, -70.0f}, 12.0f, ORANGE);
    DrawShip(ship, YELLOW);
    EndMode3D();
    DrawText("W/S fly   A/D turn   Esc menu", 32, 672, 18, Fade(RAYWHITE, 0.78f));
}

void DrawBlackHoleMode(const ShipState &ship, bool transferred)
{
    DrawStarField();
    DrawModeTitle("Black Hole World Simulator", transferred ? "Transfer complete: unknown world preview." : "Fly toward the event horizon.");
    BeginMode3D(Camera3D{{0.0f, 190.0f, 440.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, 45.0f, CAMERA_PERSPECTIVE});
    DrawGrid(24, 24.0f);
    if (transferred)
    {
        DrawSphere({0.0f, 0.0f, 0.0f}, 72.0f, Color{80, 180, 210, 255});
        DrawSphereWires({0.0f, 0.0f, 0.0f}, 110.0f, 24, 12, Fade(SKYBLUE, 0.5f));
    }
    else
    {
        DrawSphere({0.0f, 0.0f, 0.0f}, 74.0f, BLACK);
        DrawSphereWires({0.0f, 0.0f, 0.0f}, 118.0f, 32, 16, Fade(VIOLET, 0.76f));
    }
    DrawShip(ship, transferred ? SKYBLUE : YELLOW);
    EndMode3D();
    DrawText(transferred ? "Unknown world preview   Esc menu" : "W/S fly   A/D turn   Enter event horizon to transfer   Esc menu",
             32, 672, 18, Fade(RAYWHITE, 0.78f));
}
} // namespace OrbitGuard
```

- [ ] **Step 4: Connect modes in `main.cpp`**

Add variables:

```cpp
ShipState solarShip;
ShipState blackHoleShip;
bool blackHoleTransferred = false;
ResetShip(solarShip, {0.0f, 0.0f, 330.0f}, 0.0f);
ResetShip(blackHoleShip, {0.0f, 0.0f, 330.0f}, 0.0f);
```

Add after main-menu routing:

```cpp
if (gameMode == GameMode::SolarSystem)
{
    UpdateShipFromInput(solarShip, deltaTime);
    BeginDrawing();
    ClearBackground({3, 6, 12, 255});
    DrawSolarSystemMode(solarShip);
    EndDrawing();
    continue;
}

if (gameMode == GameMode::BlackHole)
{
    UpdateShipFromInput(blackHoleShip, deltaTime);
    if (!blackHoleTransferred && IsInsideBlackHoleTransferRange(blackHoleShip, 130.0f))
    {
        blackHoleTransferred = true;
    }
    BeginDrawing();
    ClearBackground({3, 6, 12, 255});
    DrawBlackHoleMode(blackHoleShip, blackHoleTransferred);
    EndDrawing();
    continue;
}
```

- [ ] **Step 5: Add build config entries**

Update `compile_orbit_guard.bat` compile command to include:

```bat
"%~dp0src\earth_events.cpp" "%~dp0src\ship_modes.cpp"
```

Update `Makefile.win`:

```makefile
OBJ      = main.o simulation.o risk.o mission.o earth_events.o ship_modes.o ui.o
LINKOBJ  = main.o simulation.o risk.o mission.o earth_events.o ship_modes.o ui.o
```

Add rules:

```makefile
earth_events.o: src/earth_events.cpp include/orbit_guard.h
	$(CPP) -c src/earth_events.cpp -o earth_events.o $(CXXFLAGS)

ship_modes.o: src/ship_modes.cpp include/orbit_guard.h
	$(CPP) -c src/ship_modes.cpp -o ship_modes.o $(CXXFLAGS)
```

Update `Project orbit guard.dev` by increasing `UnitCount` by 2 and adding `src/earth_events.cpp` and `src/ship_modes.cpp` as source units.

- [ ] **Step 6: Run ship-state test and compile app**

Run:

```powershell
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' `
  tests\ship_state_smoke.cpp `
  src\ship_modes.cpp `
  src\ui.cpp `
  src\simulation.cpp `
  -std=c++17 `
  -I include `
  -I raylib\raylib-6.0_win64_mingw-w64\include `
  -L raylib\raylib-6.0_win64_mingw-w64\lib `
  -lraylibdll `
  -lopengl32 `
  -lgdi32 `
  -lwinmm `
  -o tests\ship_state_smoke.exe

& .\tests\ship_state_smoke.exe
```

Then compile the full app with all source files.

Expected: test exits with code `0`; app compiles; menu options `2` and `3` enter playable ship scenes.

- [ ] **Step 7: Commit**

Run:

```powershell
& 'C:\Program Files\Git\cmd\git.exe' add include\orbit_guard.h src\ship_modes.cpp main.cpp compile_orbit_guard.bat Makefile.win "Project orbit guard.dev" tests\ship_state_smoke.cpp
& 'C:\Program Files\Git\cmd\git.exe' commit -m "feat: add playable ship simulator prototypes"
```

---

### Task 8: Final Verification Pass

**Files:**
- Read/Run: tests and `OrbitGuard.exe`
- Modify if needed: files changed above

- [ ] **Step 1: Run all smoke tests**

Run:

```powershell
& .\tests\mission_state_smoke.exe
& .\tests\risk_scope_smoke.exe
& .\tests\orbit_layer_smoke.exe
& .\tests\player_satellite_smoke.exe
& .\tests\earth_event_smoke.exe
& .\tests\ship_state_smoke.exe
```

Expected: all exit with code `0`.

- [ ] **Step 2: Compile full app**

Run the full app compile command including:

```powershell
main.cpp
src\simulation.cpp
src\risk.cpp
src\mission.cpp
src\earth_events.cpp
src\ship_modes.cpp
src\ui.cpp
```

Expected: `OrbitGuard.exe` is produced.

- [ ] **Step 3: Manual Earth verification**

Run:

```powershell
& .\OrbitGuard.exe
```

Manual checks:

- Main menu appears first.
- Press `1` to enter Earth mode.
- Option A background is visible and does not reduce warning readability.
- LEO/MEO/GEO guides are visible.
- Press `Tab` and `Left`/`Right` to place preview orbit in the target layer.
- Press `L`; only one `PlayerSat` exists.
- Pressing `L` again updates `PlayerSat`, not adding another player satellite.
- Collision warning banner uses `Press A`.
- Pressing `A` during a warning starts avoidance burn animation.
- Unknown warning banner uses `Select Unknown-01 and press C`.
- Scanning only starts when Unknown-01 is selected and `C` is pressed.
- Unselected objects show no frames.

- [ ] **Step 4: Manual Solar System verification**

Manual checks:

- Press `Esc` to return to menu.
- Press `2` to enter Solar System mode.
- `W/S` moves the ship.
- `A/D` turns the ship.
- Scene contains sun and planets.
- `Esc` returns to menu.

- [ ] **Step 5: Manual Black Hole verification**

Manual checks:

- Press `3` to enter Black Hole mode.
- `W/S` and `A/D` control the ship.
- Flying close to the black hole changes to the unknown world preview.
- `Esc` returns to menu.

- [ ] **Step 6: Check ignored files and Git status**

Run:

```powershell
& 'C:\Program Files\Git\cmd\git.exe' status --short --ignored
```

Expected:

- Source files and docs are either clean or staged for final commit.
- `.exe`, `.o`, `.dll`, installer, downloads, generated reports, and proposal documents remain ignored.

- [ ] **Step 7: Final commit**

If any verification fixes were made, run:

```powershell
& 'C:\Program Files\Git\cmd\git.exe' add include main.cpp src tests compile_orbit_guard.bat Makefile.win "Project orbit guard.dev"
& 'C:\Program Files\Git\cmd\git.exe' commit -m "fix: polish simulator mode verification"
```

Expected: final working tree is clean.

---

## Self-Review

- Spec coverage: this plan covers the main menu, Earth simulator mission upgrade, single `PlayerSat`, orbit layers, immediate warnings, avoidance animation, active unknown scan, selected-only frames, Option A background, playable Solar System ship prototype, playable Black Hole ship prototype, tests, build configuration, and manual verification.
- Placeholder scan: no unfinished markers are present.
- Type consistency: `GameMode`, `OrbitLayer`, `EarthMissionState`, `ImmediateEventState`, `AvoidanceAnimationState`, `UnknownScanState`, and `ShipState` are declared before use and named consistently across tests, helpers, and mode routing.
