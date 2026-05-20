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

    const int countAfterFirst = CountPlayerSatellites(objects);
    settings.orbitRadius = 260.0f;
    int secondIndex = UpsertPlayerSatellite(objects, settings);

    ok = Expect(firstIndex == secondIndex, "upsert reuses same PlayerSat slot") && ok;
    ok = Expect(CountPlayerSatellites(objects) == countAfterFirst, "second launch does not add another PlayerSat") && ok;
    ok = Expect(objects[secondIndex].orbitRadius == 260.0f, "second launch updates PlayerSat orbit radius") && ok;
    return ok ? 0 : 1;
}
