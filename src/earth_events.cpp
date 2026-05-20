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
