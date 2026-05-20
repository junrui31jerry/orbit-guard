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
} // namespace OrbitGuard
