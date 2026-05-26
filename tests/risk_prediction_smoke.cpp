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
    (void)object;
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

void UpdateObjects(std::vector<OrbitObject> &objects, float deltaTime, float timeScale)
{
    const float dt = deltaTime * timeScale;
    for (OrbitObject &object : objects)
    {
        object.position = Vector3Add(object.position, Vector3Scale(object.velocity, dt));
    }
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

OrbitObject MakeObject(const char *name, ObjectType type, bool player, Vector3 position, Vector3 velocity)
{
    OrbitObject object;
    object.name = name;
    object.type = type;
    object.userControlled = player;
    object.position = position;
    object.velocity = velocity;
    object.physicsDriven = false;
    object.collisionRadius = type == ObjectType::Satellite ? kDefaultSatelliteCollisionRadius : kDefaultDebrisCollisionRadius;
    return object;
}

int main()
{
    bool ok = true;

    std::vector<OrbitObject> objects = {
        MakeObject("PlayerSat", ObjectType::Satellite, true, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}),
        MakeObject("Impact-Debris-01", ObjectType::Debris, false, {120.0f, 0.0f, 0.0f}, {-2.0f, 0.0f, 0.0f}),
    };

    RiskReport report = AnalyzePairRisk(objects, 0, 1);
    ok = Expect(report.predicted, "pair risk uses prediction for moving objects") && ok;
    ok = Expect(report.closestApproachTime > 55.0f && report.closestApproachTime < 65.0f,
                "closest approach time is predicted from future motion") &&
         ok;
    ok = Expect(report.distance < kHighRiskDistance, "future close approach becomes high risk") && ok;
    ok = Expect(report.level == RiskLevel::High, "predicted close approach sets high risk") && ok;

    return ok ? 0 : 1;
}
