#include "orbit_guard.h"

#include <algorithm>
#include <cmath>

namespace OrbitGuard
{
float ClampFloat(float value, float minimum, float maximum)
{
    return std::max(minimum, std::min(value, maximum));
}

Vector3 CalculateOrbitPosition(float radius, float inclinationDeg, float angleRad)
{
    const float inclination = inclinationDeg * kDegToRad;
    const float x = std::cos(angleRad) * radius;
    const float orbitZ = std::sin(angleRad) * radius;
    const float y = orbitZ * std::sin(inclination);
    const float z = orbitZ * std::cos(inclination);
    return {x, y, z};
}

float SpeedBiasFromControl(float speedControl)
{
    const float bias = std::fabs(speedControl) / kDefaultSpeedBiasControl;
    return ClampFloat(bias, 0.65f, 1.35f);
}

Vector3 CalculateOrbitTangent(float inclinationDeg, float angleRad)
{
    const float inclination = inclinationDeg * kDegToRad;
    Vector3 tangent = {
        -std::sin(angleRad),
        std::cos(angleRad) * std::sin(inclination),
        std::cos(angleRad) * std::cos(inclination)};
    return Vector3Normalize(tangent);
}

Vector3 CalculateCircularOrbitVelocity(float radius, float inclinationDeg, float angleRad, float speedControl)
{
    const float safeRadius = std::max(radius, kEarthRadius + 1.0f);
    const float direction = speedControl < 0.0f ? -1.0f : 1.0f;
    const float speed = std::sqrt(kEarthMu / safeRadius) * SpeedBiasFromControl(speedControl);
    return Vector3Scale(CalculateOrbitTangent(inclinationDeg, angleRad), speed * direction);
}

void SyncOrbitFieldsFromPosition(OrbitObject &object)
{
    const float radius = Vector3Length(object.position);
    if (radius <= 0.001f)
    {
        return;
    }

    const float inclination = object.inclinationDeg * kDegToRad;
    const float signedOrbitZ = object.position.y * std::sin(inclination) + object.position.z * std::cos(inclination);
    object.orbitRadius = radius;
    object.angleRad = std::atan2(signedOrbitZ, object.position.x);
    while (object.angleRad < 0.0f)
    {
        object.angleRad += 2.0f * kPi;
    }
    while (object.angleRad >= 2.0f * kPi)
    {
        object.angleRad -= 2.0f * kPi;
    }
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

void InitializeOrbitPhysics(OrbitObject &object)
{
    object.angleRad = object.initialAngleDeg * kDegToRad;
    object.position = CalculateOrbitPosition(object.orbitRadius, object.inclinationDeg, object.angleRad);
    object.velocity = CalculateCircularOrbitVelocity(object.orbitRadius, object.inclinationDeg, object.angleRad, object.angularSpeed);
    object.collisionRadius = object.type == ObjectType::Satellite ? kDefaultSatelliteCollisionRadius : kDefaultDebrisCollisionRadius;
    object.physicsDriven = true;
}

OrbitLayer ClassifyOrbitLayer(float orbitRadius)
{
    if (orbitRadius < 85.0f)
    {
        return OrbitLayer::BelowOperational;
    }
    if (orbitRadius <= 145.0f)
    {
        return OrbitLayer::LEO;
    }
    if (orbitRadius <= 230.0f)
    {
        return OrbitLayer::MEO;
    }
    if (orbitRadius <= 330.0f)
    {
        return OrbitLayer::GEO;
    }
    return OrbitLayer::BeyondOperational;
}

const char *OrbitLayerText(OrbitLayer layer)
{
    switch (layer)
    {
    case OrbitLayer::LEO:
        return "LEO";
    case OrbitLayer::MEO:
        return "MEO";
    case OrbitLayer::GEO:
        return "GEO";
    case OrbitLayer::BelowOperational:
        return "Below operational orbit";
    case OrbitLayer::BeyondOperational:
        return "Beyond operational orbit";
    default:
        return "Unknown orbit";
    }
}

bool IsOrbitLayerMatch(OrbitLayer layer, float orbitRadius)
{
    return ClassifyOrbitLayer(orbitRadius) == layer;
}

Color OrbitLayerColor(OrbitLayer layer)
{
    switch (layer)
    {
    case OrbitLayer::LEO:
        return SKYBLUE;
    case OrbitLayer::MEO:
        return GOLD;
    case OrbitLayer::GEO:
        return VIOLET;
    default:
        return Fade(RAYWHITE, 0.45f);
    }
}

const char *LaunchFieldName(int selectedField)
{
    switch (selectedField)
    {
    case 0:
        return "Orbit Radius";
    case 1:
        return "Inclination";
    case 2:
        return "Angular Speed";
    case 3:
        return "Initial Angle";
    default:
        return "Orbit Radius";
    }
}

void AdjustLaunchSettings(LaunchSettings &settings, float direction, bool largeStep)
{
    switch (settings.selectedField)
    {
    case 0:
        settings.orbitRadius = ClampFloat(settings.orbitRadius + direction * (largeStep ? 20.0f : 5.0f), 85.0f, 330.0f);
        break;
    case 1:
        settings.inclinationDeg = ClampFloat(settings.inclinationDeg + direction * (largeStep ? 10.0f : 2.5f), -75.0f, 75.0f);
        break;
    case 2:
        settings.angularSpeed = ClampFloat(settings.angularSpeed + direction * (largeStep ? 0.10f : 0.02f), -1.20f, 1.20f);
        break;
    case 3:
        settings.initialAngleDeg += direction * (largeStep ? 30.0f : 5.0f);
        while (settings.initialAngleDeg < 0.0f)
        {
            settings.initialAngleDeg += 360.0f;
        }
        while (settings.initialAngleDeg >= 360.0f)
        {
            settings.initialAngleDeg -= 360.0f;
        }
        break;
    default:
        break;
    }
}

OrbitObject CreateUserSatellite(LaunchSettings &settings)
{
    settings.launchCount++;

    OrbitObject object;
    object.name = "UserSat-" + std::to_string(settings.launchCount);
    object.type = ObjectType::Satellite;
    object.orbitRadius = settings.orbitRadius;
    object.inclinationDeg = settings.inclinationDeg;
    object.angularSpeed = settings.angularSpeed;
    object.initialAngleDeg = settings.initialAngleDeg;
    object.color = YELLOW;
    object.userControlled = true;
    InitializeOrbitPhysics(object);
    return object;
}

OrbitObject CreatePlayerSatellite(const LaunchSettings &settings)
{
    OrbitObject object;
    object.name = "PlayerSat";
    object.type = ObjectType::Satellite;
    object.orbitRadius = settings.orbitRadius;
    object.inclinationDeg = settings.inclinationDeg;
    object.angularSpeed = settings.angularSpeed;
    object.initialAngleDeg = settings.initialAngleDeg;
    object.color = YELLOW;
    object.userControlled = true;
    InitializeOrbitPhysics(object);
    return object;
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

int CountPlayerSatellites(const std::vector<OrbitObject> &objects)
{
    int count = 0;
    for (const OrbitObject &object : objects)
    {
        if (object.userControlled && object.name == "PlayerSat")
        {
            count++;
        }
    }
    return count;
}

int UpsertPlayerSatellite(std::vector<OrbitObject> &objects, const LaunchSettings &settings)
{
    const int existingIndex = FindPlayerSatelliteIndex(objects);
    const OrbitObject playerSat = CreatePlayerSatellite(settings);
    if (existingIndex >= 0)
    {
        objects[existingIndex] = playerSat;
        return existingIndex;
    }

    objects.push_back(playerSat);
    return static_cast<int>(objects.size()) - 1;
}

std::vector<OrbitObject> CreateDemoObjects()
{
    const Color demoSatelliteColor = {86, 166, 255, 255};

    std::vector<OrbitObject> objects = {
        {"Astra-1", ObjectType::Satellite, 150.0f, 8.0f, 0.53f, 8.0f, 0.0f, {}, demoSatelliteColor, false},
        {"Debris-A17", ObjectType::Debris, 153.0f, 11.0f, 0.49f, 17.0f, 0.0f, {}, ORANGE, false},
        {"Beacon-3", ObjectType::Satellite, 205.0f, -18.0f, -0.34f, 136.0f, 0.0f, {}, demoSatelliteColor, false},
        {"Panel-88", ObjectType::Debris, 214.0f, -20.0f, -0.28f, 165.0f, 0.0f, {}, GOLD, false},
        {"Zenith-12", ObjectType::Satellite, 255.0f, 35.0f, 0.25f, 252.0f, 0.0f, {}, demoSatelliteColor, false},
        {"Bolt-5", ObjectType::Debris, 118.0f, 58.0f, -0.72f, 284.0f, 0.0f, {}, RED, false},
        {"Relay-9", ObjectType::Satellite, 178.0f, -38.0f, 0.44f, 318.0f, 0.0f, {}, demoSatelliteColor, false},
        {"Fragment-C", ObjectType::Debris, 232.0f, 6.0f, -0.31f, 72.0f, 0.0f, {}, PINK, false},
    };

    ResetObjects(objects);
    return objects;
}

void ResetObjects(std::vector<OrbitObject> &objects)
{
    for (OrbitObject &object : objects)
    {
        InitializeOrbitPhysics(object);
    }
}

void UpdateObjects(std::vector<OrbitObject> &objects, float deltaTime, float timeScale)
{
    const float physicsDelta = deltaTime * timeScale;
    const int stepCount = std::max(1, static_cast<int>(std::ceil(std::fabs(physicsDelta) / kPredictionStepSeconds)));
    const float stepDelta = physicsDelta / static_cast<float>(stepCount);
    for (OrbitObject &object : objects)
    {
        if (!object.physicsDriven)
        {
            object.angleRad += object.angularSpeed * physicsDelta;
            object.angleRad = std::fmod(object.angleRad, 2.0f * kPi);
            RefreshObjectPosition(object);
            continue;
        }

        for (int step = 0; step < stepCount; ++step)
        {
            const float radius = Vector3Length(object.position);
            if (radius <= kEarthRadius + 0.5f)
            {
                break;
            }

            const float radiusCubed = radius * radius * radius;
            const Vector3 acceleration = Vector3Scale(object.position, -kEarthMu / radiusCubed);
            object.velocity = Vector3Add(object.velocity, Vector3Scale(acceleration, stepDelta));
            object.position = Vector3Add(object.position, Vector3Scale(object.velocity, stepDelta));
        }
        SyncOrbitFieldsFromPosition(object);
    }
}

void RefreshObjectPosition(OrbitObject &object)
{
    object.position = CalculateOrbitPosition(object.orbitRadius, object.inclinationDeg, object.angleRad);
    if (object.physicsDriven)
    {
        object.velocity = CalculateCircularOrbitVelocity(object.orbitRadius, object.inclinationDeg, object.angleRad, object.angularSpeed);
    }
}

Rectangle BackToMenuButtonBounds()
{
    return {18.0f, 18.0f, 44.0f, 44.0f};
}

bool IsPointInBackToMenuButton(Vector2 point)
{
    const Rectangle bounds = BackToMenuButtonBounds();
    return point.x >= bounds.x &&
           point.x <= bounds.x + bounds.width &&
           point.y >= bounds.y &&
           point.y <= bounds.y + bounds.height;
}

void ApplyEscapeAndWindowClose(GameMode &mode,
                               bool &shouldClose,
                               bool escapePressed,
                               bool escapeDown,
                               bool windowCloseRequested)
{
    if (windowCloseRequested && !escapeDown)
    {
        shouldClose = true;
        return;
    }

    if (mode == GameMode::MainMenu)
    {
        if (escapePressed)
        {
            shouldClose = true;
        }
        return;
    }

    if (escapePressed || escapeDown)
    {
        mode = GameMode::MainMenu;
    }
}

void UpdateOrbitCamera(OrbitCameraState &state, Camera3D &camera)
{
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    {
        const Vector2 delta = GetMouseDelta();
        state.yaw -= delta.x * 0.006f;
        state.pitch += delta.y * 0.006f;
        state.pitch = ClampFloat(state.pitch, -1.15f, 1.15f);
    }

    const float wheel = GetMouseWheelMove();
    if (wheel != 0.0f)
    {
        state.distance = ClampFloat(state.distance - wheel * 24.0f, 180.0f, 780.0f);
    }

    const float horizontalDistance = std::cos(state.pitch) * state.distance;
    camera.position = {
        std::sin(state.yaw) * horizontalDistance,
        std::sin(state.pitch) * state.distance,
        std::cos(state.yaw) * horizontalDistance};
    camera.target = {0.0f, 0.0f, 0.0f};
    camera.up = {0.0f, 1.0f, 0.0f};
}
} // namespace OrbitGuard
