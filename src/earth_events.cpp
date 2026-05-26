#include "orbit_guard.h"

#include <algorithm>
#include <cmath>

namespace OrbitGuard
{
namespace
{
constexpr float kScanDuration = 1.6f;
constexpr float kResolvedEventHoldSeconds = 3.0f;
constexpr float kCollisionContactDistance = 14.0f;
constexpr float kThreatClearDistance = kMediumRiskDistance + 18.0f;
constexpr float kInterceptionDelay = 2.5f;
constexpr float kInterceptionSpeed = 18.0f;
constexpr float kScriptedImpactSpeed = 240.0f;
constexpr float kScriptedImpactContactSeconds = 5.0f;
constexpr float kImpactEventDelay = 5.0f;
constexpr float kImpactSpawnDistance = 72.0f;
constexpr float kNextScanDelay = 8.0f;
const Color kUnknownObjectColor = {255, 72, 236, 255};
const Color kImpactDebrisColor = {255, 96, 40, 255};

int OtherRiskIndexForPlayer(const RiskReport &report, int playerIndex)
{
    if (report.firstIndex == playerIndex)
    {
        return report.secondIndex;
    }
    if (report.secondIndex == playerIndex)
    {
        return report.firstIndex;
    }
    return -1;
}

bool IsDebrisThreat(const std::vector<OrbitObject> &objects, const UnknownScanState &scan, int index)
{
    return index >= 0 &&
           index < static_cast<int>(objects.size()) &&
           objects[index].type == ObjectType::Debris &&
           !(scan.objectActive && index == scan.objectIndex);
}

float DistanceBetweenObjects(const std::vector<OrbitObject> &objects, int firstIndex, int secondIndex)
{
    if (firstIndex < 0 ||
        secondIndex < 0 ||
        firstIndex >= static_cast<int>(objects.size()) ||
        secondIndex >= static_cast<int>(objects.size()))
    {
        return 0.0f;
    }

    return Vector3Distance(objects[firstIndex].position, objects[secondIndex].position);
}

void SyncOrbitWithPosition(OrbitObject &object)
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

float AdvanceThreatTowardPlayer(ImmediateEventState &event,
                                std::vector<OrbitObject> &objects,
                                int playerIndex,
                                int threatIndex,
                                float deltaTime,
                                bool scriptedImpact)
{
    const float previousTimer = event.timer;
    event.timer += deltaTime;

    OrbitObject &threat = objects[threatIndex];
    if (scriptedImpact && event.timer >= kScriptedImpactContactSeconds)
    {
        threat.position = objects[playerIndex].position;
        threat.angularSpeed = 0.0f;
        SyncOrbitWithPosition(threat);
        return 0.0f;
    }

    float activeDelta = 0.0f;
    if (event.timer > kInterceptionDelay)
    {
        activeDelta = previousTimer >= kInterceptionDelay ? deltaTime : event.timer - kInterceptionDelay;
    }
    if (activeDelta <= 0.0f)
    {
        return DistanceBetweenObjects(objects, playerIndex, threatIndex);
    }

    const Vector3 toPlayer = Vector3Subtract(objects[playerIndex].position, threat.position);
    const float distance = Vector3Length(toPlayer);
    if (distance <= 0.001f)
    {
        return 0.0f;
    }

    const float speed = scriptedImpact ? kScriptedImpactSpeed : kInterceptionSpeed;
    const float step = std::min(distance, speed * activeDelta);
    threat.position = Vector3Add(threat.position, Vector3Scale(toPlayer, step / distance));
    threat.angularSpeed = 0.0f;
    SyncOrbitWithPosition(threat);
    return DistanceBetweenObjects(objects, playerIndex, threatIndex);
}

std::string MakeUnknownObjectName(int objectNumber)
{
    return objectNumber < 10 ? "Unknown-0" + std::to_string(objectNumber) : "Unknown-" + std::to_string(objectNumber);
}

std::string MakeImpactDebrisName(int objectNumber)
{
    return objectNumber < 10 ? "Impact-Debris-0" + std::to_string(objectNumber) : "Impact-Debris-" + std::to_string(objectNumber);
}

float NormalizeAngleDegrees(float angleDeg)
{
    while (angleDeg < 0.0f)
    {
        angleDeg += 360.0f;
    }
    while (angleDeg >= 360.0f)
    {
        angleDeg -= 360.0f;
    }
    return angleDeg;
}

bool IsScriptedImpactThreat(const std::vector<OrbitObject> &objects, int index)
{
    return index >= 0 &&
           index < static_cast<int>(objects.size()) &&
           objects[index].name.rfind("Impact-Debris", 0) == 0;
}

int FindObjectByName(const std::vector<OrbitObject> &objects, const std::string &name)
{
    for (int i = 0; i < static_cast<int>(objects.size()); ++i)
    {
        if (objects[i].name == name)
        {
            return i;
        }
    }
    return -1;
}

OrbitObject CreateUnknownObjectNearPlayer(const OrbitObject &player, const std::string &name)
{
    OrbitObject object;
    object.name = name;
    object.type = ObjectType::Debris;
    object.orbitRadius = ClampFloat(player.orbitRadius + 48.0f, 85.0f, 330.0f);
    object.inclinationDeg = ClampFloat(player.inclinationDeg + 9.0f, -75.0f, 75.0f);
    object.angularSpeed = player.angularSpeed == 0.0f ? -0.28f : player.angularSpeed * 0.72f;
    object.initialAngleDeg = player.initialAngleDeg + 34.0f;
    object.initialAngleDeg = NormalizeAngleDegrees(object.initialAngleDeg);
    object.angleRad = object.initialAngleDeg * kDegToRad;
    object.position = CalculateOrbitPosition(object.orbitRadius, object.inclinationDeg, object.angleRad);
    object.color = kUnknownObjectColor;
    object.userControlled = false;
    return object;
}

Vector3 ImpactSpawnDirection(const OrbitObject &player)
{
    Vector3 radial = Vector3Normalize(player.position);
    if (Vector3Length(radial) <= 0.001f)
    {
        radial = {0.0f, 0.0f, 1.0f};
    }

    Vector3 tangent = Vector3Normalize({-radial.z, 0.0f, radial.x});
    if (Vector3Length(tangent) <= 0.001f)
    {
        tangent = {1.0f, 0.0f, 0.0f};
    }

    return Vector3Normalize(Vector3Add(Vector3Scale(radial, 0.82f), Vector3Scale(tangent, 0.18f)));
}

OrbitObject CreateImpactDebrisForPlayer(const OrbitObject &player, int impactNumber)
{
    OrbitObject object;
    object.name = MakeImpactDebrisName(impactNumber);
    object.type = ObjectType::Debris;
    object.angularSpeed = 0.0f;
    object.position = Vector3Add(player.position, Vector3Scale(ImpactSpawnDirection(player), kImpactSpawnDistance));
    object.color = kImpactDebrisColor;
    object.userControlled = false;
    SyncOrbitWithPosition(object);
    return object;
}

void StartOrCreateUnknownObjectEvent(ImmediateEventState &event,
                                     UnknownScanState &scan,
                                     std::vector<OrbitObject> &objects,
                                     int playerIndex)
{
    int unknownIndex = scan.objectIndex;
    if (!scan.objectActive || unknownIndex < 0 || unknownIndex >= static_cast<int>(objects.size()))
    {
        if (!scan.objectName.empty())
        {
            unknownIndex = FindObjectByName(objects, scan.objectName);
        }
        if (unknownIndex < 0)
        {
            scan.objectName = MakeUnknownObjectName(scan.nextObjectNumber);
            scan.nextObjectNumber++;
            objects.push_back(CreateUnknownObjectNearPlayer(objects[playerIndex], scan.objectName));
            unknownIndex = static_cast<int>(objects.size()) - 1;
        }
    }

    StartUnknownObjectEvent(event, scan, unknownIndex);
}

void StartScriptedImpactEvent(ImmediateEventState &event,
                              UnknownScanState &scan,
                              std::vector<OrbitObject> &objects,
                              int playerIndex)
{
    objects.push_back(CreateImpactDebrisForPlayer(objects[playerIndex], scan.nextImpactNumber));
    scan.nextImpactNumber++;
    scan.collisionEventTimer = 0.0f;

    const int targetIndex = static_cast<int>(objects.size()) - 1;
    StartCollisionWarning(event, targetIndex, objects[targetIndex].name);
    event.detail = objects[targetIndex].name + " is on a direct impact path toward PlayerSat";
    event.action = "Press A before the debris touches PlayerSat.";
}

bool MaybeStartScriptedImpactEvent(ImmediateEventState &event,
                                   UnknownScanState &scan,
                                   std::vector<OrbitObject> &objects,
                                   int playerIndex,
                                   float deltaTime)
{
    if (scan.identified)
    {
        return false;
    }

    scan.collisionEventTimer += deltaTime;
    if (scan.collisionEventTimer < kImpactEventDelay)
    {
        return false;
    }

    StartScriptedImpactEvent(event, scan, objects, playerIndex);
    return true;
}

void RemoveResolvedImpactDebris(ImmediateEventState &event, std::vector<OrbitObject> &objects, int playerIndex)
{
    const int targetIndex = event.targetIndex;
    if (targetIndex >= 0 &&
        targetIndex < static_cast<int>(objects.size()) &&
        targetIndex != playerIndex &&
        IsScriptedImpactThreat(objects, targetIndex))
    {
        objects.erase(objects.begin() + targetIndex);
    }

    event.targetIndex = -1;
}

void DestroyPlayerSatellite(ImmediateEventState &event, std::vector<OrbitObject> &objects, int playerIndex, const std::string &detail)
{
    const int impactIndex = event.targetIndex;
    const bool removeImpactDebris = impactIndex >= 0 &&
                                    impactIndex < static_cast<int>(objects.size()) &&
                                    impactIndex != playerIndex &&
                                    IsScriptedImpactThreat(objects, impactIndex);

    event.type = ImmediateEventType::CollisionWarning;
    event.phase = ImmediateEventPhase::Resolved;
    event.targetIndex = -1;
    event.timer = 0.0f;
    event.playerDestroyed = true;
    event.explosionPosition = objects[playerIndex].position;
    event.title = "PLAYER SAT DESTROYED";
    event.detail = detail;
    event.action = "Press L to deploy a replacement PlayerSat.";
    event.result = "PlayerSat destroyed by debris impact.";

    if (removeImpactDebris && impactIndex > playerIndex)
    {
        objects.erase(objects.begin() + impactIndex);
        objects.erase(objects.begin() + playerIndex);
    }
    else if (removeImpactDebris && impactIndex < playerIndex)
    {
        objects.erase(objects.begin() + playerIndex);
        objects.erase(objects.begin() + impactIndex);
    }
    else
    {
        objects.erase(objects.begin() + playerIndex);
    }
}

void RemoveScannedUnknownObject(UnknownScanState &scan, std::vector<OrbitObject> &objects)
{
    if (scan.objectIndex >= 0 && scan.objectIndex < static_cast<int>(objects.size()))
    {
        objects.erase(objects.begin() + scan.objectIndex);
    }

    scan.objectActive = false;
    scan.scanning = false;
    scan.identified = false;
    scan.objectIndex = -1;
    scan.progress = 0.0f;
    scan.cooldownTimer = 0.0f;
    scan.objectName.clear();
}

bool UpdateActiveCollisionWarning(ImmediateEventState &event,
                                  const UnknownScanState &scan,
                                  std::vector<OrbitObject> &objects,
                                  int playerIndex,
                                  float deltaTime)
{
    if (event.type != ImmediateEventType::CollisionWarning ||
        event.phase != ImmediateEventPhase::WaitingForPlayer)
    {
        return false;
    }

    if (!IsDebrisThreat(objects, scan, event.targetIndex))
    {
        event.phase = ImmediateEventPhase::Resolved;
        event.timer = 0.0f;
        event.result = "Collision warning cleared: tracked debris is no longer a threat.";
        event.action = "Continue monitoring PlayerSat.";
        return true;
    }

    objects[event.targetIndex].angularSpeed = 0.0f;
    SyncOrbitWithPosition(objects[event.targetIndex]);
    const bool scriptedImpact = IsScriptedImpactThreat(objects, event.targetIndex);

    float currentDistance = DistanceBetweenObjects(objects, playerIndex, event.targetIndex);
    if (currentDistance <= kCollisionContactDistance)
    {
        DestroyPlayerSatellite(event, objects, playerIndex, "PlayerSat made contact with debris");
        return true;
    }

    if (!scriptedImpact && currentDistance > kThreatClearDistance)
    {
        event.phase = ImmediateEventPhase::Resolved;
        event.timer = 0.0f;
        event.result = "Threat passed: debris moved away from PlayerSat.";
        event.action = "Continue monitoring PlayerSat.";
        return true;
    }

    currentDistance = AdvanceThreatTowardPlayer(event, objects, playerIndex, event.targetIndex, deltaTime, scriptedImpact);
    if (currentDistance <= kCollisionContactDistance)
    {
        DestroyPlayerSatellite(event, objects, playerIndex, "PlayerSat made contact with debris");
        return true;
    }

    event.action = "Press A before contact. Current distance " + FormatFloat(currentDistance, 1) + " km.";
    return true;
}
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
    event.playerDestroyed = false;
    event.explosionPosition = {};
    event.title = "COLLISION WARNING";
    event.detail = targetName + " is approaching PlayerSat";
    event.action = "Press A before contact to perform avoidance burn";
    event.result.clear();
}

void StartUnknownObjectEvent(ImmediateEventState &event, UnknownScanState &scan, int objectIndex)
{
    event.type = ImmediateEventType::UnknownObject;
    event.phase = ImmediateEventPhase::WaitingForPlayer;
    event.targetIndex = objectIndex;
    event.timer = 0.0f;
    event.playerDestroyed = false;
    event.explosionPosition = {};
    event.title = "UNKNOWN OBJECT DETECTED";
    const std::string targetName = scan.objectName.empty() ? "Unknown object" : scan.objectName;
    event.detail = targetName + " entered the monitoring zone";
    event.action = "Select " + targetName + " and press C to scan";
    event.result.clear();

    scan.objectActive = true;
    scan.scanning = false;
    scan.identified = false;
    scan.objectIndex = objectIndex;
    scan.progress = 0.0f;
    scan.cooldownTimer = 0.0f;
    if (scan.objectName.empty())
    {
        scan.objectName = targetName;
    }
    scan.revealedType = ObjectType::Debris;
}

bool CanStartUnknownScan(const ImmediateEventState &event, const UnknownScanState &scan, int selectedObjectIndex)
{
    const bool collisionHasPriority = event.type == ImmediateEventType::CollisionWarning &&
                                      event.phase != ImmediateEventPhase::Resolved &&
                                      event.phase != ImmediateEventPhase::Inactive;
    return !collisionHasPriority &&
           scan.objectActive &&
           !scan.identified &&
           selectedObjectIndex == scan.objectIndex;
}

void BeginUnknownScan(ImmediateEventState &event, UnknownScanState &scan)
{
    if (!scan.objectActive || scan.identified)
    {
        return;
    }

    scan.scanning = true;
    scan.progress = 0.0f;
    if (event.type != ImmediateEventType::CollisionWarning)
    {
        event.type = ImmediateEventType::UnknownObject;
        event.phase = ImmediateEventPhase::Animating;
        event.targetIndex = scan.objectIndex;
        event.title = "UNKNOWN OBJECT DETECTED";
        event.detail = (scan.objectName.empty() ? "Unknown object" : scan.objectName) + " entered the monitoring zone";
        event.action = "Scanning " + (scan.objectName.empty() ? "unknown object" : scan.objectName) + "...";
        event.result.clear();
    }
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
        scan.cooldownTimer = 0.0f;
        if (event.type == ImmediateEventType::UnknownObject)
        {
            const std::string targetName = scan.objectName.empty() ? "Unknown object" : scan.objectName;
            event.phase = ImmediateEventPhase::Resolved;
            event.result = scan.revealedType == ObjectType::Debris ? targetName + " identified as debris." : targetName + " identified as satellite.";
            event.action = "Scan complete. Continue monitoring PlayerSat.";
        }
    }
}

void UpdateEarthImmediateEvent(ImmediateEventState &event,
                               UnknownScanState &scan,
                               std::vector<OrbitObject> &objects,
                               const RiskReport &report,
                               float deltaTime)
{
    const int playerIndex = FindPlayerSatelliteIndex(objects);
    if (playerIndex < 0)
    {
        scan.collisionEventTimer = 0.0f;
        if (event.phase == ImmediateEventPhase::Resolved && event.playerDestroyed)
        {
            event.timer += deltaTime;
            if (event.timer >= kResolvedEventHoldSeconds)
            {
                ResetImmediateEvent(event);
            }
        }
        return;
    }

    if (UpdateActiveCollisionWarning(event, scan, objects, playerIndex, deltaTime))
    {
        return;
    }

    if (event.type == ImmediateEventType::CollisionWarning && event.phase == ImmediateEventPhase::Animating)
    {
        return;
    }

    if (event.phase == ImmediateEventPhase::Resolved)
    {
        event.timer += deltaTime;
        if (event.timer < kResolvedEventHoldSeconds)
        {
            return;
        }
        const bool completedCollisionWarning = event.type == ImmediateEventType::CollisionWarning && !event.playerDestroyed;
        ResetImmediateEvent(event);
        if (completedCollisionWarning && !scan.objectActive)
        {
            StartOrCreateUnknownObjectEvent(event, scan, objects, playerIndex);
            return;
        }
    }

    const int riskTargetIndex = OtherRiskIndexForPlayer(report, playerIndex);
    if (riskTargetIndex >= 0 &&
        riskTargetIndex < static_cast<int>(objects.size()) &&
        IsDebrisThreat(objects, scan, riskTargetIndex) &&
        (report.level == RiskLevel::Medium || report.level == RiskLevel::High))
    {
        StartScriptedImpactEvent(event, scan, objects, playerIndex);
        return;
    }

    if (MaybeStartScriptedImpactEvent(event, scan, objects, playerIndex, deltaTime))
    {
        return;
    }

    if (event.type == ImmediateEventType::UnknownObject &&
        (event.phase == ImmediateEventPhase::WaitingForPlayer || event.phase == ImmediateEventPhase::Animating))
    {
        return;
    }

    if (scan.identified)
    {
        scan.cooldownTimer += deltaTime;
        if (scan.cooldownTimer < kNextScanDelay)
        {
            return;
        }
        RemoveScannedUnknownObject(scan, objects);
    }

    if (!scan.objectActive || scan.objectIndex < 0 || scan.objectIndex >= static_cast<int>(objects.size()))
    {
        StartOrCreateUnknownObjectEvent(event, scan, objects, playerIndex);
        return;
    }

    if (!scan.identified)
    {
        StartUnknownObjectEvent(event, scan, scan.objectIndex);
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
        const float liveAngleRad = object.angleRad;
        object = animation.endObject;
        object.angleRad = liveAngleRad;
        object.initialAngleDeg = NormalizeAngleDegrees(liveAngleRad / kDegToRad);
        RefreshObjectPosition(object);
        RemoveResolvedImpactDebris(event, objects, animation.objectIndex);
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
