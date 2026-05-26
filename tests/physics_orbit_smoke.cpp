#include "orbit_guard.h"

#include <cmath>
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

float AngleDelta(float a, float b)
{
    float delta = std::fabs(a - b);
    while (delta > 2.0f * kPi)
    {
        delta -= 2.0f * kPi;
    }
    if (delta > kPi)
    {
        delta = 2.0f * kPi - delta;
    }
    return delta;
}

int main()
{
    bool ok = true;

    LaunchSettings settings;
    settings.orbitRadius = kReferenceOrbitRadius;
    settings.inclinationDeg = 0.0f;
    settings.angularSpeed = kDefaultSpeedBiasControl;
    settings.initialAngleDeg = 0.0f;

    OrbitObject player = CreatePlayerSatellite(settings);
    ok = Expect(player.physicsDriven, "PlayerSat uses physics-driven motion") && ok;
    ok = Expect(Vector3Length(player.velocity) > 0.1f, "PlayerSat launch creates nonzero velocity") && ok;

    const Vector3 radial = Vector3Normalize(player.position);
    const Vector3 velocityDirection = Vector3Normalize(player.velocity);
    ok = Expect(std::fabs(Vector3DotProduct(radial, velocityDirection)) < 0.02f,
                "PlayerSat velocity is tangent to the launch radius") &&
         ok;

    std::vector<OrbitObject> objects = {player};
    const float startRadius = Vector3Length(objects[0].position);
    const float startAngle = objects[0].angleRad;

    UpdateObjects(objects, 30.0f, 1.0f);

    const float endRadius = Vector3Length(objects[0].position);
    const float halfOrbitDelta = AngleDelta(objects[0].angleRad, startAngle);
    ok = Expect(std::fabs(halfOrbitDelta - kPi) < 0.28f,
                "default MEO PlayerSat completes about a half orbit in 30 seconds") &&
         ok;
    ok = Expect(std::fabs(endRadius - startRadius) < startRadius * 0.08f,
                "physics orbit keeps radius stable over a half orbit") &&
         ok;

    return ok ? 0 : 1;
}
