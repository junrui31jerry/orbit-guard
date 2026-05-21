#include "orbit_guard.h"

#include <iostream>
#include <vector>

using namespace OrbitGuard;

bool Expect(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

OrbitObject MakeObject(const char *name, ObjectType type, bool player, Vector3 position)
{
    OrbitObject object;
    object.name = name;
    object.type = type;
    object.userControlled = player;
    object.position = position;
    object.orbitRadius = Vector3Length(position);
    object.color = player ? YELLOW : ORANGE;
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
    ok = Expect(collisionEvent.targetIndex == 1, "collision warning targets debris") && ok;
    ok = Expect(collisionEvent.detail.find("Debris-A17") != std::string::npos, "collision message names debris") && ok;

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
    ok = Expect(quietObjects[unknownScan.objectIndex].color.b > quietObjects[unknownScan.objectIndex].color.r, "unknown object uses cool color") && ok;

    return ok ? 0 : 1;
}
