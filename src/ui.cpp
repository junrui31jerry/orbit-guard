#include "orbit_guard.h"

#include <fstream>
#include <iomanip>
#include <sstream>

namespace OrbitGuard
{
std::string FormatFloat(float value, int precision)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

std::string PairName(const RiskReport &report, const std::vector<OrbitObject> &objects)
{
    if (report.firstIndex < 0 ||
        report.secondIndex < 0 ||
        report.firstIndex >= static_cast<int>(objects.size()) ||
        report.secondIndex >= static_cast<int>(objects.size()))
    {
        return "N/A";
    }

    return objects[report.firstIndex].name + " / " + objects[report.secondIndex].name;
}

bool ExportRiskReport(const RiskReport &report,
                      const std::vector<OrbitObject> &objects,
                      float simulationTime,
                      float timeScale,
                      const LaunchSettings &launchSettings,
                      const AvoidancePlan &avoidancePlan)
{
    std::ofstream file("orbit_guard_report.txt");
    if (!file)
    {
        return false;
    }

    file << "OrbitGuard Risk Analysis Report\n";
    file << "================================\n\n";
    file << "Simulation time: " << FormatFloat(simulationTime, 2) << " seconds\n";
    file << "Time scale: " << FormatFloat(timeScale, 2) << "x\n";
    file << "Object count: " << objects.size() << "\n";
    file << "Closest pair: " << PairName(report, objects) << "\n";
    file << "Minimum distance: " << FormatFloat(report.distance, 2) << " simulated km\n";
    file << "Risk level: " << RiskLevelText(report.level) << "\n";
    file << "AI recommendation: " << report.advice << "\n\n";

    file << "User launch settings\n";
    file << "--------------------\n";
    file << "Orbit radius: " << FormatFloat(launchSettings.orbitRadius, 1) << "\n";
    file << "Inclination: " << FormatFloat(launchSettings.inclinationDeg, 1) << "\n";
    file << "Angular speed: " << FormatFloat(launchSettings.angularSpeed, 2) << "\n";
    file << "Initial angle: " << FormatFloat(launchSettings.initialAngleDeg, 1) << "\n\n";

    file << "Avoidance preview\n";
    file << "-----------------\n";
    file << "Action: " << avoidancePlan.action << "\n";
    file << "Before distance: " << FormatFloat(avoidancePlan.beforeDistance, 2) << "\n";
    file << "After distance: " << FormatFloat(avoidancePlan.afterDistance, 2) << "\n";
    file << "After risk level: " << RiskLevelText(avoidancePlan.afterLevel) << "\n\n";

    file << "Object positions\n";
    file << "----------------\n";

    for (const OrbitObject &object : objects)
    {
        file << std::left << std::setw(12) << object.name
             << " type=" << (object.type == ObjectType::Satellite ? "satellite" : "debris")
             << " user_controlled=" << (object.userControlled ? "yes" : "no")
             << " radius=" << std::setw(7) << FormatFloat(object.orbitRadius, 1)
             << " inclination=" << std::setw(7) << FormatFloat(object.inclinationDeg, 1)
             << " pos=(" << FormatFloat(object.position.x, 1) << ", "
             << FormatFloat(object.position.y, 1) << ", "
             << FormatFloat(object.position.z, 1) << ")\n";
    }

    file.flush();
    return static_cast<bool>(file);
}

void DrawStarField()
{
    for (int i = 0; i < 180; ++i)
    {
        const int x = (i * 73 + 19) % kScreenWidth;
        const int y = (i * 151 + 31) % kScreenHeight;
        const unsigned char brightness = static_cast<unsigned char>(100 + (i * 37) % 155);
        DrawPixel(x, y, {brightness, brightness, brightness, 255});
    }
}

void DrawOrbitPath(float radius, float inclinationDeg, Color color)
{
    constexpr int segments = 160;
    Vector3 previous = CalculateOrbitPosition(radius, inclinationDeg, 0.0f);

    for (int step = 1; step <= segments; ++step)
    {
        const float angle = (2.0f * kPi * step) / static_cast<float>(segments);
        const Vector3 current = CalculateOrbitPosition(radius, inclinationDeg, angle);
        DrawLine3D(previous, current, color);
        previous = current;
    }
}

void DrawSpaceObject(const OrbitObject &object, Color frameColor)
{
    const float size = object.type == ObjectType::Satellite ? 8.0f : 5.5f;
    const bool impactDebris = object.type == ObjectType::Debris && object.name.rfind("Impact-Debris", 0) == 0;

    if (object.type == ObjectType::Satellite)
    {
        DrawCubeV(object.position, {size * 1.4f, size * 0.75f, size * 0.75f}, object.color);
        DrawCubeWiresV(object.position, {size * 1.4f, size * 0.75f, size * 0.75f}, WHITE);
    }
    else if (impactDebris)
    {
        const float spike = size * 2.2f;
        DrawCubeV(object.position, {size * 1.25f, size * 1.25f, size * 1.25f}, object.color);
        DrawCubeWiresV(object.position, {size * 1.25f, size * 1.25f, size * 1.25f}, Fade(WHITE, 0.82f));
        DrawLine3D({object.position.x - spike, object.position.y, object.position.z},
                   {object.position.x + spike, object.position.y, object.position.z},
                   Fade(RED, 0.88f));
        DrawLine3D({object.position.x, object.position.y - spike, object.position.z},
                   {object.position.x, object.position.y + spike, object.position.z},
                   Fade(RED, 0.88f));
        DrawLine3D({object.position.x, object.position.y, object.position.z - spike},
                   {object.position.x, object.position.y, object.position.z + spike},
                   Fade(RED, 0.88f));
    }
    else
    {
        DrawSphere(object.position, size, object.color);
        DrawSphereWires(object.position, size, 8, 8, Fade(WHITE, 0.55f));
    }

    if (frameColor.a > 0)
    {
        DrawSphereWires(object.position, size + 6.0f, 16, 12, frameColor);
    }
}

void DrawBackToMenuButton(Vector2 mousePosition)
{
    const Rectangle bounds = BackToMenuButtonBounds();
    const bool hovered = IsPointInBackToMenuButton(mousePosition);
    const Color fill = hovered ? Color{28, 56, 82, 235} : Color{8, 16, 25, 220};
    const Color accent = hovered ? RAYWHITE : Fade(SKYBLUE, 0.86f);
    const float centerY = bounds.y + bounds.height * 0.5f;
    const float left = bounds.x + 12.0f;
    const float right = bounds.x + bounds.width - 11.0f;

    DrawRectangleRec(bounds, fill);
    DrawRectangleLinesEx(bounds, 2.0f, Fade(SKYBLUE, hovered ? 0.95f : 0.56f));
    DrawLineEx({right, centerY}, {left + 4.0f, centerY}, 3.0f, accent);
    DrawLineEx({left + 5.0f, centerY}, {left + 16.0f, centerY - 11.0f}, 3.0f, accent);
    DrawLineEx({left + 5.0f, centerY}, {left + 16.0f, centerY + 11.0f}, 3.0f, accent);
}

int DrawWrappedText(const std::string &text, int x, int y, int maxWidth, int fontSize, int spacing, Color color)
{
    std::istringstream words(text);
    std::string word;
    std::string line;
    int currentY = y;
    int lineCount = 0;

    while (words >> word)
    {
        const std::string trial = line.empty() ? word : line + " " + word;
        if (MeasureText(trial.c_str(), fontSize) > maxWidth && !line.empty())
        {
            DrawText(line.c_str(), x, currentY, fontSize, color);
            currentY += fontSize + spacing;
            lineCount++;
            line = word;
        }
        else
        {
            line = trial;
        }
    }

    if (!line.empty())
    {
        DrawText(line.c_str(), x, currentY, fontSize, color);
        lineCount++;
    }

    return lineCount == 0 ? 0 : lineCount * fontSize + (lineCount - 1) * spacing;
}

namespace
{
int CountVisibleObjects(const std::vector<OrbitObject> &objects, bool showDemoObjects)
{
    int count = 0;
    for (const OrbitObject &object : objects)
    {
        if (showDemoObjects || object.userControlled)
        {
            count++;
        }
    }
    return count;
}

std::string SelectedObjectName(const std::vector<OrbitObject> &objects, int selectedObjectIndex)
{
    if (selectedObjectIndex < 0 || selectedObjectIndex >= static_cast<int>(objects.size()))
    {
        return "None - click a visible satellite";
    }
    return objects[selectedObjectIndex].name;
}

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
    DrawRectangle(76, 18, 430, 64, {4, 8, 14, 190});
    DrawText(title, 94, 30, 26, RAYWHITE);
    DrawText(subtitle, 96, 60, 15, Fade(RAYWHITE, 0.68f));
}

void DrawSolarSystemBackground()
{
    DrawStarField();
    DrawCircleGradient({88.0f, 128.0f}, 92.0f, Color{255, 183, 82, 82}, Color{255, 183, 82, 0});
    DrawCircle(86, 126, 28.0f, Color{255, 213, 128, 170});
    DrawCircleGradient({1130.0f, 148.0f}, 26.0f, Color{201, 105, 78, 120}, Color{201, 105, 78, 0});
    DrawCircle(1130, 148, 10.0f, Color{185, 88, 63, 180});
    DrawCircleGradient({1080.0f, 570.0f}, 42.0f, Color{82, 148, 215, 70}, Color{82, 148, 215, 0});
}

void DrawOrbitLayerBands()
{
    DrawOrbitPath(85.0f, 0.0f, Fade(SKYBLUE, 0.16f));
    DrawOrbitPath(145.0f, 0.0f, Fade(SKYBLUE, 0.22f));
    DrawOrbitPath(230.0f, 0.0f, Fade(GOLD, 0.22f));
    DrawOrbitPath(330.0f, 0.0f, Fade(VIOLET, 0.20f));
}

void DrawImmediateEventBanner(const ImmediateEventState &event, const UnknownScanState &scan)
{
    if (event.type == ImmediateEventType::None || event.phase == ImmediateEventPhase::Inactive)
    {
        return;
    }

    const Color accent = event.type == ImmediateEventType::CollisionWarning ? RED : Color{255, 72, 236, 255};
    DrawRectangle(310, 24, 590, 86, {7, 12, 20, 235});
    DrawRectangle(310, 24, 8, 86, accent);
    DrawRectangleLines(310, 24, 590, 86, Fade(accent, 0.62f));
    DrawText(event.title.c_str(), 334, 36, 24, accent);
    DrawText(event.detail.c_str(), 336, 64, 16, RAYWHITE);

    std::string action = event.action;
    if (event.type == ImmediateEventType::UnknownObject && scan.scanning)
    {
        action = "Scanning progress: " + FormatFloat(scan.progress * 100.0f, 0) + "%";
    }
    if (!event.result.empty())
    {
        action = event.result;
    }
    DrawText(action.c_str(), 336, 88, 15, Fade(RAYWHITE, 0.78f));
}

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
            return Color{255, 72, 236, 255};
        }
    }

    if (objects[selectedObjectIndex].userControlled && earthMission.playerSatDeployed)
    {
        return YELLOW;
    }

    return BLANK;
}

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
                   const std::string &actionMessage)
{
    (void)activeEvent;
    (void)unknownScan;

    constexpr int panelX = 890;
    constexpr int panelWidth = 374;
    constexpr int panelY = 18;
    constexpr int panelHeight = 616;
    constexpr int contentX = panelX + 22;
    constexpr int contentWidth = panelWidth - 44;
    const Color sectionColor = Fade(RAYWHITE, 0.70f);
    const Color mutedColor = Fade(RAYWHITE, 0.62f);

    DrawRectangle(panelX, panelY, panelWidth, panelHeight, {9, 15, 23, 230});
    DrawRectangleLines(panelX, panelY, panelWidth, panelHeight, Fade(SKYBLUE, 0.35f));

    int y = panelY + 16;
    DrawText("OrbitGuard", contentX, y, 28, RAYWHITE);
    y += 31;
    DrawText("Focused Risk Monitor", contentX, y, 16, Fade(RAYWHITE, 0.74f));
    y += 27;

    DrawText("System Status", contentX, y, 15, sectionColor);
    y += 18;
    DrawText(TextFormat("%s  |  Visible %d/%d", paused ? "PAUSED" : "RUNNING", CountVisibleObjects(objects, showDemoObjects), static_cast<int>(objects.size())),
             contentX,
             y,
             14,
             paused ? GOLD : LIME);
    y += 17;
    DrawText(TextFormat("Time %ss  |  Speed %sx", FormatFloat(simulationTime, 1).c_str(), FormatFloat(timeScale, 2).c_str()),
             contentX,
             y,
             14,
             RAYWHITE);
    y += 17;
    DrawText(showDemoObjects ? "Initial objects: Shown (D)" : "Initial objects: Hidden (D)",
             contentX,
             y,
             14,
             showDemoObjects ? SKYBLUE : GOLD);
    y += 21;

    DrawText("Launch Setup", contentX, y, 15, sectionColor);
    y += 18;
    DrawText(TextFormat("Selected: %s", LaunchFieldName(launchSettings.selectedField)), contentX, y, 14, YELLOW);
    y += 16;
    DrawText(TextFormat("Radius %.1f  |  Inclination %.1f", launchSettings.orbitRadius, launchSettings.inclinationDeg),
             contentX,
             y,
             14,
             RAYWHITE);
    y += 16;
    DrawText(TextFormat("Speed %.2f  |  Angle %.1f", launchSettings.angularSpeed, launchSettings.initialAngleDeg),
             contentX,
             y,
             14,
             RAYWHITE);
    y += 16;
    DrawText(TextFormat("Layer %s  |  Target %s",
                        OrbitLayerText(ClassifyOrbitLayer(launchSettings.orbitRadius)),
                        OrbitLayerText(earthMission.targetLayer)),
             contentX,
             y,
             14,
             earthMission.deploymentComplete ? LIME : SKYBLUE);
    y += 20;

    DrawText("Selected Target", contentX, y, 15, sectionColor);
    y += 18;
    y += DrawWrappedText(SelectedObjectName(objects, selectedObjectIndex), contentX, y, contentWidth, 14, 3, selectedObjectIndex >= 0 ? YELLOW : mutedColor) + 8;

    DrawText("Risk Insight", contentX, y, 15, sectionColor);
    y += 18;
    y += DrawWrappedText("Pair: " + PairName(report, objects), contentX, y, contentWidth, 14, 3, RAYWHITE) + 7;
    DrawText(TextFormat("Distance %s km", FormatFloat(report.distance, 1).c_str()), contentX, y, 14, RAYWHITE);
    DrawRectangle(contentX + 190, y - 4, 88, 22, Fade(report.color, 0.88f));
    DrawText(RiskLevelText(report.level), contentX + 202, y, 14, BLACK);
    y += 26;

    DrawText("AI Advice", contentX, y, 15, sectionColor);
    y += 18;
    y += DrawWrappedText(report.advice, contentX, y, contentWidth, 14, 3, RAYWHITE) + 13;

    DrawText("Avoidance Preview", contentX, y, 15, sectionColor);
    y += 18;
    y += DrawWrappedText(avoidancePlan.action, contentX, y, contentWidth, 14, 3, avoidancePlan.available ? RAYWHITE : mutedColor) + 8;
    DrawText(TextFormat("Before %.1f  |  After %.1f", avoidancePlan.beforeDistance, avoidancePlan.afterDistance),
             contentX,
             y,
             14,
             avoidancePlan.available ? LIME : mutedColor);
    y += 16;
    DrawText(TextFormat("Mission: %s", MissionStepTitle(missionState.currentStep)), contentX, y, 14, sectionColor);

    std::string promptText = earthMission.playerSatDeployed ? missionState.nextActionText : earthMission.nextAction;
    Color promptColor = SKYBLUE;

    if (exportMessageTimer > 0.0f && missionState.hasSavedMissionReport)
    {
        promptText = "Mission complete: report saved to orbit_guard_report.txt.";
        promptColor = LIME;
    }
    else if (actionMessageTimer > 0.0f && !actionMessage.empty())
    {
        promptText = actionMessage;
    }

    constexpr int promptY = 552;
    constexpr int promptHeight = 68;
    DrawRectangle(contentX - 8, promptY, contentWidth + 16, promptHeight, {14, 22, 34, 218});
    DrawRectangleLines(contentX - 8, promptY, contentWidth + 16, promptHeight, Fade(SKYBLUE, 0.24f));
    DrawText("Next Action", contentX, promptY + 9, 15, sectionColor);
    DrawWrappedText(promptText, contentX, promptY + 30, contentWidth, 14, 2, promptColor);
}

void DrawLaunchHelpOverlay(const LaunchSettings &launchSettings)
{
    const int panelX = 84;
    const int panelY = 72;
    const int panelWidth = 690;
    const int panelHeight = 500;
    const int contentX = panelX + 34;
    const int contentWidth = panelWidth - 68;

    DrawRectangle(0, 0, kScreenWidth, kScreenHeight, {0, 0, 0, 118});
    DrawRectangle(panelX, panelY, panelWidth, panelHeight, {12, 20, 32, 242});
    DrawRectangleLines(panelX, panelY, panelWidth, panelHeight, Fade(SKYBLUE, 0.75f));

    int y = panelY + 30;
    DrawText("Launch Operation Guide", contentX, y, 30, RAYWHITE);
    y += 42;
    y += DrawWrappedText("Use this guide when adjusting a launch or explaining the controls.", contentX, y, contentWidth, 16, 4, Fade(RAYWHITE, 0.76f)) + 26;

    const std::string guideLines[] = {
        "1. Press TAB to choose the launch parameter.",
        "2. Press LEFT / RIGHT to adjust it.",
        "3. Hold SHIFT for larger adjustments.",
        "4. Press L to launch and lock the new satellite.",
        "5. Click any visible satellite to focus its risk.",
        "6. Press D to show or hide initial objects.",
        "7. Press A for avoidance, S to save the report.",
    };

    for (const std::string &line : guideLines)
    {
        y += DrawWrappedText(line, panelX + 42, y, contentWidth - 8, 18, 4, RAYWHITE) + 12;
    }

    DrawRectangle(contentX, y, contentWidth, 78, {24, 36, 52, 255});
    DrawText("Current launch setup", contentX + 18, y + 12, 16, Fade(RAYWHITE, 0.72f));
    DrawText(TextFormat("Selected: %s", LaunchFieldName(launchSettings.selectedField)), contentX + 18, y + 34, 15, YELLOW);
    DrawWrappedText(TextFormat("Radius %.1f | Inclination %.1f | Speed %.2f | Angle %.1f",
                               launchSettings.orbitRadius,
                               launchSettings.inclinationDeg,
                               launchSettings.angularSpeed,
                               launchSettings.initialAngleDeg),
                    contentX + 18,
                    y + 54,
                    contentWidth - 36,
                    14,
                    3,
                    RAYWHITE);

    DrawText("Press H to hide/show this guide.", contentX, panelY + panelHeight - 38, 17, SKYBLUE);
}

void DrawControls()
{
    DrawRectangle(0, 642, kScreenWidth, 78, {7, 10, 15, 235});
    DrawText("H Help   D Initial Objects   Click Target   Space Pause   R Reset   Up/Down Speed",
             28, 658, 16, Fade(RAYWHITE, 0.88f));
    DrawText("Tab Field   Left/Right Adjust   L Launch   Backspace Remove   A Avoid   S Report   Q Quit",
             28, 681, 16, Fade(RAYWHITE, 0.78f));
    DrawText("Mouse drag rotates camera, wheel zooms",
             28, 704, 13, Fade(RAYWHITE, 0.58f));
}
} // namespace OrbitGuard
