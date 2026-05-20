// OrbitGuard - modular C++17/raylib prototype
// Open "Project orbit guard.dev" in Dev-C++ so the raylib include/lib paths are loaded.

#include "orbit_guard.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace OrbitGuard;

namespace
{
bool IsVisibleObject(const OrbitObject &object, bool showDemoObjects)
{
    return showDemoObjects || object.userControlled;
}

bool IsValidVisibleSelection(const std::vector<OrbitObject> &objects, int index, bool showDemoObjects)
{
    return index >= 0 &&
           index < static_cast<int>(objects.size()) &&
           IsVisibleObject(objects[index], showDemoObjects);
}

bool IsScenePickArea(Vector2 mouse)
{
    return mouse.x < 870.0f && mouse.y < 640.0f;
}

int PickVisibleObject(const std::vector<OrbitObject> &objects, const Camera3D &camera, bool showDemoObjects)
{
    const Vector2 mouse = GetMousePosition();
    int bestIndex = -1;
    float bestDistanceSq = 24.0f * 24.0f;

    for (int i = 0; i < static_cast<int>(objects.size()); ++i)
    {
        if (!IsVisibleObject(objects[i], showDemoObjects))
        {
            continue;
        }

        const Vector2 screenPosition = GetWorldToScreen(objects[i].position, camera);
        const float dx = mouse.x - screenPosition.x;
        const float dy = mouse.y - screenPosition.y;
        const float distanceSq = dx * dx + dy * dy;

        if (distanceSq < bestDistanceSq)
        {
            bestDistanceSq = distanceSq;
            bestIndex = i;
        }
    }

    return bestIndex;
}
} // namespace

int main()
{
    InitWindow(kScreenWidth, kScreenHeight, "OrbitGuard - 3D Collision Risk Monitor");
    SetTargetFPS(60);

    Camera3D camera = {};
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    GameMode gameMode = GameMode::MainMenu;
    int selectedMenuItem = 0;
    OrbitCameraState cameraState;
    std::vector<OrbitObject> objects = CreateDemoObjects();
    LaunchSettings launchSettings;
    MissionState missionState;
    EarthMissionState earthMission;
    ImmediateEventState activeEvent;
    AvoidanceAnimationState avoidanceAnimation;
    UnknownScanState unknownScan;

    bool paused = false;
    bool shouldClose = false;
    float timeScale = 1.0f;
    float simulationTime = 0.0f;
    float exportMessageTimer = 0.0f;
    float actionMessageTimer = 0.0f;
    bool showLaunchHelp = false;
    bool showDemoObjects = true;
    int selectedObjectIndex = -1;
    std::string actionMessage;

    while (!WindowShouldClose() && !shouldClose)
    {
        const float deltaTime = GetFrameTime();

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

        if (gameMode == GameMode::EarthSpace)
        {
        if (IsKeyPressed(KEY_SPACE))
        {
            paused = !paused;
        }
        if (IsKeyPressed(KEY_R))
        {
            objects = CreateDemoObjects();
            simulationTime = 0.0f;
            ResetMissionState(missionState);
            earthMission = EarthMissionState{};
            ResetImmediateEvent(activeEvent);
            avoidanceAnimation = AvoidanceAnimationState{};
            unknownScan = UnknownScanState{};
            selectedObjectIndex = -1;
            exportMessageTimer = 0.0f;
            actionMessage = "Simulation reset. User satellites were cleared.";
            actionMessageTimer = 2.4f;
        }
        if (IsKeyPressed(KEY_UP))
        {
            timeScale = ClampFloat(timeScale + 0.25f, 0.25f, 8.0f);
        }
        if (IsKeyPressed(KEY_DOWN))
        {
            timeScale = ClampFloat(timeScale - 0.25f, 0.25f, 8.0f);
        }
        if (IsKeyPressed(KEY_Q))
        {
            shouldClose = true;
        }
        if (IsKeyPressed(KEY_H))
        {
            showLaunchHelp = !showLaunchHelp;
        }
        if (IsKeyPressed(KEY_D))
        {
            showDemoObjects = !showDemoObjects;
            if (!IsValidVisibleSelection(objects, selectedObjectIndex, showDemoObjects))
            {
                selectedObjectIndex = -1;
            }
            actionMessage = showDemoObjects ? "Initial satellites are visible and included in risk." : "Initial satellites are hidden and excluded from risk.";
            actionMessageTimer = 2.6f;
            exportMessageTimer = 0.0f;
        }
        if (IsKeyPressed(KEY_TAB))
        {
            launchSettings.selectedField = (launchSettings.selectedField + 1) % 4;
        }
        if (IsKeyPressed(KEY_RIGHT))
        {
            const bool largeStep = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            AdjustLaunchSettings(launchSettings, 1.0f, largeStep);
        }
        if (IsKeyPressed(KEY_LEFT))
        {
            const bool largeStep = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            AdjustLaunchSettings(launchSettings, -1.0f, largeStep);
        }
        if (IsKeyPressed(KEY_L))
        {
            selectedObjectIndex = UpsertPlayerSatellite(objects, launchSettings);
            MarkMissionLaunched(missionState);
            exportMessageTimer = 0.0f;
            actionMessage = "PlayerSat deployed into " + std::string(OrbitLayerText(ClassifyOrbitLayer(launchSettings.orbitRadius))) + ".";
            actionMessageTimer = 2.4f;
        }
        if (IsKeyPressed(KEY_BACKSPACE))
        {
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
        }

        if (!paused)
        {
            simulationTime += deltaTime * timeScale;
            UpdateObjects(objects, deltaTime, timeScale);
        }
        UpdateAvoidanceAnimation(avoidanceAnimation, activeEvent, objects, deltaTime);
        UpdateUnknownScan(activeEvent, unknownScan, deltaTime);

        UpdateOrbitCamera(cameraState, camera);

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !showLaunchHelp)
        {
            const Vector2 mouse = GetMousePosition();
            if (IsScenePickArea(mouse))
            {
                const int pickedIndex = PickVisibleObject(objects, camera, showDemoObjects);
                if (pickedIndex >= 0)
                {
                    selectedObjectIndex = pickedIndex;
                    actionMessage = "Risk target locked: " + objects[pickedIndex].name + ".";
                    actionMessageTimer = 2.2f;
                    exportMessageTimer = 0.0f;
                }
            }
        }

        if (!IsValidVisibleSelection(objects, selectedObjectIndex, showDemoObjects))
        {
            selectedObjectIndex = -1;
        }

        RiskReport report = AnalyzeRisk(objects, showDemoObjects, selectedObjectIndex);
        AvoidancePlan avoidancePlan = BuildAvoidancePlan(report, objects, showDemoObjects, selectedObjectIndex);
        UpdateMissionState(missionState, report, avoidancePlan, objects);
        if (FindPlayerSatelliteIndex(objects) >= 0)
        {
            UpdateEarthMissionAfterLaunch(earthMission, objects);
        }
        else
        {
            UpdateEarthMissionBeforeLaunch(earthMission, launchSettings);
        }

        if (IsKeyPressed(KEY_A))
        {
            if (avoidancePlan.available)
            {
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
            }
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
            actionMessageTimer = 2.8f;
        }

        if (IsKeyPressed(KEY_S))
        {
            if (missionState.hasLaunchedUserSatellite)
            {
                const bool reportSaved = ExportRiskReport(report, objects, simulationTime, timeScale, launchSettings, avoidancePlan);
                if (reportSaved)
                {
                    MarkMissionReportSaved(missionState);
                    exportMessageTimer = 2.6f;
                }
                else
                {
                    actionMessage = "Report could not be saved. Check the project folder permissions.";
                    actionMessageTimer = 2.8f;
                    exportMessageTimer = 0.0f;
                }
            }
            else
            {
                actionMessage = "Launch a user satellite before saving the mission report.";
                actionMessageTimer = 2.8f;
                exportMessageTimer = 0.0f;
            }
        }
        exportMessageTimer = std::max(0.0f, exportMessageTimer - deltaTime);
        actionMessageTimer = std::max(0.0f, actionMessageTimer - deltaTime);

        BeginDrawing();
        ClearBackground({3, 6, 12, 255});
        DrawSolarSystemBackground();

        BeginMode3D(camera);
        DrawOrbitLayerBands();
        DrawGrid(24, 24.0f);
        DrawSphere({0.0f, 0.0f, 0.0f}, kEarthRadius, {31, 108, 188, 255});
        DrawSphereWires({0.0f, 0.0f, 0.0f}, kEarthRadius + 0.5f, 24, 16, Fade(SKYBLUE, 0.45f));

        for (const OrbitObject &object : objects)
        {
            if (!IsVisibleObject(object, showDemoObjects))
            {
                continue;
            }
            DrawOrbitPath(object.orbitRadius, object.inclinationDeg, Fade(object.color, 0.26f));
        }

        DrawOrbitPath(launchSettings.orbitRadius, launchSettings.inclinationDeg, Fade(YELLOW, 0.45f));
        OrbitObject previewObject;
        previewObject.name = "Launch Preview";
        previewObject.type = ObjectType::Satellite;
        previewObject.orbitRadius = launchSettings.orbitRadius;
        previewObject.inclinationDeg = launchSettings.inclinationDeg;
        previewObject.angularSpeed = launchSettings.angularSpeed;
        previewObject.initialAngleDeg = launchSettings.initialAngleDeg;
        previewObject.angleRad = launchSettings.initialAngleDeg * kDegToRad;
        previewObject.position = CalculateOrbitPosition(previewObject.orbitRadius, previewObject.inclinationDeg, previewObject.angleRad);
        previewObject.color = Fade(YELLOW, 0.65f);
        DrawSpaceObject(previewObject, Fade(YELLOW, 0.75f));

        if (report.firstIndex >= 0 && report.secondIndex >= 0)
        {
            DrawLine3D(objects[report.firstIndex].position, objects[report.secondIndex].position, report.color);
        }

        for (int i = 0; i < static_cast<int>(objects.size()); ++i)
        {
            if (!IsVisibleObject(objects[i], showDemoObjects))
            {
                continue;
            }

            const Color frameColor = i == selectedObjectIndex ? SelectedFrameColor(activeEvent, earthMission, objects, selectedObjectIndex) : BLANK;
            DrawSpaceObject(objects[i], frameColor);
        }
        EndMode3D();

        DrawImmediateEventBanner(activeEvent, unknownScan);
        DrawInfoPanel(report, objects, paused, timeScale, simulationTime, exportMessageTimer, launchSettings, avoidancePlan, missionState, earthMission, activeEvent, unknownScan, showDemoObjects, selectedObjectIndex, actionMessageTimer, actionMessage);
        DrawControls();
        if (showLaunchHelp)
        {
            DrawLaunchHelpOverlay(launchSettings);
        }
        EndDrawing();
        continue;
        }

        BeginDrawing();
        ClearBackground({3, 6, 12, 255});
        DrawStarField();
        if (gameMode == GameMode::SolarSystem)
        {
            DrawModeTitle("Solar System Simulator", "Playable ship prototype arrives in the next task.");
        }
        else
        {
            DrawModeTitle("Black Hole World Simulator", "Playable ship prototype arrives in the next task.");
        }
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
