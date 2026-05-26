#include "orbit_guard.h"

#include <iostream>
#include <vector>

using namespace OrbitGuard;

namespace OrbitGuard
{
float ClampFloat(float value, float minimum, float maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

void RefreshObjectPosition(OrbitObject &object)
{
    object.position = {object.orbitRadius, 0.0f, 0.0f};
}

void SyncOrbitFieldsFromPosition(OrbitObject &object)
{
    object.orbitRadius = Vector3Length(object.position);
}

int FindPlayerSatelliteIndex(const std::vector<OrbitObject> &objects)
{
    for (int i = 0; i < static_cast<int>(objects.size()); ++i)
    {
        if (objects[i].userControlled && objects[i].name == "PlayerSat")
        {
            return i;
        }
    }

    return -1;
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

OrbitObject MakeObject(const char *name, bool userControlled, Vector3 position)
{
    OrbitObject object;
    object.name = name;
    object.userControlled = userControlled;
    object.position = position;
    object.orbitRadius = position.x;
    object.physicsDriven = false;
    return object;
}

OrbitObject MakeTypedObject(const char *name, ObjectType type, bool userControlled, Vector3 position)
{
    OrbitObject object = MakeObject(name, userControlled, position);
    object.type = type;
    return object;
}

int main()
{
    bool ok = true;

    std::vector<OrbitObject> objects = {
        MakeObject("Target", true, {0.0f, 0.0f, 0.0f}),
        MakeObject("Unrelated-A", true, {200.0f, 0.0f, 0.0f}),
        MakeObject("Unrelated-B", true, {201.0f, 0.0f, 0.0f}),
        MakeObject("Near-Target", true, {42.0f, 0.0f, 0.0f}),
    };

    RiskReport targetReport = AnalyzeRisk(objects, true, 0);
    ok = Expect(targetReport.firstIndex == 0 || targetReport.secondIndex == 0,
                "selected target is part of the reported risk pair") &&
         ok;
    ok = Expect(targetReport.distance > 40.0f && targetReport.distance < 44.0f,
                "selected target ignores closer unrelated pairs") &&
         ok;

    std::vector<OrbitObject> mixedObjects = {
        MakeObject("Demo-A", false, {0.0f, 0.0f, 0.0f}),
        MakeObject("Demo-B", false, {1.0f, 0.0f, 0.0f}),
        MakeObject("User-A", true, {100.0f, 0.0f, 0.0f}),
        MakeObject("User-B", true, {170.0f, 0.0f, 0.0f}),
    };

    RiskReport visibleDemoReport = AnalyzeRisk(mixedObjects, true, -1);
    ok = Expect(visibleDemoReport.firstIndex == 0 && visibleDemoReport.secondIndex == 1,
                "shown demo objects participate in global risk") &&
         ok;

    RiskReport hiddenDemoReport = AnalyzeRisk(mixedObjects, false, -1);
    ok = Expect(hiddenDemoReport.firstIndex == 2 && hiddenDemoReport.secondIndex == 3,
                "hidden demo objects are excluded from risk") &&
         ok;

    std::vector<OrbitObject> satelliteThreatObjects = {
        MakeTypedObject("PlayerSat", ObjectType::Satellite, true, {0.0f, 0.0f, 0.0f}),
        MakeTypedObject("Relay-9", ObjectType::Satellite, false, {24.0f, 0.0f, 0.0f}),
    };
    RiskReport satelliteThreatReport = AnalyzeRisk(satelliteThreatObjects, true, 0);
    AvoidancePlan satelliteThreatPlan = BuildAvoidancePlan(satelliteThreatReport, satelliteThreatObjects, true, 0);
    ok = Expect(!satelliteThreatPlan.available, "satellite-only threats do not produce A avoidance plans") && ok;

    return ok ? 0 : 1;
}
