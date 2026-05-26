#include "orbit_guard.h"

#include <iostream>
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

bool Expect(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

int main()
{
    bool ok = true;
    std::vector<OrbitObject> objects = CreateDemoObjects();
    LaunchSettings settings;
    settings.orbitRadius = 165.0f;
    settings.inclinationDeg = 12.0f;
    settings.angularSpeed = 0.48f;
    settings.initialAngleDeg = 24.0f;

    int firstIndex = UpsertPlayerSatellite(objects, settings);
    ok = Expect(firstIndex >= 0, "first PlayerSat index exists") && ok;
    ok = Expect(objects[firstIndex].name == "PlayerSat", "player satellite has stable name") && ok;
    ok = Expect(objects[firstIndex].userControlled, "PlayerSat is user controlled") && ok;

    Color demoSatelliteColor = {};
    bool sawDemoSatellite = false;
    for (const OrbitObject &object : objects)
    {
        if (object.type == ObjectType::Satellite && !object.userControlled)
        {
            if (!sawDemoSatellite)
            {
                demoSatelliteColor = object.color;
                sawDemoSatellite = true;
            }
            ok = Expect(object.color.r == demoSatelliteColor.r &&
                            object.color.g == demoSatelliteColor.g &&
                            object.color.b == demoSatelliteColor.b,
                        "initial demo satellites share one marker color") &&
                 ok;
        }
    }
    ok = Expect(sawDemoSatellite, "demo satellites exist") && ok;

    const int countAfterFirst = CountPlayerSatellites(objects);
    settings.orbitRadius = 260.0f;
    int secondIndex = UpsertPlayerSatellite(objects, settings);

    ok = Expect(firstIndex == secondIndex, "upsert reuses same PlayerSat slot") && ok;
    ok = Expect(CountPlayerSatellites(objects) == countAfterFirst, "second launch does not add another PlayerSat") && ok;
    ok = Expect(objects[secondIndex].orbitRadius == 260.0f, "second launch updates PlayerSat orbit radius") && ok;
    return ok ? 0 : 1;
}
