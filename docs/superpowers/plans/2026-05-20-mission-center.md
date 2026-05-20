# Mission Center Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a clean right-side Mission Center that guides users through configure, launch, risk review, avoidance, and Save Mission Report.

**Architecture:** Add a small mission-state model and pure helper functions in a new `src/mission.cpp`, then connect that state to `main.cpp` actions and `src/ui.cpp` rendering. Keep the right panel as one compact system center, with the detailed help overlay remaining behind `H`.

**Tech Stack:** C++17, raylib, Dev-C++/TDM-GCC, existing `compile_orbit_guard.bat`, existing `Makefile.win`.

---

## Scope Check

The spec covers one subsystem: a guided mission center inside the existing OrbitGuard prototype. It does not require separate plans for mouse input, mission history, multiple mission modes, or a report viewer.

## File Structure

- Create `src/mission.cpp`: owns mission progress rules and prompt text.
- Create `tests/mission_state_smoke.cpp`: lightweight executable smoke test for mission-state behavior.
- Modify `include/orbit_guard.h`: declares mission enums, state, helper functions, and the updated `DrawInfoPanel` signature.
- Modify `main.cpp`: updates mission state when the user launches, avoids, saves a report, removes a satellite, or resets.
- Modify `src/ui.cpp`: renders Mission Flow and Next Action inside the existing right panel.
- Modify `compile_orbit_guard.bat`: adds `src\mission.cpp` to the direct build command.
- Modify `Makefile.win`: adds `mission.o` and the compile rule.
- Modify `Project orbit guard.dev`: registers `src/mission.cpp` in the Dev-C++ project.

---

### Task 1: Add Mission State Core

**Files:**
- Modify: `include/orbit_guard.h`
- Create: `src/mission.cpp`
- Create: `tests/mission_state_smoke.cpp`

- [ ] **Step 1: Write the mission-state smoke test**

Create `tests/mission_state_smoke.cpp`:

```cpp
#include "orbit_guard.h"

#include <cassert>
#include <vector>

using namespace OrbitGuard;

int main()
{
    MissionState mission;
    ResetMissionState(mission);

    assert(mission.currentStep == MissionStep::ConfigureOrbit);
    assert(!mission.hasLaunchedUserSatellite);
    assert(!mission.hasReviewedRisk);
    assert(!mission.hasAppliedAvoidance);
    assert(!mission.hasSavedMissionReport);

    RiskReport report;
    report.level = RiskLevel::Low;
    AvoidancePlan plan;
    std::vector<OrbitObject> objects;

    UpdateMissionState(mission, report, plan, objects);
    assert(mission.currentStep == MissionStep::ConfigureOrbit);
    assert(mission.nextActionText == "Next: Adjust orbit settings, then press L to launch.");

    OrbitObject userSatellite;
    userSatellite.name = "UserSat-1";
    userSatellite.userControlled = true;
    objects.push_back(userSatellite);

    MarkMissionLaunched(mission);
    UpdateMissionState(mission, report, plan, objects);

    assert(mission.hasLaunchedUserSatellite);
    assert(mission.hasReviewedRisk);
    assert(mission.currentStep == MissionStep::SaveMissionReport);
    assert(MissionStepCompleted(mission, MissionStep::ApplyAvoidance, RiskLevel::Low));

    report.level = RiskLevel::High;
    plan.available = true;
    MarkMissionLaunched(mission);
    UpdateMissionState(mission, report, plan, objects);

    assert(mission.currentStep == MissionStep::ApplyAvoidance);
    assert(mission.nextActionText == "Next: Press A to apply the suggested avoidance action.");

    MarkMissionAvoidanceApplied(mission);
    UpdateMissionState(mission, report, plan, objects);

    assert(mission.hasAppliedAvoidance);
    assert(mission.currentStep == MissionStep::SaveMissionReport);

    MarkMissionReportSaved(mission);
    assert(mission.hasSavedMissionReport);
    assert(mission.currentStep == MissionStep::Complete);
    assert(mission.nextActionText == "Mission complete: report saved to orbit_guard_report.txt.");

    return 0;
}
```

- [ ] **Step 2: Run the smoke test compile and verify it fails before the model exists**

Run from `C:\Users\user\Documents\orbit guard`:

```powershell
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' `
  tests\mission_state_smoke.cpp `
  -std=c++17 `
  -I include `
  -I raylib\raylib-6.0_win64_mingw-w64\include `
  -o tests\mission_state_smoke.exe
```

Expected: compile failure mentioning missing symbols such as `MissionState`, `MissionStep`, or `ResetMissionState`.

- [ ] **Step 3: Add mission types and declarations**

In `include/orbit_guard.h`, add this after `struct AvoidancePlan`:

```cpp
enum class MissionStep
{
    ConfigureOrbit,
    LaunchSatellite,
    ReviewRisk,
    ApplyAvoidance,
    SaveMissionReport,
    Complete
};

struct MissionState
{
    bool hasLaunchedUserSatellite = false;
    bool hasReviewedRisk = false;
    bool hasAppliedAvoidance = false;
    bool hasSavedMissionReport = false;
    MissionStep currentStep = MissionStep::ConfigureOrbit;
    std::string nextActionText = "Next: Adjust orbit settings, then press L to launch.";
};
```

In `include/orbit_guard.h`, add these declarations after `void ApplyAvoidancePlan(std::vector<OrbitObject> &objects, const AvoidancePlan &plan);`:

```cpp
void ResetMissionState(MissionState &mission);
void MarkMissionLaunched(MissionState &mission);
void MarkMissionAvoidanceApplied(MissionState &mission);
void MarkMissionReportSaved(MissionState &mission);
void UpdateMissionState(MissionState &mission,
                        const RiskReport &report,
                        const AvoidancePlan &avoidancePlan,
                        const std::vector<OrbitObject> &objects);
bool MissionStepCompleted(const MissionState &mission, MissionStep step, RiskLevel currentRiskLevel);
const char *MissionStepTitle(MissionStep step);
const char *MissionStepStatusText(const MissionState &mission, MissionStep step, RiskLevel currentRiskLevel);
```

- [ ] **Step 4: Implement mission helpers**

Create `src/mission.cpp`:

```cpp
#include "orbit_guard.h"

namespace OrbitGuard
{
namespace
{
bool HasUserSatellite(const std::vector<OrbitObject> &objects)
{
    for (const OrbitObject &object : objects)
    {
        if (object.userControlled)
        {
            return true;
        }
    }
    return false;
}
} // namespace

void ResetMissionState(MissionState &mission)
{
    mission.hasLaunchedUserSatellite = false;
    mission.hasReviewedRisk = false;
    mission.hasAppliedAvoidance = false;
    mission.hasSavedMissionReport = false;
    mission.currentStep = MissionStep::ConfigureOrbit;
    mission.nextActionText = "Next: Adjust orbit settings, then press L to launch.";
}

void MarkMissionLaunched(MissionState &mission)
{
    mission.hasLaunchedUserSatellite = true;
    mission.hasReviewedRisk = false;
    mission.hasAppliedAvoidance = false;
    mission.hasSavedMissionReport = false;
    mission.currentStep = MissionStep::ReviewRisk;
    mission.nextActionText = "Next: Review the closest-pair risk level.";
}

void MarkMissionAvoidanceApplied(MissionState &mission)
{
    mission.hasAppliedAvoidance = true;
    mission.hasSavedMissionReport = false;
    mission.currentStep = MissionStep::SaveMissionReport;
    mission.nextActionText = "Next: Review the improved distance, then press S to save the mission report.";
}

void MarkMissionReportSaved(MissionState &mission)
{
    mission.hasSavedMissionReport = true;
    mission.currentStep = MissionStep::Complete;
    mission.nextActionText = "Mission complete: report saved to orbit_guard_report.txt.";
}

void UpdateMissionState(MissionState &mission,
                        const RiskReport &report,
                        const AvoidancePlan &avoidancePlan,
                        const std::vector<OrbitObject> &objects)
{
    if (!HasUserSatellite(objects))
    {
        ResetMissionState(mission);
        return;
    }

    mission.hasLaunchedUserSatellite = true;
    mission.hasReviewedRisk = true;

    if (mission.hasSavedMissionReport)
    {
        mission.currentStep = MissionStep::Complete;
        mission.nextActionText = "Mission complete: report saved to orbit_guard_report.txt.";
        return;
    }

    if (report.level != RiskLevel::Low && !mission.hasAppliedAvoidance)
    {
        mission.currentStep = MissionStep::ApplyAvoidance;
        mission.nextActionText = avoidancePlan.available
                                      ? "Next: Press A to apply the suggested avoidance action."
                                      : "Next: No quick avoidance found. Adjust orbit settings and relaunch.";
        return;
    }

    mission.currentStep = MissionStep::SaveMissionReport;
    mission.nextActionText = mission.hasAppliedAvoidance
                                  ? "Next: Review the improved distance, then press S to save the mission report."
                                  : "Next: Risk is low. Save the mission report when ready.";
}

bool MissionStepCompleted(const MissionState &mission, MissionStep step, RiskLevel currentRiskLevel)
{
    switch (step)
    {
    case MissionStep::ConfigureOrbit:
    case MissionStep::LaunchSatellite:
        return mission.hasLaunchedUserSatellite;
    case MissionStep::ReviewRisk:
        return mission.hasReviewedRisk;
    case MissionStep::ApplyAvoidance:
        return mission.hasAppliedAvoidance || (mission.hasReviewedRisk && currentRiskLevel == RiskLevel::Low);
    case MissionStep::SaveMissionReport:
    case MissionStep::Complete:
        return mission.hasSavedMissionReport;
    default:
        return false;
    }
}

const char *MissionStepTitle(MissionStep step)
{
    switch (step)
    {
    case MissionStep::ConfigureOrbit:
        return "Configure Orbit";
    case MissionStep::LaunchSatellite:
        return "Launch Satellite";
    case MissionStep::ReviewRisk:
        return "Review Risk";
    case MissionStep::ApplyAvoidance:
        return "Apply Avoidance";
    case MissionStep::SaveMissionReport:
        return "Save Mission Report";
    case MissionStep::Complete:
        return "Mission Complete";
    default:
        return "Mission";
    }
}

const char *MissionStepStatusText(const MissionState &mission, MissionStep step, RiskLevel currentRiskLevel)
{
    if (MissionStepCompleted(mission, step, currentRiskLevel))
    {
        return "Done";
    }
    if (mission.currentStep == step)
    {
        return "Current";
    }
    return "Waiting";
}
} // namespace OrbitGuard
```

- [ ] **Step 5: Compile and run the smoke test**

Run:

```powershell
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' `
  tests\mission_state_smoke.cpp `
  src\mission.cpp `
  -std=c++17 `
  -I include `
  -I raylib\raylib-6.0_win64_mingw-w64\include `
  -o tests\mission_state_smoke.exe

& .\tests\mission_state_smoke.exe
```

Expected: compile succeeds, executable exits with code `0`, and no assertion text appears.

---

### Task 2: Add Mission File to Build Configuration

**Files:**
- Modify: `compile_orbit_guard.bat`
- Modify: `Makefile.win`
- Modify: `Project orbit guard.dev`

- [ ] **Step 1: Update direct compile script**

In `compile_orbit_guard.bat`, replace the compile command with:

```bat
"%DEVCPP_BIN%\g++.exe" "%~dp0main.cpp" "%~dp0src\simulation.cpp" "%~dp0src\risk.cpp" "%~dp0src\mission.cpp" "%~dp0src\ui.cpp" -o "%~dp0OrbitGuard.exe" -std=c++17 -I "%~dp0include" -I "%RAYLIB_ROOT%\include" -L "%RAYLIB_ROOT%\lib" -lraylibdll -lopengl32 -lgdi32 -lwinmm
```

- [ ] **Step 2: Update Makefile object lists**

In `Makefile.win`, replace:

```makefile
OBJ      = main.o simulation.o risk.o ui.o
LINKOBJ  = main.o simulation.o risk.o ui.o
```

with:

```makefile
OBJ      = main.o simulation.o risk.o mission.o ui.o
LINKOBJ  = main.o simulation.o risk.o mission.o ui.o
```

- [ ] **Step 3: Add the Makefile compile rule**

In `Makefile.win`, add this rule between `risk.o` and `ui.o`:

```makefile
mission.o: src/mission.cpp include/orbit_guard.h
	$(CPP) -c src/mission.cpp -o mission.o $(CXXFLAGS)
```

- [ ] **Step 4: Register the file in the Dev-C++ project**

In `Project orbit guard.dev`, replace:

```ini
UnitCount=5
```

with:

```ini
UnitCount=6
```

Add this block after `[Unit4]` and before the existing header unit:

```ini
[Unit5]
FileName=src/mission.cpp
CompileCpp=1
Folder=Source Files
Compile=1
Link=1
Priority=1000
OverrideBuildCmd=0
BuildCmd=
```

Rename the existing header section from `[Unit5]` to `[Unit6]`, leaving its body unchanged:

```ini
[Unit6]
FileName=include/orbit_guard.h
CompileCpp=1
Folder=Header Files
Compile=0
Link=0
Priority=1000
OverrideBuildCmd=0
BuildCmd=
```

- [ ] **Step 5: Compile the app with mission.cpp included**

Run:

```powershell
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' `
  main.cpp `
  src\simulation.cpp `
  src\risk.cpp `
  src\mission.cpp `
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

Expected: compile succeeds and writes `OrbitGuard.exe`.

---

### Task 3: Connect Mission State to Main Loop

**Files:**
- Modify: `main.cpp`

- [ ] **Step 1: Add mission state variable**

In `main.cpp`, after:

```cpp
LaunchSettings launchSettings;
```

add:

```cpp
MissionState missionState;
```

- [ ] **Step 2: Reset mission state on reset**

Inside the `if (IsKeyPressed(KEY_R))` block, after:

```cpp
simulationTime = 0.0f;
```

add:

```cpp
ResetMissionState(missionState);
```

- [ ] **Step 3: Mark launch progress**

Inside the `if (IsKeyPressed(KEY_L))` block, after:

```cpp
objects.push_back(CreateUserSatellite(launchSettings));
```

add:

```cpp
MarkMissionLaunched(missionState);
```

- [ ] **Step 4: Update mission state after risk analysis**

After:

```cpp
RiskReport report = AnalyzeRisk(objects);
AvoidancePlan avoidancePlan = BuildAvoidancePlan(report, objects);
```

add:

```cpp
UpdateMissionState(missionState, report, avoidancePlan, objects);
```

- [ ] **Step 5: Improve avoidance feedback and mark applied avoidance**

Replace the existing `else` branch inside `if (IsKeyPressed(KEY_A))` with:

```cpp
            else
            {
                if (!missionState.hasLaunchedUserSatellite)
                {
                    actionMessage = "Launch a user satellite before applying avoidance.";
                }
                else if (report.level == RiskLevel::Low)
                {
                    actionMessage = "Risk is low. Avoidance is not required.";
                }
                else
                {
                    actionMessage = "No quick avoidance found. Adjust orbit settings and relaunch.";
                }
            }
```

Inside the successful avoidance branch, after:

```cpp
ApplyAvoidancePlan(objects, avoidancePlan);
```

add:

```cpp
MarkMissionAvoidanceApplied(missionState);
```

After the successful branch recalculates `report` and `avoidancePlan`, add:

```cpp
UpdateMissionState(missionState, report, avoidancePlan, objects);
```

- [ ] **Step 6: Mark report saving**

Inside the `if (IsKeyPressed(KEY_S))` block, after:

```cpp
ExportRiskReport(report, objects, simulationTime, timeScale, launchSettings, avoidancePlan);
```

add:

```cpp
MarkMissionReportSaved(missionState);
```

- [ ] **Step 7: Compile and run the mission smoke test again**

Run:

```powershell
& 'C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe' `
  tests\mission_state_smoke.cpp `
  src\mission.cpp `
  -std=c++17 `
  -I include `
  -I raylib\raylib-6.0_win64_mingw-w64\include `
  -o tests\mission_state_smoke.exe

& .\tests\mission_state_smoke.exe
```

Expected: smoke test exits with code `0`.

- [ ] **Step 8: Compile the app**

Run the compile command from Task 2 Step 5.

Expected: compile succeeds and writes `OrbitGuard.exe`.

---

### Task 4: Render Mission Flow in the Right Panel

**Files:**
- Modify: `include/orbit_guard.h`
- Modify: `main.cpp`
- Modify: `src/ui.cpp`

- [ ] **Step 1: Update the DrawInfoPanel declaration**

In `include/orbit_guard.h`, replace the `DrawInfoPanel` declaration with:

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
                   float actionMessageTimer,
                   const std::string &actionMessage);
```

- [ ] **Step 2: Pass mission state from main**

In `main.cpp`, replace:

```cpp
DrawInfoPanel(report, objects, paused, timeScale, simulationTime, exportMessageTimer, launchSettings, avoidancePlan, actionMessageTimer, actionMessage);
```

with:

```cpp
DrawInfoPanel(report, objects, paused, timeScale, simulationTime, exportMessageTimer, launchSettings, avoidancePlan, missionState, actionMessageTimer, actionMessage);
```

- [ ] **Step 3: Add mission row helpers**

In `src/ui.cpp`, add these helper functions before `void DrawInfoPanel(...)`:

```cpp
namespace
{
Color MissionStatusColor(const char *statusText)
{
    const std::string status(statusText);
    if (status == "Done")
    {
        return LIME;
    }
    if (status == "Current")
    {
        return YELLOW;
    }
    return Fade(RAYWHITE, 0.42f);
}

void DrawMissionRow(int x, int y, int width, MissionStep step, const MissionState &missionState, RiskLevel riskLevel)
{
    const char *statusText = MissionStepStatusText(missionState, step, riskLevel);
    const Color markerColor = MissionStatusColor(statusText);
    const Color textColor = std::string(statusText) == "Waiting" ? Fade(RAYWHITE, 0.58f) : RAYWHITE;

    DrawCircle(x + 8, y + 9, 5.0f, markerColor);
    DrawText(MissionStepTitle(step), x + 22, y, 15, textColor);
    DrawText(statusText, x + width - 72, y, 14, markerColor);
}
} // namespace
```

- [ ] **Step 4: Replace DrawInfoPanel with the mission-center layout**

In `src/ui.cpp`, replace the full `DrawInfoPanel` function with:

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
                   float actionMessageTimer,
                   const std::string &actionMessage)
{
    constexpr int panelX = 890;
    constexpr int panelWidth = 374;
    DrawRectangle(panelX, 18, panelWidth, 616, {9, 15, 23, 230});
    DrawRectangleLines(panelX, 18, panelWidth, 616, Fade(SKYBLUE, 0.35f));

    int y = 36;
    DrawText("OrbitGuard", panelX + 22, y, 30, RAYWHITE);
    y += 34;
    DrawText("Mission Control Center", panelX + 22, y, 17, Fade(RAYWHITE, 0.72f));
    y += 34;

    DrawText("System Status", panelX + 22, y, 17, Fade(RAYWHITE, 0.70f));
    y += 23;
    DrawText(TextFormat("%s  Objects: %d", paused ? "PAUSED" : "RUNNING", static_cast<int>(objects.size())),
             panelX + 22, y, 16, paused ? GOLD : LIME);
    y += 20;
    DrawText(TextFormat("Time %ss  Speed %sx", FormatFloat(simulationTime, 1).c_str(), FormatFloat(timeScale, 2).c_str()),
             panelX + 22, y, 16, RAYWHITE);
    y += 28;

    DrawText("Mission Flow", panelX + 22, y, 17, Fade(RAYWHITE, 0.70f));
    y += 24;
    const MissionStep steps[] = {
        MissionStep::ConfigureOrbit,
        MissionStep::LaunchSatellite,
        MissionStep::ReviewRisk,
        MissionStep::ApplyAvoidance,
        MissionStep::SaveMissionReport};
    for (const MissionStep step : steps)
    {
        DrawMissionRow(panelX + 24, y, panelWidth - 48, step, missionState, report.level);
        y += 23;
    }
    y += 12;

    DrawText("Launch Setup", panelX + 22, y, 17, Fade(RAYWHITE, 0.70f));
    y += 22;
    DrawText(TextFormat("Selected: %s", LaunchFieldName(launchSettings.selectedField)), panelX + 22, y, 15, YELLOW);
    y += 18;
    DrawText(TextFormat("Radius %.1f  Inclination %.1f", launchSettings.orbitRadius, launchSettings.inclinationDeg), panelX + 22, y, 14, RAYWHITE);
    y += 17;
    DrawText(TextFormat("Speed %.2f  Angle %.1f", launchSettings.angularSpeed, launchSettings.initialAngleDeg), panelX + 22, y, 14, RAYWHITE);
    y += 26;

    DrawText("Risk Insight", panelX + 22, y, 17, Fade(RAYWHITE, 0.70f));
    y += 22;
    DrawWrappedText(PairName(report, objects), panelX + 22, y, panelWidth - 44, 16, 3, RAYWHITE);
    y += 38;
    DrawText((FormatFloat(report.distance, 2) + " simulated km").c_str(), panelX + 22, y, 17, RAYWHITE);
    y += 24;
    DrawRectangle(panelX + 22, y, 118, 30, Fade(report.color, 0.88f));
    DrawText(RiskLevelText(report.level), panelX + 38, y + 7, 17, BLACK);
    y += 40;

    DrawText("AI Advice", panelX + 22, y, 17, Fade(RAYWHITE, 0.70f));
    y += 21;
    DrawWrappedText(report.advice, panelX + 22, y, panelWidth - 44, 14, 3, RAYWHITE);
    y += 52;

    DrawText("Avoidance Preview", panelX + 22, y, 17, Fade(RAYWHITE, 0.70f));
    y += 21;
    DrawWrappedText(avoidancePlan.action, panelX + 22, y, panelWidth - 44, 14, 3, RAYWHITE);
    y += 42;
    DrawText(TextFormat("Before %.1f  After %.1f", avoidancePlan.beforeDistance, avoidancePlan.afterDistance),
             panelX + 22, y, 14, avoidancePlan.available ? LIME : Fade(RAYWHITE, 0.70f));

    const int promptY = 558;
    DrawRectangle(panelX + 18, promptY, panelWidth - 36, 62, {18, 29, 43, 245});
    DrawRectangleLines(panelX + 18, promptY, panelWidth - 36, 62, Fade(SKYBLUE, 0.26f));
    DrawText("Next Action", panelX + 32, promptY + 9, 15, Fade(RAYWHITE, 0.70f));

    std::string prompt = missionState.nextActionText;
    Color promptColor = SKYBLUE;
    if (exportMessageTimer > 0.0f)
    {
        prompt = "Mission complete: report saved to orbit_guard_report.txt.";
        promptColor = LIME;
    }
    else if (actionMessageTimer > 0.0f && !actionMessage.empty())
    {
        prompt = actionMessage;
    }

    DrawWrappedText(prompt, panelX + 32, promptY + 30, panelWidth - 64, 14, 3, promptColor);
}
```

- [ ] **Step 5: Compile the app**

Run the compile command from Task 2 Step 5.

Expected: compile succeeds and writes `OrbitGuard.exe`.

---

### Task 5: Manual Verification and Final Cleanup

**Files:**
- Read: `OrbitGuard.exe`
- Read: `orbit_guard_report.txt`
- Modify if needed: files changed in Tasks 1-4

- [ ] **Step 1: Run the smoke test**

Run:

```powershell
& .\tests\mission_state_smoke.exe
```

Expected: executable exits with code `0`.

- [ ] **Step 2: Launch the app**

Run:

```powershell
& .\OrbitGuard.exe
```

Expected: app opens in a raylib window titled `OrbitGuard - 3D Collision Risk Monitor`.

- [ ] **Step 3: Verify initial mission state**

Manual checks:

- Right panel subtitle reads `Mission Control Center`.
- Mission Flow shows `Configure Orbit` as `Current`.
- `Launch Satellite`, `Review Risk`, `Apply Avoidance`, and `Save Mission Report` show `Waiting`.
- Next Action reads `Next: Adjust orbit settings, then press L to launch.`

- [ ] **Step 4: Verify launch flow**

Manual actions:

- Press `Tab` at least once.
- Press `Right` at least once.
- Press `L`.

Expected:

- `Configure Orbit` and `Launch Satellite` show `Done`.
- `Review Risk` shows `Done` after the frame updates.
- If risk is low, `Save Mission Report` becomes `Current`.
- If risk is medium or high, `Apply Avoidance` becomes `Current`.

- [ ] **Step 5: Verify avoidance flow**

Manual action:

- If `Apply Avoidance` is current and the Next Action says to press `A`, press `A`.

Expected:

- `Apply Avoidance` shows `Done`.
- `Save Mission Report` becomes `Current`.
- Avoidance preview still shows before and after distance.

- [ ] **Step 6: Verify report saving**

Manual action:

- Press `S`.

Expected:

- `Save Mission Report` shows `Done`.
- Next Action says `Mission complete: report saved to orbit_guard_report.txt.`
- `orbit_guard_report.txt` exists in `C:\Users\user\Documents\orbit guard`.
- The report contains `OrbitGuard Risk Analysis Report`.

- [ ] **Step 7: Verify reset flow**

Manual action:

- Press `R`.

Expected:

- Mission Flow returns to `Configure Orbit` as `Current`.
- Next Action returns to `Next: Adjust orbit settings, then press L to launch.`

- [ ] **Step 8: Verify out-of-order avoidance guidance**

Manual actions:

- Press `R`.
- Press `A` before launching.

Expected:

- Next Action or action feedback says `Launch a user satellite before applying avoidance.`
- The app keeps running.

- [ ] **Step 9: Check for incomplete markers in changed source**

Run:

```powershell
$markers = @('TB' + 'D', 'TO' + 'DO', 'FIX' + 'ME')
Select-String -Path main.cpp,include\orbit_guard.h,src\mission.cpp,src\ui.cpp,compile_orbit_guard.bat,Makefile.win,'Project orbit guard.dev' -Pattern $markers
```

Expected: no matches.

- [ ] **Step 10: Record Git status when Git is available**

Run:

```powershell
git status --short
```

Expected in this workspace today: PowerShell reports that `git` is not recognized. If Git is installed in the execution environment, expected changed files are:

```text
M Project orbit guard.dev
M Makefile.win
M compile_orbit_guard.bat
M include/orbit_guard.h
M main.cpp
M src/ui.cpp
A src/mission.cpp
A tests/mission_state_smoke.cpp
```

---

## Self-Review

- Spec coverage: the plan covers mission steps, state model, Next Action prompt, friendly out-of-order guidance, compact right-panel rendering, report save confirmation, reset behavior, and manual verification.
- Unfinished marker scan: the plan avoids common unfinished-work markers.
- Type consistency: `MissionState`, `MissionStep`, `UpdateMissionState`, `MissionStepCompleted`, and the updated `DrawInfoPanel` signature are named consistently across all tasks.
