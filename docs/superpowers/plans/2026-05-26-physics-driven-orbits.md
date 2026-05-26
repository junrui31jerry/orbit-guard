# Physics-Driven Orbits Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace fixed-angle orbit animation with a simplified two-body physics model tuned so the default PlayerSat takes about 30 seconds to complete a half orbit.

**Architecture:** Keep the existing C++17/raylib structure and add physics behavior inside the current simulation, risk, and event modules. `OrbitObject` will store velocity and physics metadata; `src/simulation.cpp` will own orbital integration and launch velocity helpers; `src/risk.cpp` will own future closest-approach prediction; `src/earth_events.cpp` will convert avoidance and impact debris to velocity-based behavior while preserving existing UI flow.

**Tech Stack:** C++17, raylib/raymath, Dev-C++ MinGW toolchain, existing smoke-test executables.

---

## File Structure

- Modify `include/orbit_guard.h`
  - Add physics fields to `OrbitObject`.
  - Add prediction fields to `RiskReport`.
  - Add delta-v fields to `AvoidancePlan` and `AvoidanceAnimationState`.
  - Declare physics helper functions and prediction constants.

- Modify `src/simulation.cpp`
  - Add game-tuned gravity constants.
  - Add tangent velocity creation from launch settings.
  - Convert `UpdateObjects()` to integrate position and velocity under central gravity.
  - Keep old orbit fields synchronized for UI labels and orbit path drawing.

- Modify `src/risk.cpp`
  - Add non-mutating future closest-approach prediction.
  - Evaluate risk using predicted closest approach over 120 seconds of simulation time.
  - Build avoidance plans from candidate delta-v impulses.

- Modify `src/earth_events.cpp`
  - Remove direct scripted stepping of impact debris toward PlayerSat.
  - Spawn impact debris with a physical initial velocity.
  - Apply avoidance as delta-v.
  - Keep successful-avoidance and explosion cleanup behavior.

- Modify `src/ui.cpp`
  - Rename the user-facing speed control from angular speed to speed bias.
  - Display predicted closest-approach time when present.

- Modify `main.cpp`
  - Keep the existing main loop, object update order, and controls.
  - Recompute risk after event updates as already done.

- Add `tests/physics_orbit_smoke.cpp`
  - Verify launch velocity and 30-second half-orbit target.

- Add `tests/risk_prediction_smoke.cpp`
  - Verify future closest-approach prediction and delta-v avoidance planning.

- Update `tests/earth_event_flow_smoke.cpp`
  - Verify spawned impact debris moves through physics and is cleaned up after collision or avoidance.

- Update `tests/player_satellite_smoke.cpp`
  - Verify PlayerSat has physics enabled and nonzero velocity.

---

### Task 1: Add Physics Fields And Launch Velocity Tests

**Files:**
- Modify: `include/orbit_guard.h`
- Modify: `src/simulation.cpp`
- Create: `tests/physics_orbit_smoke.cpp`
- Modify: `tests/player_satellite_smoke.cpp`

- [ ] **Step 1: Write the failing physics orbit smoke test**

Create `tests/physics_orbit_smoke.cpp`:

```cpp
#include "orbit_guard.h"

#include <cmath>
#include <iostream>
#include <vector>

using namespace OrbitGuard;

extern "C"
{
Color Fade(Color color, float alpha)
{
    color.a = static_cast<unsigned char>(static_cast<float>(color.a) * alpha);
    return color;
}

bool IsMouseButtonDown(int button)
{
    (void)button;
    return false;
}

Vector2 GetMouseDelta(void)
{
    return {0.0f, 0.0f};
}

float GetMouseWheelMove(void)
{
    return 0.0f;
}
}

bool Expect(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

float AngleDelta(float a, float b)
{
    float delta = std::fabs(a - b);
    while (delta > 2.0f * kPi)
    {
        delta -= 2.0f * kPi;
    }
    if (delta > kPi)
    {
        delta = 2.0f * kPi - delta;
    }
    return delta;
}

int main()
{
    bool ok = true;

    LaunchSettings settings;
    settings.orbitRadius = kReferenceOrbitRadius;
    settings.inclinationDeg = 0.0f;
    settings.angularSpeed = kDefaultSpeedBiasControl;
    settings.initialAngleDeg = 0.0f;

    OrbitObject player = CreatePlayerSatellite(settings);
    ok = Expect(player.physicsDriven, "PlayerSat uses physics-driven motion") && ok;
    ok = Expect(Vector3Length(player.velocity) > 0.1f, "PlayerSat launch creates nonzero velocity") && ok;

    const Vector3 radial = Vector3Normalize(player.position);
    const Vector3 velocityDirection = Vector3Normalize(player.velocity);
    ok = Expect(std::fabs(Vector3DotProduct(radial, velocityDirection)) < 0.02f,
                "PlayerSat velocity is tangent to the launch radius") &&
         ok;

    std::vector<OrbitObject> objects = {player};
    const float startRadius = Vector3Length(objects[0].position);
    const float startAngle = objects[0].angleRad;

    UpdateObjects(objects, 30.0f, 1.0f);

    const float endRadius = Vector3Length(objects[0].position);
    const float halfOrbitDelta = AngleDelta(objects[0].angleRad, startAngle);
    ok = Expect(std::fabs(halfOrbitDelta - kPi) < 0.28f,
                "default MEO PlayerSat completes about a half orbit in 30 seconds") &&
         ok;
    ok = Expect(std::fabs(endRadius - startRadius) < startRadius * 0.08f,
                "physics orbit keeps radius stable over a half orbit") &&
         ok;

    return ok ? 0 : 1;
}
```

- [ ] **Step 2: Run the new test and verify it fails**

Run:

```powershell
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' tests\physics_orbit_smoke.cpp src\simulation.cpp -I include -I raylib\raylib-6.0_win64_mingw-w64\include -L raylib\raylib-6.0_win64_mingw-w64\lib -std=c++17 -lraylibdll -lopengl32 -lgdi32 -lwinmm -o physics_orbit_smoke.exe
```

Expected: compile fails because `kReferenceOrbitRadius`, `kDefaultSpeedBiasControl`, `physicsDriven`, and `velocity` do not exist yet.

- [ ] **Step 3: Add physics fields and constants**

Modify `include/orbit_guard.h`:

```cpp
inline constexpr float kReferenceOrbitRadius = 165.0f;
inline constexpr float kReferenceOrbitPeriodSeconds = 60.0f;
inline constexpr float kDefaultSpeedBiasControl = 0.48f;
inline constexpr float kEarthMu = (4.0f * kPi * kPi * kReferenceOrbitRadius * kReferenceOrbitRadius * kReferenceOrbitRadius) /
                                  (kReferenceOrbitPeriodSeconds * kReferenceOrbitPeriodSeconds);
inline constexpr float kPredictionHorizonSeconds = 120.0f;
inline constexpr float kPredictionStepSeconds = 0.5f;
inline constexpr float kDefaultSatelliteCollisionRadius = 8.0f;
inline constexpr float kDefaultDebrisCollisionRadius = 6.0f;
```

Add fields at the end of `OrbitObject` so existing aggregate initializers keep compiling:

```cpp
Vector3 velocity = {};
float collisionRadius = kDefaultSatelliteCollisionRadius;
bool physicsDriven = true;
```

Declare helpers near the existing simulation functions:

```cpp
float SpeedBiasFromControl(float speedControl);
Vector3 CalculateOrbitTangent(float inclinationDeg, float angleRad);
Vector3 CalculateCircularOrbitVelocity(float radius, float inclinationDeg, float angleRad, float speedControl);
void SyncOrbitFieldsFromPosition(OrbitObject &object);
void InitializeOrbitPhysics(OrbitObject &object);
```

- [ ] **Step 4: Implement launch velocity helpers**

Modify `src/simulation.cpp` after `CalculateOrbitPosition()`:

```cpp
float SpeedBiasFromControl(float speedControl)
{
    const float bias = std::fabs(speedControl) / kDefaultSpeedBiasControl;
    return ClampFloat(bias, 0.65f, 1.35f);
}

Vector3 CalculateOrbitTangent(float inclinationDeg, float angleRad)
{
    const float inclination = inclinationDeg * kDegToRad;
    Vector3 tangent = {
        -std::sin(angleRad),
        std::cos(angleRad) * std::sin(inclination),
        std::cos(angleRad) * std::cos(inclination)};
    return Vector3Normalize(tangent);
}

Vector3 CalculateCircularOrbitVelocity(float radius, float inclinationDeg, float angleRad, float speedControl)
{
    const float safeRadius = std::max(radius, kEarthRadius + 1.0f);
    const float direction = speedControl < 0.0f ? -1.0f : 1.0f;
    const float speed = std::sqrt(kEarthMu / safeRadius) * SpeedBiasFromControl(speedControl);
    return Vector3Scale(CalculateOrbitTangent(inclinationDeg, angleRad), speed * direction);
}

void SyncOrbitFieldsFromPosition(OrbitObject &object)
{
    const float radius = Vector3Length(object.position);
    if (radius <= 0.001f)
    {
        return;
    }

    const float yzRadius = std::sqrt(object.position.y * object.position.y + object.position.z * object.position.z);
    object.orbitRadius = radius;
    object.angleRad = std::atan2(yzRadius, object.position.x);
    object.inclinationDeg = std::atan2(object.position.y, object.position.z) / kDegToRad;
    object.initialAngleDeg = object.angleRad / kDegToRad;
    while (object.initialAngleDeg < 0.0f)
    {
        object.initialAngleDeg += 360.0f;
    }
    while (object.initialAngleDeg >= 360.0f)
    {
        object.initialAngleDeg -= 360.0f;
    }
}

void InitializeOrbitPhysics(OrbitObject &object)
{
    object.angleRad = object.initialAngleDeg * kDegToRad;
    object.position = CalculateOrbitPosition(object.orbitRadius, object.inclinationDeg, object.angleRad);
    object.velocity = CalculateCircularOrbitVelocity(object.orbitRadius, object.inclinationDeg, object.angleRad, object.angularSpeed);
    object.collisionRadius = object.type == ObjectType::Satellite ? kDefaultSatelliteCollisionRadius : kDefaultDebrisCollisionRadius;
    object.physicsDriven = true;
}
```

- [ ] **Step 5: Update satellite creation and reset**

In `CreateUserSatellite()` and `CreatePlayerSatellite()`, after setting `userControlled`, call:

```cpp
InitializeOrbitPhysics(object);
```

In `ResetObjects()`, replace direct angle/position reset with:

```cpp
for (OrbitObject &object : objects)
{
    InitializeOrbitPhysics(object);
}
```

- [ ] **Step 6: Add physics integration**

Modify `UpdateObjects()` in `src/simulation.cpp`:

```cpp
void UpdateObjects(std::vector<OrbitObject> &objects, float deltaTime, float timeScale)
{
    const float physicsDelta = deltaTime * timeScale;
    for (OrbitObject &object : objects)
    {
        if (!object.physicsDriven)
        {
            object.angleRad += object.angularSpeed * physicsDelta;
            object.angleRad = std::fmod(object.angleRad, 2.0f * kPi);
            RefreshObjectPosition(object);
            continue;
        }

        const float radius = Vector3Length(object.position);
        if (radius <= kEarthRadius + 0.5f)
        {
            continue;
        }

        const float radiusCubed = radius * radius * radius;
        const Vector3 acceleration = Vector3Scale(object.position, -kEarthMu / radiusCubed);
        object.velocity = Vector3Add(object.velocity, Vector3Scale(acceleration, physicsDelta));
        object.position = Vector3Add(object.position, Vector3Scale(object.velocity, physicsDelta));
        SyncOrbitFieldsFromPosition(object);
    }
}
```

- [ ] **Step 7: Run physics orbit test and verify it passes**

Run:

```powershell
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' tests\physics_orbit_smoke.cpp src\simulation.cpp -I include -I raylib\raylib-6.0_win64_mingw-w64\include -L raylib\raylib-6.0_win64_mingw-w64\lib -std=c++17 -lraylibdll -lopengl32 -lgdi32 -lwinmm -o physics_orbit_smoke.exe
cmd /c physics_orbit_smoke.exe
```

Expected: exit code `0`.

- [ ] **Step 8: Update PlayerSat smoke test**

In `tests/player_satellite_smoke.cpp`, after the existing first PlayerSat assertions, add:

```cpp
ok = Expect(objects[firstIndex].physicsDriven, "PlayerSat is physics driven") && ok;
ok = Expect(Vector3Length(objects[firstIndex].velocity) > 0.1f, "PlayerSat has launch velocity") && ok;
```

- [ ] **Step 9: Run PlayerSat smoke test**

Run:

```powershell
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' tests\player_satellite_smoke.cpp src\simulation.cpp -I include -I raylib\raylib-6.0_win64_mingw-w64\include -std=c++17 -o player_satellite_smoke.exe
cmd /c player_satellite_smoke.exe
```

Expected: exit code `0`.

- [ ] **Step 10: Commit Task 1**

Run:

```powershell
git add include/orbit_guard.h src/simulation.cpp tests/physics_orbit_smoke.cpp tests/player_satellite_smoke.cpp
git commit -m "feat: add physics-driven orbit motion"
```

Expected: commit succeeds after preserving unrelated user changes.

---

### Task 2: Predict Future Closest Approach

**Files:**
- Modify: `include/orbit_guard.h`
- Modify: `src/risk.cpp`
- Create: `tests/risk_prediction_smoke.cpp`

- [ ] **Step 1: Write the failing risk prediction test**

Create `tests/risk_prediction_smoke.cpp`:

```cpp
#include "orbit_guard.h"

#include <iostream>
#include <vector>

using namespace OrbitGuard;

namespace OrbitGuard
{
float ClampFloat(float value, float minimum, float maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

void RefreshObjectPosition(OrbitObject &object)
{
    (void)object;
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

void UpdateObjects(std::vector<OrbitObject> &objects, float deltaTime, float timeScale)
{
    const float dt = deltaTime * timeScale;
    for (OrbitObject &object : objects)
    {
        object.position = Vector3Add(object.position, Vector3Scale(object.velocity, dt));
    }
}
} // namespace OrbitGuard

bool Expect(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

OrbitObject MakeObject(const char *name, ObjectType type, bool player, Vector3 position, Vector3 velocity)
{
    OrbitObject object;
    object.name = name;
    object.type = type;
    object.userControlled = player;
    object.position = position;
    object.velocity = velocity;
    object.physicsDriven = true;
    object.collisionRadius = type == ObjectType::Satellite ? kDefaultSatelliteCollisionRadius : kDefaultDebrisCollisionRadius;
    return object;
}

int main()
{
    bool ok = true;

    std::vector<OrbitObject> objects = {
        MakeObject("PlayerSat", ObjectType::Satellite, true, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}),
        MakeObject("Impact-Debris-01", ObjectType::Debris, false, {120.0f, 0.0f, 0.0f}, {-2.0f, 0.0f, 0.0f}),
    };

    RiskReport report = AnalyzePairRisk(objects, 0, 1);
    ok = Expect(report.predicted, "pair risk uses prediction for physics objects") && ok;
    ok = Expect(report.closestApproachTime > 55.0f && report.closestApproachTime < 65.0f,
                "closest approach time is predicted from future motion") &&
         ok;
    ok = Expect(report.distance < kHighRiskDistance, "future close approach becomes high risk") && ok;
    ok = Expect(report.level == RiskLevel::High, "predicted close approach sets high risk") && ok;

    return ok ? 0 : 1;
}
```

- [ ] **Step 2: Run the prediction test and verify it fails**

Run:

```powershell
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' tests\risk_prediction_smoke.cpp src\risk.cpp -I include -I raylib\raylib-6.0_win64_mingw-w64\include -std=c++17 -o risk_prediction_smoke.exe
```

Expected: compile fails because `RiskReport::predicted` and `RiskReport::closestApproachTime` do not exist.

- [ ] **Step 3: Add prediction fields**

Modify `RiskReport` in `include/orbit_guard.h`:

```cpp
bool predicted = false;
float closestApproachTime = 0.0f;
```

Declare prediction helpers near risk declarations:

```cpp
RiskReport PredictPairRisk(const std::vector<OrbitObject> &objects,
                           int firstIndex,
                           int secondIndex,
                           float horizonSeconds = kPredictionHorizonSeconds,
                           float stepSeconds = kPredictionStepSeconds);
```

- [ ] **Step 4: Implement pair prediction**

Add this helper in `src/risk.cpp` inside the anonymous namespace:

```cpp
void StepPredictedObject(OrbitObject &object, float deltaTime)
{
    if (!object.physicsDriven)
    {
        object.position = Vector3Add(object.position, Vector3Scale(object.velocity, deltaTime));
        return;
    }

    const float radius = Vector3Length(object.position);
    if (radius <= kEarthRadius + 0.5f)
    {
        return;
    }

    const float radiusCubed = radius * radius * radius;
    const Vector3 acceleration = Vector3Scale(object.position, -kEarthMu / radiusCubed);
    object.velocity = Vector3Add(object.velocity, Vector3Scale(acceleration, deltaTime));
    object.position = Vector3Add(object.position, Vector3Scale(object.velocity, deltaTime));
}
```

Add public `PredictPairRisk()` before `AnalyzePairRisk()`:

```cpp
RiskReport PredictPairRisk(const std::vector<OrbitObject> &objects,
                           int firstIndex,
                           int secondIndex,
                           float horizonSeconds,
                           float stepSeconds)
{
    RiskReport report;
    if (firstIndex < 0 ||
        secondIndex < 0 ||
        firstIndex == secondIndex ||
        firstIndex >= static_cast<int>(objects.size()) ||
        secondIndex >= static_cast<int>(objects.size()))
    {
        FinalizeRiskReport(report);
        return report;
    }

    OrbitObject first = objects[firstIndex];
    OrbitObject second = objects[secondIndex];
    report.firstIndex = firstIndex < secondIndex ? firstIndex : secondIndex;
    report.secondIndex = firstIndex < secondIndex ? secondIndex : firstIndex;
    report.distance = Vector3Distance(first.position, second.position);
    report.closestApproachTime = 0.0f;
    report.predicted = true;

    for (float t = stepSeconds; t <= horizonSeconds + 0.001f; t += stepSeconds)
    {
        StepPredictedObject(first, stepSeconds);
        StepPredictedObject(second, stepSeconds);
        const float distance = Vector3Distance(first.position, second.position);
        if (distance < report.distance)
        {
            report.distance = distance;
            report.closestApproachTime = t;
        }
    }

    FinalizeRiskReport(report);
    return report;
}
```

- [ ] **Step 5: Route pair and global risk through prediction**

Replace `AnalyzePairRisk()` body after validation with:

```cpp
return PredictPairRisk(objects, firstIndex, secondIndex);
```

In `AnalyzeRisk()`, when calculating each candidate pair, replace raw distance comparison with:

```cpp
const RiskReport pairReport = PredictPairRisk(objects, i, j);
if (pairReport.distance < report.distance)
{
    report = pairReport;
}
```

For selected-object mode, use:

```cpp
const RiskReport pairReport = PredictPairRisk(objects, selectedObjectIndex, j);
if (pairReport.distance < report.distance)
{
    report = pairReport;
}
```

Keep `FinalizeRiskReport(report);` only for the no-pair case by checking `report.firstIndex < 0`.

- [ ] **Step 6: Run prediction test and verify it passes**

Run:

```powershell
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' tests\risk_prediction_smoke.cpp src\risk.cpp -I include -I raylib\raylib-6.0_win64_mingw-w64\include -std=c++17 -o risk_prediction_smoke.exe
cmd /c risk_prediction_smoke.exe
```

Expected: exit code `0`.

- [ ] **Step 7: Commit Task 2**

Run:

```powershell
git add include/orbit_guard.h src/risk.cpp tests/risk_prediction_smoke.cpp
git commit -m "feat: predict future closest approaches"
```

Expected: commit succeeds after preserving unrelated user changes.

---

### Task 3: Convert Avoidance Plans To Delta-V

**Files:**
- Modify: `include/orbit_guard.h`
- Modify: `src/risk.cpp`
- Modify: `src/earth_events.cpp`
- Modify: `tests/risk_prediction_smoke.cpp`
- Modify: `tests/earth_event_flow_smoke.cpp`

- [ ] **Step 1: Add failing delta-v planning assertions**

Append this block to `tests/risk_prediction_smoke.cpp` before `return ok ? 0 : 1;`:

```cpp
    AvoidancePlan plan = BuildAvoidancePlanForThreat(objects, 1);
    ok = Expect(plan.available, "future collision creates an avoidance plan") && ok;
    ok = Expect(plan.usesDeltaV, "avoidance plan uses delta-v") && ok;
    ok = Expect(Vector3Length(plan.deltaV) > 0.01f, "avoidance plan includes a nonzero delta-v") && ok;
    ok = Expect(plan.afterDistance > plan.beforeDistance, "delta-v improves predicted closest approach") && ok;
```

- [ ] **Step 2: Run prediction test and verify it fails**

Run:

```powershell
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' tests\risk_prediction_smoke.cpp src\risk.cpp -I include -I raylib\raylib-6.0_win64_mingw-w64\include -std=c++17 -o risk_prediction_smoke.exe
```

Expected: compile fails because `AvoidancePlan::usesDeltaV` and `AvoidancePlan::deltaV` do not exist.

- [ ] **Step 3: Add delta-v fields**

Modify `AvoidancePlan` in `include/orbit_guard.h`:

```cpp
bool usesDeltaV = false;
Vector3 deltaV = {};
```

Modify `AvoidanceAnimationState`:

```cpp
Vector3 deltaV = {};
bool deltaVApplied = false;
```

- [ ] **Step 4: Build candidate delta-v plans**

In `BuildAvoidancePlanWithEvaluator()` in `src/risk.cpp`, replace radius/inclination/speed candidate fields with delta-v candidates:

```cpp
struct Candidate
{
    std::string action;
    Vector3 direction;
    float magnitude;
};

const OrbitObject &sourceObject = objects[targetIndex];
Vector3 radial = Vector3Normalize(sourceObject.position);
if (Vector3Length(radial) <= 0.001f)
{
    radial = {1.0f, 0.0f, 0.0f};
}
Vector3 prograde = Vector3Normalize(sourceObject.velocity);
if (Vector3Length(prograde) <= 0.001f)
{
    prograde = CalculateOrbitTangent(sourceObject.inclinationDeg, sourceObject.angleRad);
}
Vector3 normal = Vector3Normalize(Vector3CrossProduct(radial, prograde));
if (Vector3Length(normal) <= 0.001f)
{
    normal = {0.0f, 1.0f, 0.0f};
}

const float impulse = std::max(Vector3Length(sourceObject.velocity) * 0.08f, 0.35f);
const std::vector<Candidate> candidates = {
    {"Prograde avoidance burn", prograde, impulse},
    {"Retrograde avoidance burn", Vector3Scale(prograde, -1.0f), impulse},
    {"Radial outward avoidance burn", radial, impulse},
    {"Radial inward avoidance burn", Vector3Scale(radial, -1.0f), impulse},
    {"Normal plane-change avoidance burn", normal, impulse * 0.7f},
    {"Anti-normal plane-change avoidance burn", Vector3Scale(normal, -1.0f), impulse * 0.7f},
};
```

Replace candidate application with:

```cpp
for (const Candidate &candidate : candidates)
{
    std::vector<OrbitObject> testObjects = objects;
    OrbitObject &testObject = testObjects[targetIndex];
    const Vector3 deltaV = Vector3Scale(candidate.direction, candidate.magnitude);
    testObject.velocity = Vector3Add(testObject.velocity, deltaV);

    const RiskReport candidateReport = evaluateRisk(testObjects);
    if (candidateReport.distance > bestReport.distance)
    {
        bestReport = candidateReport;
        bestObject = testObject;
        bestAction = candidate.action;
        plan.deltaV = deltaV;
        plan.usesDeltaV = true;
    }
}
```

Keep:

```cpp
plan.proposedObject = bestObject;
plan.available = bestReport.distance > report.distance + 1.0f;
```

- [ ] **Step 5: Apply delta-v during avoidance animation**

In `BeginAvoidanceAnimation()` in `src/earth_events.cpp`, add:

```cpp
animation.deltaV = plan.deltaV;
animation.deltaVApplied = false;
```

In `UpdateAvoidanceAnimation()`, before interpolating object fields, add:

```cpp
if (animation.deltaVApplied == false && Vector3Length(animation.deltaV) > 0.001f)
{
    object.velocity = Vector3Add(object.velocity, animation.deltaV);
    animation.deltaVApplied = true;
}
```

For physics-driven objects, replace the orbit-parameter interpolation block with:

```cpp
if (!object.physicsDriven)
{
    object.orbitRadius = animation.startObject.orbitRadius + (animation.endObject.orbitRadius - animation.startObject.orbitRadius) * t;
    object.inclinationDeg = animation.startObject.inclinationDeg + (animation.endObject.inclinationDeg - animation.startObject.inclinationDeg) * t;
    object.angularSpeed = animation.startObject.angularSpeed + (animation.endObject.angularSpeed - animation.startObject.angularSpeed) * t;
    RefreshObjectPosition(object);
}
```

At completion for physics-driven objects, do not assign `object = animation.endObject`. Use:

```cpp
if (!object.physicsDriven)
{
    const float liveAngleRad = object.angleRad;
    object = animation.endObject;
    object.angleRad = liveAngleRad;
    object.initialAngleDeg = NormalizeAngleDegrees(liveAngleRad / kDegToRad);
    RefreshObjectPosition(object);
}
```

- [ ] **Step 6: Run prediction and event flow tests**

Run:

```powershell
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' tests\risk_prediction_smoke.cpp src\risk.cpp src\simulation.cpp -I include -I raylib\raylib-6.0_win64_mingw-w64\include -L raylib\raylib-6.0_win64_mingw-w64\lib -std=c++17 -lraylibdll -lopengl32 -lgdi32 -lwinmm -o risk_prediction_smoke.exe
cmd /c risk_prediction_smoke.exe
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' tests\earth_event_flow_smoke.cpp src\simulation.cpp src\risk.cpp src\earth_events.cpp -I include -I raylib\raylib-6.0_win64_mingw-w64\include -std=c++17 -o earth_event_flow_smoke.exe
cmd /c earth_event_flow_smoke.exe
```

Expected: both executables exit `0`.

- [ ] **Step 7: Commit Task 3**

Run:

```powershell
git add include/orbit_guard.h src/risk.cpp src/earth_events.cpp tests/risk_prediction_smoke.cpp tests/earth_event_flow_smoke.cpp
git commit -m "feat: use delta-v avoidance burns"
```

Expected: commit succeeds after preserving unrelated user changes.

---

### Task 4: Make Impact Debris Physics-Driven

**Files:**
- Modify: `src/earth_events.cpp`
- Modify: `tests/earth_event_flow_smoke.cpp`

- [ ] **Step 1: Add failing impact debris physics assertions**

In `tests/earth_event_flow_smoke.cpp`, after the existing `"impact warning targets a spawned debris object"` assertion, add:

```cpp
    const OrbitObject &impactDebris = scriptedImpactObjects[scriptedImpactEvent.targetIndex];
    ok = Expect(impactDebris.physicsDriven, "spawned impact debris is physics driven") && ok;
    ok = Expect(Vector3Length(impactDebris.velocity) > 0.1f, "spawned impact debris has an initial velocity") && ok;
```

- [ ] **Step 2: Run event flow test and verify it fails if debris velocity is missing**

Run:

```powershell
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' tests\earth_event_flow_smoke.cpp src\simulation.cpp src\risk.cpp src\earth_events.cpp -I include -I raylib\raylib-6.0_win64_mingw-w64\include -std=c++17 -o earth_event_flow_smoke.exe
cmd /c earth_event_flow_smoke.exe
```

Expected: fails on the new velocity assertion until impact debris creation is updated.

- [ ] **Step 3: Update impact debris creation**

In `CreateImpactDebrisForPlayer()` in `src/earth_events.cpp`, after setting `object.position`, replace `SyncOrbitWithPosition(object);` with:

```cpp
SyncOrbitWithPosition(object);
const Vector3 toPlayer = Vector3Normalize(Vector3Subtract(player.position, object.position));
const float closingSpeed = std::max(Vector3Length(player.velocity) * 0.18f, 6.0f);
object.velocity = Vector3Add(player.velocity, Vector3Scale(toPlayer, closingSpeed));
object.collisionRadius = kDefaultDebrisCollisionRadius;
object.physicsDriven = true;
```

- [ ] **Step 4: Stop directly stepping warning debris**

In `UpdateActiveCollisionWarning()`, remove the call to `AdvanceThreatTowardPlayer()` for scripted impact debris. Replace the second distance check block with:

```cpp
event.timer += deltaTime;
currentDistance = DistanceBetweenObjects(objects, playerIndex, event.targetIndex);
if (currentDistance <= objects[playerIndex].collisionRadius + objects[event.targetIndex].collisionRadius)
{
    DestroyPlayerSatellite(event, objects, playerIndex, "PlayerSat made contact with debris");
    return true;
}
```

For the initial contact check in the same function, replace `kCollisionContactDistance` with:

```cpp
objects[playerIndex].collisionRadius + objects[event.targetIndex].collisionRadius
```

Keep the existing clear-distance logic for debris that moves away.

- [ ] **Step 5: Run event flow test**

Run:

```powershell
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' tests\earth_event_flow_smoke.cpp src\simulation.cpp src\risk.cpp src\earth_events.cpp -I include -I raylib\raylib-6.0_win64_mingw-w64\include -std=c++17 -o earth_event_flow_smoke.exe
cmd /c earth_event_flow_smoke.exe
```

Expected: exit code `0`.

- [ ] **Step 6: Commit Task 4**

Run:

```powershell
git add src/earth_events.cpp tests/earth_event_flow_smoke.cpp
git commit -m "feat: make impact debris physics driven"
```

Expected: commit succeeds after preserving unrelated user changes.

---

### Task 5: Update UI Labels And Prediction Text

**Files:**
- Modify: `src/simulation.cpp`
- Modify: `src/ui.cpp`
- Modify: `tests/player_satellite_smoke.cpp`

- [ ] **Step 1: Update launch field label**

In `LaunchFieldName()` in `src/simulation.cpp`, replace case `2` return value:

```cpp
case 2:
    return "Speed Bias";
```

- [ ] **Step 2: Update launch setup text**

In `DrawInfoPanel()` in `src/ui.cpp`, replace:

```cpp
DrawText(TextFormat("Speed %.2f  |  Angle %.1f", launchSettings.angularSpeed, launchSettings.initialAngleDeg),
```

with:

```cpp
DrawText(TextFormat("Speed Bias %.2fx  |  Angle %.1f",
                    SpeedBiasFromControl(launchSettings.angularSpeed),
                    launchSettings.initialAngleDeg),
```

- [ ] **Step 3: Display prediction time**

In the risk section of `DrawInfoPanel()`, after drawing the risk pair and distance, add:

```cpp
if (report.predicted)
{
    DrawText(TextFormat("Closest approach in %.0fs", report.closestApproachTime),
             contentX,
             y,
             14,
             Fade(RAYWHITE, 0.72f));
    y += 16;
}
```

Place this before the avoidance preview heading so the panel remains readable.

- [ ] **Step 4: Update control text**

In `DrawControls()` in `src/ui.cpp`, replace:

```cpp
DrawText("Tab Field   Left/Right Adjust   L Launch   Backspace Remove   A Avoid   S Report   Q Quit",
```

with:

```cpp
DrawText("Tab Field   Left/Right Adjust   L Launch   Backspace Remove   A Delta-V Avoid   S Report   Q Quit",
```

- [ ] **Step 5: Run UI-dependent compile through full build**

Run:

```powershell
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' main.cpp src\simulation.cpp src\risk.cpp src\mission.cpp src\earth_events.cpp src\ship_modes.cpp src\ui.cpp -o OrbitGuard.exe -std=c++17 -I include -I raylib\raylib-6.0_win64_mingw-w64\include -L raylib\raylib-6.0_win64_mingw-w64\lib -lraylibdll -lopengl32 -lgdi32 -lwinmm
```

Expected: exit code `0`.

- [ ] **Step 6: Commit Task 5**

Run:

```powershell
git add src/simulation.cpp src/ui.cpp tests/player_satellite_smoke.cpp
git commit -m "feat: show physics prediction details"
```

Expected: commit succeeds after preserving unrelated user changes.

---

### Task 6: Full Regression Pass

**Files:**
- Verify: all modified source and test files

- [ ] **Step 1: Build all smoke test executables**

Run:

```powershell
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' tests\ship_state_smoke.cpp src\simulation.cpp src\risk.cpp src\mission.cpp src\earth_events.cpp src\ship_modes.cpp src\ui.cpp -I include -I raylib\raylib-6.0_win64_mingw-w64\include -L raylib\raylib-6.0_win64_mingw-w64\lib -std=c++17 -lraylibdll -lopengl32 -lgdi32 -lwinmm -o ship_state_smoke.exe
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' tests\risk_scope_smoke.cpp src\risk.cpp src\simulation.cpp -I include -I raylib\raylib-6.0_win64_mingw-w64\include -L raylib\raylib-6.0_win64_mingw-w64\lib -std=c++17 -lraylibdll -lopengl32 -lgdi32 -lwinmm -o risk_scope_smoke.exe
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' tests\player_satellite_smoke.cpp src\simulation.cpp -I include -I raylib\raylib-6.0_win64_mingw-w64\include -std=c++17 -o player_satellite_smoke.exe
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' tests\orbit_layer_smoke.cpp src\simulation.cpp -I include -I raylib\raylib-6.0_win64_mingw-w64\include -L raylib\raylib-6.0_win64_mingw-w64\lib -std=c++17 -lraylibdll -lopengl32 -lgdi32 -lwinmm -o orbit_layer_smoke.exe
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' tests\mission_state_smoke.cpp src\mission.cpp -I include -I raylib\raylib-6.0_win64_mingw-w64\include -std=c++17 -o mission_state_smoke.exe
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' tests\earth_event_smoke.cpp src\simulation.cpp src\risk.cpp src\mission.cpp src\earth_events.cpp src\ui.cpp -I include -I raylib\raylib-6.0_win64_mingw-w64\include -L raylib\raylib-6.0_win64_mingw-w64\lib -std=c++17 -lraylibdll -lopengl32 -lgdi32 -lwinmm -o earth_event_smoke.exe
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' tests\earth_event_flow_smoke.cpp src\simulation.cpp src\risk.cpp src\earth_events.cpp -I include -I raylib\raylib-6.0_win64_mingw-w64\include -std=c++17 -o earth_event_flow_smoke.exe
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' tests\escape_navigation_smoke.cpp src\simulation.cpp -I include -I raylib\raylib-6.0_win64_mingw-w64\include -L raylib\raylib-6.0_win64_mingw-w64\lib -std=c++17 -lraylibdll -lopengl32 -lgdi32 -lwinmm -o escape_navigation_smoke.exe
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' tests\physics_orbit_smoke.cpp src\simulation.cpp -I include -I raylib\raylib-6.0_win64_mingw-w64\include -L raylib\raylib-6.0_win64_mingw-w64\lib -std=c++17 -lraylibdll -lopengl32 -lgdi32 -lwinmm -o physics_orbit_smoke.exe
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' tests\risk_prediction_smoke.cpp src\risk.cpp src\simulation.cpp -I include -I raylib\raylib-6.0_win64_mingw-w64\include -L raylib\raylib-6.0_win64_mingw-w64\lib -std=c++17 -lraylibdll -lopengl32 -lgdi32 -lwinmm -o risk_prediction_smoke.exe
```

Expected: each command exits `0`.

- [ ] **Step 2: Run all smoke tests**

Run:

```powershell
cmd /c "risk_scope_smoke.exe && player_satellite_smoke.exe && orbit_layer_smoke.exe && mission_state_smoke.exe && earth_event_smoke.exe && earth_event_flow_smoke.exe && escape_navigation_smoke.exe && ship_state_smoke.exe && physics_orbit_smoke.exe && risk_prediction_smoke.exe"
```

Expected: exit code `0`.

- [ ] **Step 3: Build final game executable**

Run:

```powershell
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' main.cpp src\simulation.cpp src\risk.cpp src\mission.cpp src\earth_events.cpp src\ship_modes.cpp src\ui.cpp -o OrbitGuard.exe -std=c++17 -I include -I raylib\raylib-6.0_win64_mingw-w64\include -L raylib\raylib-6.0_win64_mingw-w64\lib -lraylibdll -lopengl32 -lgdi32 -lwinmm
```

Expected: exit code `0`, producing `OrbitGuard.exe`.

- [ ] **Step 4: Commit final verification adjustments**

Run:

```powershell
git status --short
git add include/orbit_guard.h src/simulation.cpp src/risk.cpp src/earth_events.cpp src/ui.cpp main.cpp tests/physics_orbit_smoke.cpp tests/risk_prediction_smoke.cpp tests/earth_event_flow_smoke.cpp tests/player_satellite_smoke.cpp
git commit -m "test: verify physics-driven orbit gameplay"
```

Expected: commit succeeds if Task 6 required any final edits. If no files changed after Task 5, record the passing commands in the final response instead of creating an empty commit.

---

## Self-Review

- Spec coverage: The plan covers the 30-second half-orbit target, velocity-based object motion, delta-v avoidance, future closest-approach prediction, physics-driven impact debris, UI label updates, and regression testing.
- Placeholder scan: The plan contains no unresolved markers or omitted implementation steps.
- Type consistency: The plan consistently uses `velocity`, `collisionRadius`, `physicsDriven`, `predicted`, `closestApproachTime`, `usesDeltaV`, and `deltaV`.
