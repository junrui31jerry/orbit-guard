#include "orbit_guard.h"

#include <iostream>
#include <vector>

using namespace OrbitGuard;

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

Vector2 GetMouseDelta()
{
    return {};
}

float GetMouseWheelMove()
{
    return 0.0f;
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

bool NearlyEqual(Vector3 first, Vector3 second, float tolerance)
{
    return Vector3Distance(first, second) <= tolerance;
}

OrbitObject MakeObject(const char *name, ObjectType type, bool player, Vector3 position, Vector3 velocity)
{
    OrbitObject object;
    object.name = name;
    object.type = type;
    object.userControlled = player;
    object.position = position;
    object.velocity = velocity;
    object.physicsDriven = true;
    object.collisionRadius = type == ObjectType::Satellite ? kDefaultSatelliteCollisionRadius : kDefaultDebrisCollisionRadius;
    return object;
}

int main()
{
    bool ok = true;

    std::vector<OrbitObject> objects = {
        MakeObject("PlayerSat", ObjectType::Satellite, true, {10000.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}),
        MakeObject("Impact-Debris-01", ObjectType::Debris, false, {10120.0f, 0.0f, 0.0f}, {-2.0f, 0.0f, 0.0f}),
    };

    RiskReport report = AnalyzePairRisk(objects, 0, 1);
    ok = Expect(report.predicted, "pair risk uses prediction for moving objects") && ok;
    ok = Expect(report.closestApproachTime > 55.0f && report.closestApproachTime < 65.0f,
                "closest approach time is predicted from future motion") &&
         ok;
    ok = Expect(report.distance < kHighRiskDistance, "future close approach becomes high risk") && ok;
    ok = Expect(report.level == RiskLevel::High, "predicted close approach sets high risk") && ok;

    const RiskReport zeroStepReport = PredictPairRisk(objects, 0, 1, 10.0f, 0.0f);
    ok = Expect(zeroStepReport.predicted, "zero prediction step falls back to a bounded prediction") && ok;
    ok = Expect(zeroStepReport.closestApproachTime >= 0.0f, "zero prediction step does not produce invalid timing") && ok;

    const RiskReport negativeHorizonReport = PredictPairRisk(objects, 0, 1, -10.0f, 0.5f);
    ok = Expect(negativeHorizonReport.predicted, "negative prediction horizon still reports the current pair") && ok;
    ok = Expect(negativeHorizonReport.closestApproachTime == 0.0f, "negative prediction horizon clamps to current time") && ok;
    ok = Expect(negativeHorizonReport.distance > 119.0f && negativeHorizonReport.distance < 121.0f,
                "negative prediction horizon uses current distance") &&
         ok;

    LaunchSettings settings;
    OrbitObject player = CreatePlayerSatellite(settings);
    OrbitObject debris = player;
    debris.name = "Factory-Debris";
    debris.type = ObjectType::Debris;
    debris.userControlled = false;
    debris.position = Vector3Add(player.position, Vector3Scale(player.velocity, 3.0f));
    debris.velocity = Vector3Scale(player.velocity, 0.80f);
    debris.collisionRadius = kDefaultDebrisCollisionRadius;

    std::vector<OrbitObject> physicsObjects = {player, debris};
    const Vector3 originalPlayerPosition = physicsObjects[0].position;
    const Vector3 originalPlayerVelocity = physicsObjects[0].velocity;
    const Vector3 originalDebrisPosition = physicsObjects[1].position;
    const Vector3 originalDebrisVelocity = physicsObjects[1].velocity;

    const RiskReport physicsReport = PredictPairRisk(physicsObjects, 0, 1, 20.0f, kPredictionStepSeconds);
    ok = Expect(physicsReport.predicted, "factory physics objects use future prediction") && ok;
    ok = Expect(physicsReport.closestApproachTime > 0.0f, "factory physics objects find a future closest approach") && ok;
    ok = Expect(physicsReport.distance < Vector3Distance(originalPlayerPosition, originalDebrisPosition),
                "factory physics prediction can improve on current separation") &&
         ok;
    ok = Expect(NearlyEqual(physicsObjects[0].position, originalPlayerPosition, 0.0001f) &&
                    NearlyEqual(physicsObjects[0].velocity, originalPlayerVelocity, 0.0001f) &&
                    NearlyEqual(physicsObjects[1].position, originalDebrisPosition, 0.0001f) &&
                    NearlyEqual(physicsObjects[1].velocity, originalDebrisVelocity, 0.0001f),
                "prediction does not mutate input positions or velocities") &&
         ok;

    return ok ? 0 : 1;
}
