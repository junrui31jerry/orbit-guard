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

std::string ScanTargetName(const UnknownScanState &scan, const std::vector<OrbitObject> &objects)
{
    if (scan.objectIndex >= 0 && scan.objectIndex < static_cast<int>(objects.size()))
    {
        return objects[scan.objectIndex].name;
    }
    if (!scan.objectName.empty())
    {
        return scan.objectName;
    }
    return "unknown object";
}
} // namespace

int main()
{
    InitWindow(kScreenWidth, kScreenHeight, "OrbitGuard - 3D Collision Risk Monitor");
    // Keep ESC available for in-game navigation instead of raylib's default close shortcut.
    SetExitKey(KEY_NULL);
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
    ShipState solarShip;
    ShipState blackHoleShip;
    bool blackHoleTransferred = false;
    ResetShip(solarShip, {0.0f, 0.0f, 330.0f}, 0.0f);
    ResetShip(blackHoleShip, {0.0f, 0.0f, 330.0f}, 0.0f);

    auto returnToMainMenu = [&]() {
        gameMode = GameMode::MainMenu;
        selectedMenuItem = 0;
        showLaunchHelp = false;
        paused = false;
        actionMessage.clear();
        actionMessageTimer = 0.0f;
        exportMessageTimer = 0.0f;
    };

    while (!shouldClose)
    {
        const float deltaTime = GetFrameTime();
        const Vector2 mousePosition = GetMousePosition();
        const bool escapePressed = IsKeyPressed(KEY_ESCAPE);
        const bool escapeDown = IsKeyDown(KEY_ESCAPE);
        const bool windowCloseRequested = WindowShouldClose();
        const GameMode modeBeforeEscapeHandling = gameMode;
        ApplyEscapeAndWindowClose(gameMode, shouldClose, escapePressed, escapeDown, windowCloseRequested);
        if (modeBeforeEscapeHandling != GameMode::MainMenu && gameMode == GameMode::MainMenu)
        {
            returnToMainMenu();
        }
        if (shouldClose || gameMode != modeBeforeEscapeHandling)
        {
            continue;
        }
        if (gameMode != GameMode::MainMenu &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            IsPointInBackToMenuButton(mousePosition))
        {
            returnToMainMenu();
            continue;
        }

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
            BeginDrawing();
            ClearBackground({3, 6, 12, 255});
            DrawMainMenu(selectedMenuItem);
            EndDrawing();
            continue;
        }

        if (gameMode == GameMode::SolarSystem)
        {
            UpdateShipFromInput(solarShip, deltaTime);
            BeginDrawing();
            ClearBackground({3, 6, 12, 255});
            DrawSolarSystemMode(solarShip);
            DrawBackToMenuButton(mousePosition);
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
            DrawBackToMenuButton(mousePosition);
            EndDrawing();
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
            const Vector2 mouse = mousePosition;
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
        AvoidancePlan avoidancePlan = activeEvent.type == ImmediateEventType::CollisionWarning
                                          ? BuildAvoidancePlanForThreat(objects, activeEvent.targetIndex)
                                          : BuildAvoidancePlan(report, objects, showDemoObjects, selectedObjectIndex);
        const int objectCountBeforeEventUpdate = static_cast<int>(objects.size());
        UpdateEarthImmediateEvent(activeEvent, unknownScan, objects, report, deltaTime);
        if (objectCountBeforeEventUpdate != static_cast<int>(objects.size()) ||
            activeEvent.type == ImmediateEventType::CollisionWarning)
        {
            if (!IsValidVisibleSelection(objects, selectedObjectIndex, showDemoObjects))
            {
                selectedObjectIndex = -1;
            }
            report = AnalyzeRisk(objects, showDemoObjects, selectedObjectIndex);
            avoidancePlan = activeEvent.type == ImmediateEventType::CollisionWarning && !activeEvent.playerDestroyed
                                ? BuildAvoidancePlanForThreat(objects, activeEvent.targetIndex)
                                : BuildAvoidancePlan(report, objects, showDemoObjects, selectedObjectIndex);
        }
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
            if (activeEvent.type == ImmediateEventType::CollisionWarning)
            {
                if (activeEvent.playerDestroyed)
                {
                    actionMessage = activeEvent.result;
                }
                else if (BeginAvoidanceAnimation(avoidanceAnimation, activeEvent, objects, avoidancePlan))
                {
                    MarkMissionAvoidanceApplied(missionState);
                    actionMessage = "Avoidance burn started.";
                }
                else
                {
                    actionMessage = activeEvent.result;
                }
            }
            else if (activeEvent.type == ImmediateEventType::UnknownObject)
            {
                actionMessage = "Active event is an unknown object. Select " + ScanTargetName(unknownScan, objects) + " and press C to scan.";
            }
            else
            {
                actionMessage = missionState.hasLaunchedUserSatellite ? "No active collision warning." : "Launch PlayerSat before applying avoidance.";
            }
            actionMessageTimer = 2.8f;
        }

        if (IsKeyPressed(KEY_C))
        {
            if (CanStartUnknownScan(activeEvent, unknownScan, selectedObjectIndex))
            {
                BeginUnknownScan(activeEvent, unknownScan);
                actionMessage = "Scanning " + ScanTargetName(unknownScan, objects) + "...";
            }
            else if (unknownScan.identified)
            {
                actionMessage = "Object already identified.";
            }
            else if (unknownScan.objectActive)
            {
                actionMessage = "Select " + ScanTargetName(unknownScan, objects) + " before scanning.";
            }
            else
            {
                actionMessage = "No unknown object to scan.";
            }
            actionMessageTimer = 2.8f;
            exportMessageTimer = 0.0f;
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
        if (avoidanceAnimation.active)
        {
            DrawOrbitPath(avoidanceAnimation.startObject.orbitRadius, avoidanceAnimation.startObject.inclinationDeg, Fade(ORANGE, 0.42f));
            DrawOrbitPath(avoidanceAnimation.endObject.orbitRadius, avoidanceAnimation.endObject.inclinationDeg, Fade(LIME, 0.46f));
        }
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
        if (avoidanceAnimation.active &&
            avoidanceAnimation.objectIndex >= 0 &&
            avoidanceAnimation.objectIndex < static_cast<int>(objects.size()))
        {
            const Vector3 playerPosition = objects[avoidanceAnimation.objectIndex].position;
            DrawSphere(playerPosition, 16.0f, Fade(ORANGE, 0.34f));
            DrawSphereWires(playerPosition, 24.0f, 16, 12, Fade(GOLD, 0.72f));
        }
        if (activeEvent.playerDestroyed)
        {
            const float fade = 1.0f - ClampFloat(activeEvent.timer / 3.0f, 0.0f, 1.0f);
            DrawSphere(activeEvent.explosionPosition, 18.0f + 28.0f * (1.0f - fade), Fade(ORANGE, 0.58f * fade));
            DrawSphereWires(activeEvent.explosionPosition, 34.0f + 20.0f * (1.0f - fade), 18, 12, Fade(RED, 0.82f * fade));
        }
        EndMode3D();

        DrawImmediateEventBanner(activeEvent, unknownScan);
        DrawInfoPanel(report, objects, paused, timeScale, simulationTime, exportMessageTimer, launchSettings, avoidancePlan, missionState, earthMission, activeEvent, unknownScan, showDemoObjects, selectedObjectIndex, actionMessageTimer, actionMessage);
        DrawControls();
        if (showLaunchHelp)
        {
            DrawLaunchHelpOverlay(launchSettings);
        }
        DrawBackToMenuButton(mousePosition);
        EndDrawing();
        continue;
        }
    }

    CloseWindow();
    return 0;
}
