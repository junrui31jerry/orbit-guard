#include "orbit_guard.h"

#include <cmath>
#include <iostream>
#include <string>
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

namespace OrbitGuard
{
std::string FormatFloat(float value, int precision)
{
    (void)precision;
    return std::to_string(value);
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

int CountImpactDebris(const std::vector<OrbitObject> &objects)
{
    int count = 0;
    for (const OrbitObject &object : objects)
    {
        if (object.name.find("Impact-Debris") == 0)
        {
            count++;
        }
    }
    return count;
}

float RelativeSpeed(const OrbitObject &first, const OrbitObject &second)
{
    return Vector3Length(Vector3Subtract(first.velocity, second.velocity));
}

OrbitObject MakeObject(const char *name, ObjectType type, bool player, Vector3 position)
{
    OrbitObject object;
    object.name = name;
    object.type = type;
    object.userControlled = player;
    object.position = position;
    object.orbitRadius = Vector3Length(position);
    const float yzRadius = std::sqrt(position.y * position.y + position.z * position.z);
    object.angleRad = std::atan2(yzRadius, position.x);
    object.initialAngleDeg = object.angleRad / kDegToRad;
    object.inclinationDeg = std::atan2(position.y, position.z) / kDegToRad;
    object.color = player ? YELLOW : ORANGE;
    object.collisionRadius = type == ObjectType::Satellite ? kDefaultSatelliteCollisionRadius : kDefaultDebrisCollisionRadius;
    object.velocity = CalculateCircularOrbitVelocity(object.orbitRadius,
                                                     object.inclinationDeg,
                                                     object.angleRad,
                                                     object.angularSpeed == 0.0f ? kDefaultSpeedBiasControl : object.angularSpeed);
    return object;
}

int main()
{
    bool ok = true;

    std::vector<OrbitObject> riskyObjects = {
        MakeObject("PlayerSat", ObjectType::Satellite, true, {0.0f, 0.0f, 150.0f}),
        MakeObject("Debris-A17", ObjectType::Debris, false, {0.0f, 0.0f, 178.0f}),
    };
    ImmediateEventState collisionEvent;
    UnknownScanState collisionScan;
    const RiskReport riskyReport = AnalyzeRisk(riskyObjects, true, 0);

    UpdateEarthImmediateEvent(collisionEvent, collisionScan, riskyObjects, riskyReport, 0.0f);
    ok = Expect(collisionEvent.type == ImmediateEventType::CollisionWarning, "close debris starts collision warning") && ok;
    ok = Expect(collisionEvent.targetIndex >= 0 &&
                    collisionEvent.targetIndex < static_cast<int>(riskyObjects.size()) &&
                    riskyObjects[collisionEvent.targetIndex].name.find("Impact-Debris") == 0,
                "close demo debris warning targets a spawned impact debris object") &&
         ok;
    ok = Expect(riskyObjects.size() == 3, "close demo debris warning leaves the original objects in place") && ok;
    ok = Expect(riskyObjects[1].name == "Debris-A17", "initial debris is not reused as the impact body") && ok;
    ok = Expect(collisionEvent.detail.find("Impact-Debris") != std::string::npos, "collision message names spawned debris") && ok;

    std::vector<OrbitObject> satelliteOnlyRisk = {
        MakeObject("PlayerSat", ObjectType::Satellite, true, {0.0f, 0.0f, 150.0f}),
        MakeObject("Relay-9", ObjectType::Satellite, false, {0.0f, 0.0f, 178.0f}),
    };
    ImmediateEventState satelliteEvent;
    UnknownScanState satelliteScan;
    RiskReport satelliteReport = AnalyzeRisk(satelliteOnlyRisk, true, 0);

    UpdateEarthImmediateEvent(satelliteEvent, satelliteScan, satelliteOnlyRisk, satelliteReport, 0.0f);
    ok = Expect(satelliteEvent.type != ImmediateEventType::CollisionWarning, "close satellite does not start collision warning") && ok;

    std::vector<OrbitObject> timeoutObjects = {
        MakeObject("PlayerSat", ObjectType::Satellite, true, {0.0f, 0.0f, 150.0f}),
        MakeObject("Debris-A17", ObjectType::Debris, false, {0.0f, 0.0f, 178.0f}),
    };
    ImmediateEventState timeoutEvent;
    UnknownScanState timeoutScan;
    RiskReport timeoutReport = AnalyzeRisk(timeoutObjects, true, 0);

    UpdateEarthImmediateEvent(timeoutEvent, timeoutScan, timeoutObjects, timeoutReport, 0.0f);
    UpdateEarthImmediateEvent(timeoutEvent, timeoutScan, timeoutObjects, timeoutReport, 6.0f);
    ok = Expect(FindPlayerSatelliteIndex(timeoutObjects) >= 0, "warning updates do not teleport debris into PlayerSat") && ok;
    ok = Expect(!timeoutEvent.playerDestroyed, "unanswered warning waits for physics motion before destruction") && ok;

    std::vector<OrbitObject> contactObjects = {
        MakeObject("PlayerSat", ObjectType::Satellite, true, {0.0f, 0.0f, 150.0f}),
        MakeObject("Debris-A17", ObjectType::Debris, false, {0.0f, 0.0f, 158.0f}),
    };
    ImmediateEventState contactEvent;
    UnknownScanState contactScan;
    RiskReport contactReport = AnalyzeRisk(contactObjects, true, 0);

    UpdateEarthImmediateEvent(contactEvent, contactScan, contactObjects, contactReport, 0.0f);
    UpdateEarthImmediateEvent(contactEvent, contactScan, contactObjects, contactReport, 0.1f);
    ok = Expect(FindPlayerSatelliteIndex(contactObjects) >= 0, "initial debris contact does not directly destroy PlayerSat") && ok;
    ok = Expect(contactEvent.type == ImmediateEventType::CollisionWarning &&
                    contactEvent.targetIndex >= 0 &&
                    contactEvent.targetIndex < static_cast<int>(contactObjects.size()) &&
                    contactObjects[contactEvent.targetIndex].name.find("Impact-Debris") == 0,
                "initial debris contact still creates a spawned impact warning") &&
         ok;

    collisionEvent.phase = ImmediateEventPhase::Resolved;
    collisionEvent.timer = 3.1f;
    UpdateEarthImmediateEvent(collisionEvent, collisionScan, riskyObjects, riskyReport, 0.0f);
    ok = Expect(collisionEvent.type == ImmediateEventType::UnknownObject, "resolved first collision gives unknown event a turn") && ok;
    ok = Expect(collisionScan.objectActive, "unknown event appears after resolved collision") && ok;

    std::vector<OrbitObject> quietObjects = {
        MakeObject("PlayerSat", ObjectType::Satellite, true, {0.0f, 0.0f, 260.0f}),
    };
    ImmediateEventState unknownEvent;
    UnknownScanState unknownScan;
    RiskReport quietReport = AnalyzeRisk(quietObjects, true, 0);

    UpdateEarthImmediateEvent(unknownEvent, unknownScan, quietObjects, quietReport, 0.0f);
    ok = Expect(unknownEvent.type == ImmediateEventType::UnknownObject, "quiet monitoring creates unknown event") && ok;
    ok = Expect(unknownScan.objectActive, "unknown scan state becomes active") && ok;
    ok = Expect(unknownScan.objectIndex >= 0 && quietObjects[unknownScan.objectIndex].name == "Unknown-01", "unknown object is inserted") && ok;
    ok = Expect(quietObjects[unknownScan.objectIndex].color.r > 200 && quietObjects[unknownScan.objectIndex].color.b > 200, "unknown object uses high contrast magenta marker") && ok;

    std::vector<OrbitObject> scriptedImpactObjects = {
        MakeObject("PlayerSat", ObjectType::Satellite, true, {0.0f, 0.0f, 260.0f}),
    };
    ImmediateEventState scriptedImpactEvent;
    UnknownScanState scriptedImpactScan;

    UpdateEarthImmediateEvent(scriptedImpactEvent, scriptedImpactScan, scriptedImpactObjects, AnalyzeRisk(scriptedImpactObjects, true, 0), 0.0f);
    UpdateEarthImmediateEvent(scriptedImpactEvent, scriptedImpactScan, scriptedImpactObjects, AnalyzeRisk(scriptedImpactObjects, true, 0), 5.2f);
    ok = Expect(scriptedImpactEvent.type == ImmediateEventType::CollisionWarning, "quiet monitoring eventually creates an impact warning") && ok;
    ok = Expect(scriptedImpactEvent.targetIndex >= 0 &&
                    scriptedImpactEvent.targetIndex < static_cast<int>(scriptedImpactObjects.size()) &&
                    scriptedImpactObjects[scriptedImpactEvent.targetIndex].name.find("Impact-Debris") == 0,
                "impact warning targets a spawned debris object") &&
         ok;
    const OrbitObject &scriptedImpactDebris = scriptedImpactObjects[scriptedImpactEvent.targetIndex];
    ok = Expect(scriptedImpactDebris.physicsDriven, "spawned impact debris is physics driven") && ok;
    ok = Expect(Vector3Length(scriptedImpactDebris.velocity) > 0.1f, "spawned impact debris has an initial velocity") && ok;
    ok = Expect(RelativeSpeed(scriptedImpactDebris, scriptedImpactObjects[0]) < 5.5f,
                "spawned impact debris relative speed stays playable") &&
         ok;
    const int scriptedThreatIndex = scriptedImpactEvent.targetIndex;
    const float scriptedStartDistance = Vector3Distance(scriptedImpactObjects[0].position, scriptedImpactDebris.position);

    float scriptedClosestDistance = scriptedStartDistance;
    for (int i = 0; i < 24 && FindPlayerSatelliteIndex(scriptedImpactObjects) >= 0; ++i)
    {
        UpdateObjects(scriptedImpactObjects, 1.0f, 1.0f);
        UpdateEarthImmediateEvent(scriptedImpactEvent, scriptedImpactScan, scriptedImpactObjects, AnalyzeRisk(scriptedImpactObjects, true, 0), 1.0f);
        if (scriptedThreatIndex >= 0 && scriptedThreatIndex < static_cast<int>(scriptedImpactObjects.size()))
        {
            scriptedClosestDistance = std::min(scriptedClosestDistance,
                                               Vector3Distance(scriptedImpactObjects[0].position, scriptedImpactObjects[scriptedThreatIndex].position));
        }
    }
    ok = Expect(FindPlayerSatelliteIndex(scriptedImpactObjects) >= 0, "spawned impact debris does not destroy PlayerSat too quickly") && ok;
    ok = Expect(!scriptedImpactEvent.playerDestroyed, "spawned impact warning does not use forced contact") && ok;
    if (scriptedThreatIndex >= 0 && scriptedThreatIndex < static_cast<int>(scriptedImpactObjects.size()))
    {
        ok = Expect(scriptedClosestDistance < scriptedStartDistance, "spawned impact debris closes distance through physics") && ok;
    }

    std::vector<OrbitObject> impactContactObjects = {
        MakeObject("PlayerSat", ObjectType::Satellite, true, {0.0f, 0.0f, 260.0f}),
        MakeObject("Impact-Debris-77", ObjectType::Debris, false, {0.0f, 0.0f, 268.0f}),
    };
    ImmediateEventState impactContactEvent;
    UnknownScanState impactContactScan;
    StartCollisionWarning(impactContactEvent, 1, impactContactObjects[1].name);
    UpdateEarthImmediateEvent(impactContactEvent, impactContactScan, impactContactObjects, AnalyzeRisk(impactContactObjects, true, 0), 0.1f);
    ok = Expect(FindPlayerSatelliteIndex(impactContactObjects) < 0, "physical impact contact destroys PlayerSat") && ok;
    ok = Expect(impactContactEvent.playerDestroyed, "physical impact contact marks PlayerSat destroyed") && ok;
    ok = Expect(CountImpactDebris(impactContactObjects) == 0, "impact debris is removed after destroying PlayerSat") && ok;

    std::vector<OrbitObject> movingImpactObjects = {
        MakeObject("PlayerSat", ObjectType::Satellite, true, {0.0f, 0.0f, 260.0f}),
    };
    movingImpactObjects[0].angularSpeed = 0.48f;
    ImmediateEventState movingImpactEvent;
    UnknownScanState movingImpactScan;

    UpdateEarthImmediateEvent(movingImpactEvent, movingImpactScan, movingImpactObjects, AnalyzeRisk(movingImpactObjects, true, 0), 0.0f);
    UpdateEarthImmediateEvent(movingImpactEvent, movingImpactScan, movingImpactObjects, AnalyzeRisk(movingImpactObjects, true, 0), 5.2f);
    ok = Expect(movingImpactEvent.type == ImmediateEventType::CollisionWarning, "moving PlayerSat receives a scripted impact warning") && ok;
    const int movingThreatIndex = movingImpactEvent.targetIndex;
    const float movingStartDistance = movingThreatIndex >= 0
                                          ? Vector3Distance(movingImpactObjects[0].position, movingImpactObjects[movingThreatIndex].position)
                                          : 0.0f;

    for (int i = 0; i < 24 && FindPlayerSatelliteIndex(movingImpactObjects) >= 0; ++i)
    {
        UpdateObjects(movingImpactObjects, 0.5f, 1.0f);
        UpdateEarthImmediateEvent(movingImpactEvent, movingImpactScan, movingImpactObjects, AnalyzeRisk(movingImpactObjects, true, 0), 0.5f);
    }
    ok = Expect(FindPlayerSatelliteIndex(movingImpactObjects) >= 0, "moving PlayerSat is not destroyed by an over-fast scripted impact") && ok;
    if (movingThreatIndex >= 0 && movingThreatIndex < static_cast<int>(movingImpactObjects.size()))
    {
        const float movingEndDistance = Vector3Distance(movingImpactObjects[0].position, movingImpactObjects[movingThreatIndex].position);
        ok = Expect(movingEndDistance < movingStartDistance, "moving impact debris closes distance through physics") && ok;
    }

    std::vector<OrbitObject> avoidanceObjects = {
        MakeObject("PlayerSat", ObjectType::Satellite, true, {0.0f, 0.0f, 260.0f}),
        MakeObject("Impact-Debris-99", ObjectType::Debris, false, {0.0f, 0.0f, 320.0f}),
    };
    avoidanceObjects[0].angleRad = 0.25f;
    avoidanceObjects[0].initialAngleDeg = avoidanceObjects[0].angleRad / kDegToRad;
    avoidanceObjects[0].angularSpeed = 0.48f;
    avoidanceObjects[1].angularSpeed = 0.0f;

    ImmediateEventState avoidanceEvent;
    StartCollisionWarning(avoidanceEvent, 1, avoidanceObjects[1].name);

    AvoidancePlan manualPlan;
    manualPlan.available = true;
    manualPlan.objectIndex = 0;
    manualPlan.beforeDistance = 60.0f;
    manualPlan.afterDistance = 120.0f;
    manualPlan.proposedObject = avoidanceObjects[0];
    manualPlan.proposedObject.orbitRadius = 300.0f;
    manualPlan.proposedObject.inclinationDeg = 16.0f;
    manualPlan.proposedObject.angularSpeed = 0.36f;

    AvoidanceAnimationState avoidanceAnimation;
    ok = Expect(BeginAvoidanceAnimation(avoidanceAnimation, avoidanceEvent, avoidanceObjects, manualPlan),
                "manual avoidance animation can start") &&
         ok;
    avoidanceObjects[0].angleRad = 1.25f;
    UpdateAvoidanceAnimation(avoidanceAnimation, avoidanceEvent, avoidanceObjects, 2.0f);
    ok = Expect(avoidanceObjects.size() == 1, "successful avoidance removes the spawned impact debris") && ok;
    ok = Expect(std::fabs(avoidanceObjects[0].angleRad - 1.25f) < 0.001f,
                "successful avoidance keeps the live orbital angle instead of snapping backward") &&
         ok;
    ok = Expect(avoidanceEvent.targetIndex == -1, "successful avoidance clears the resolved impact target") && ok;

    BeginUnknownScan(unknownEvent, unknownScan);
    ok = Expect(unknownScan.scanning, "unknown scan can start") && ok;

    quietObjects.push_back(MakeObject("Debris-A17", ObjectType::Debris, false, {0.0f, 0.0f, 288.0f}));
    RiskReport scanBlockedCollisionReport = AnalyzeRisk(quietObjects, true, 0);
    UpdateEarthImmediateEvent(unknownEvent, unknownScan, quietObjects, scanBlockedCollisionReport, 0.1f);
    ok = Expect(unknownEvent.type == ImmediateEventType::CollisionWarning, "collision warning can interrupt an active scan") && ok;
    ok = Expect(unknownScan.scanning, "scan keeps progressing state while collision warning is active") && ok;

    ImmediateEventState repeatScanEvent;
    UnknownScanState repeatScan;
    std::vector<OrbitObject> repeatObjects = {
        MakeObject("PlayerSat", ObjectType::Satellite, true, {0.0f, 0.0f, 260.0f}),
    };
    RiskReport repeatReport = AnalyzeRisk(repeatObjects, true, 0);

    UpdateEarthImmediateEvent(repeatScanEvent, repeatScan, repeatObjects, repeatReport, 0.0f);
    ok = Expect(repeatScan.objectActive, "first repeat scan target appears") && ok;
    const int firstUnknownIndex = repeatScan.objectIndex;
    BeginUnknownScan(repeatScanEvent, repeatScan);
    UpdateUnknownScan(repeatScanEvent, repeatScan, 2.0f);
    UpdateEarthImmediateEvent(repeatScanEvent, repeatScan, repeatObjects, AnalyzeRisk(repeatObjects, true, 0), 9.0f);
    UpdateEarthImmediateEvent(repeatScanEvent, repeatScan, repeatObjects, AnalyzeRisk(repeatObjects, true, 0), 0.0f);
    ok = Expect(repeatScan.objectActive, "another scan target appears after cooldown") && ok;
    ok = Expect(repeatScan.objectIndex >= 0 && repeatScan.objectIndex < static_cast<int>(repeatObjects.size()), "new scan target index is valid") && ok;
    ok = Expect(repeatScan.objectIndex != firstUnknownIndex || repeatObjects[repeatScan.objectIndex].name != "Unknown-01", "repeat scan target is refreshed") && ok;

    return ok ? 0 : 1;
}
