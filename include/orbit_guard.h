#ifndef ORBIT_GUARD_H
#define ORBIT_GUARD_H

#include "raylib.h"
#include "raymath.h"

#include <string>
#include <vector>

#ifndef MOUSE_BUTTON_LEFT
#define MOUSE_BUTTON_LEFT MOUSE_LEFT_BUTTON
#endif

namespace OrbitGuard
{
inline constexpr int kScreenWidth = 1280;
inline constexpr int kScreenHeight = 720;
inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kDegToRad = kPi / 180.0f;
inline constexpr float kEarthRadius = 46.0f;
inline constexpr float kHighRiskDistance = 35.0f;
inline constexpr float kMediumRiskDistance = 85.0f;

enum class ObjectType
{
    Satellite,
    Debris
};

enum class RiskLevel
{
    Low,
    Medium,
    High
};

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

struct OrbitObject
{
    std::string name;
    ObjectType type = ObjectType::Satellite;
    float orbitRadius = 0.0f;
    float inclinationDeg = 0.0f;
    float angularSpeed = 0.0f;
    float initialAngleDeg = 0.0f;
    float angleRad = 0.0f;
    Vector3 position = {};
    Color color = WHITE;
    bool userControlled = false;
};

struct RiskReport
{
    int firstIndex = -1;
    int secondIndex = -1;
    float distance = 0.0f;
    RiskLevel level = RiskLevel::Low;
    std::string advice;
    Color color = GREEN;
};

struct OrbitCameraState
{
    float yaw = 43.0f * kDegToRad;
    float pitch = 30.0f * kDegToRad;
    float distance = 430.0f;
};

struct LaunchSettings
{
    int selectedField = 0;
    int launchCount = 0;
    float orbitRadius = 165.0f;
    float inclinationDeg = 12.0f;
    float angularSpeed = 0.48f;
    float initialAngleDeg = 24.0f;
};

struct AvoidancePlan
{
    bool available = false;
    int objectIndex = -1;
    float beforeDistance = 0.0f;
    float afterDistance = 0.0f;
    RiskLevel afterLevel = RiskLevel::Low;
    std::string action = "Launch a user satellite, then press A when it is part of the closest risk pair.";
    OrbitObject proposedObject = {};
};

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

float ClampFloat(float value, float minimum, float maximum);
Vector3 CalculateOrbitPosition(float radius, float inclinationDeg, float angleRad);
OrbitLayer ClassifyOrbitLayer(float orbitRadius);
const char *OrbitLayerText(OrbitLayer layer);
bool IsOrbitLayerMatch(OrbitLayer layer, float orbitRadius);
Color OrbitLayerColor(OrbitLayer layer);
const char *LaunchFieldName(int selectedField);
void AdjustLaunchSettings(LaunchSettings &settings, float direction, bool largeStep);
OrbitObject CreateUserSatellite(LaunchSettings &settings);
OrbitObject CreatePlayerSatellite(const LaunchSettings &settings);
int FindPlayerSatelliteIndex(const std::vector<OrbitObject> &objects);
int CountPlayerSatellites(const std::vector<OrbitObject> &objects);
int UpsertPlayerSatellite(std::vector<OrbitObject> &objects, const LaunchSettings &settings);
std::vector<OrbitObject> CreateDemoObjects();
void ResetObjects(std::vector<OrbitObject> &objects);
void UpdateObjects(std::vector<OrbitObject> &objects, float deltaTime, float timeScale);
void RefreshObjectPosition(OrbitObject &object);
void UpdateOrbitCamera(OrbitCameraState &state, Camera3D &camera);

const char *RiskLevelText(RiskLevel level);
RiskReport AnalyzeRisk(const std::vector<OrbitObject> &objects);
RiskReport AnalyzeRisk(const std::vector<OrbitObject> &objects, bool showDemoObjects, int selectedObjectIndex);
int FindControlledRiskObject(const RiskReport &report, const std::vector<OrbitObject> &objects);
AvoidancePlan BuildAvoidancePlan(const RiskReport &report, const std::vector<OrbitObject> &objects);
AvoidancePlan BuildAvoidancePlan(const RiskReport &report,
                                 const std::vector<OrbitObject> &objects,
                                 bool showDemoObjects,
                                 int selectedObjectIndex);
void ApplyAvoidancePlan(std::vector<OrbitObject> &objects, const AvoidancePlan &plan);
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
void UpdateEarthMissionBeforeLaunch(EarthMissionState &mission, const LaunchSettings &settings);
void UpdateEarthMissionAfterLaunch(EarthMissionState &mission, const std::vector<OrbitObject> &objects);
void ResetImmediateEvent(ImmediateEventState &event);
void StartCollisionWarning(ImmediateEventState &event, int targetIndex, const std::string &targetName);
void StartUnknownObjectEvent(ImmediateEventState &event, UnknownScanState &scan, int objectIndex);
bool CanStartUnknownScan(const ImmediateEventState &event, const UnknownScanState &scan, int selectedObjectIndex);
void BeginUnknownScan(ImmediateEventState &event, UnknownScanState &scan);
void UpdateUnknownScan(ImmediateEventState &event, UnknownScanState &scan, float deltaTime);
bool BeginAvoidanceAnimation(AvoidanceAnimationState &animation,
                             ImmediateEventState &event,
                             const std::vector<OrbitObject> &objects,
                             const AvoidancePlan &plan);
void UpdateAvoidanceAnimation(AvoidanceAnimationState &animation,
                              ImmediateEventState &event,
                              std::vector<OrbitObject> &objects,
                              float deltaTime);

std::string FormatFloat(float value, int precision = 1);
std::string PairName(const RiskReport &report, const std::vector<OrbitObject> &objects);
bool ExportRiskReport(const RiskReport &report,
                      const std::vector<OrbitObject> &objects,
                      float simulationTime,
                      float timeScale,
                      const LaunchSettings &launchSettings,
                      const AvoidancePlan &avoidancePlan);

void DrawStarField();
void DrawOrbitPath(float radius, float inclinationDeg, Color color);
void DrawSpaceObject(const OrbitObject &object, bool highlighted);
void DrawMainMenu(int selectedMenuItem);
void DrawModeTitle(const char *title, const char *subtitle);
int DrawWrappedText(const std::string &text, int x, int y, int maxWidth, int fontSize, int spacing, Color color);
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
                   const std::string &actionMessage);
void DrawLaunchHelpOverlay(const LaunchSettings &launchSettings);
void DrawControls();
} // namespace OrbitGuard

#endif
