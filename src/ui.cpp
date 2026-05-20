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
    if (report.firstIndex < 0 || report.secondIndex < 0)
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

void DrawSpaceObject(const OrbitObject &object, bool highlighted)
{
    const float size = object.type == ObjectType::Satellite ? 8.0f : 5.5f;

    if (object.type == ObjectType::Satellite)
    {
        DrawCubeV(object.position, {size * 1.4f, size * 0.75f, size * 0.75f}, object.color);
        DrawCubeWiresV(object.position, {size * 1.4f, size * 0.75f, size * 0.75f}, WHITE);
    }
    else
    {
        DrawSphere(object.position, size, object.color);
        DrawSphereWires(object.position, size, 8, 8, Fade(WHITE, 0.55f));
    }

    if (highlighted)
    {
        DrawSphereWires(object.position, size + 6.0f, 16, 12, WHITE);
    }
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
    DrawRectangle(20, 18, 430, 64, {4, 8, 14, 190});
    DrawText(title, 38, 30, 26, RAYWHITE);
    DrawText(subtitle, 40, 60, 15, Fade(RAYWHITE, 0.68f));
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
                   bool showDemoObjects,
                   int selectedObjectIndex,
                   float actionMessageTimer,
                   const std::string &actionMessage)
{
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

    std::string promptText = missionState.nextActionText;
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
