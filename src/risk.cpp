#include "orbit_guard.h"

#include <functional>
#include <limits>

namespace OrbitGuard
{
namespace
{
bool IsVisibleForRisk(const OrbitObject &object, bool showDemoObjects)
{
    return showDemoObjects || object.userControlled;
}

bool IsValidVisibleIndex(const std::vector<OrbitObject> &objects, int index, bool showDemoObjects)
{
    return index >= 0 &&
           index < static_cast<int>(objects.size()) &&
           IsVisibleForRisk(objects[index], showDemoObjects);
}

int OtherRiskIndex(const RiskReport &report, int objectIndex)
{
    if (report.firstIndex == objectIndex)
    {
        return report.secondIndex;
    }
    if (report.secondIndex == objectIndex)
    {
        return report.firstIndex;
    }
    return -1;
}

bool IsDebrisThreat(const std::vector<OrbitObject> &objects, int index)
{
    return index >= 0 &&
           index < static_cast<int>(objects.size()) &&
           objects[index].type == ObjectType::Debris;
}

void FinalizeRiskReport(RiskReport &report)
{
    if (report.firstIndex < 0 || report.secondIndex < 0)
    {
        report.distance = 0.0f;
        report.level = RiskLevel::Low;
        report.color = LIME;
        report.advice = "Select a visible satellite or show more objects to calculate a focused risk.";
        return;
    }

    if (report.distance < kHighRiskDistance)
    {
        report.level = RiskLevel::High;
        report.color = RED;
        report.advice = "Immediate review: prepare avoidance maneuver and increase tracking frequency.";
    }
    else if (report.distance < kMediumRiskDistance)
    {
        report.level = RiskLevel::Medium;
        report.color = ORANGE;
        report.advice = "Monitor closely: keep this pair on the watch list and compare next pass.";
    }
    else
    {
        report.level = RiskLevel::Low;
        report.color = LIME;
        report.advice = "Stable condition: continue routine monitoring.";
    }
}

void StepPredictedObject(OrbitObject &object, float deltaTime)
{
    if (!object.physicsDriven)
    {
        object.position = Vector3Add(object.position, Vector3Scale(object.velocity, deltaTime));
        return;
    }

    const float radius = Vector3Length(object.position);
    if (radius <= kEarthRadius + 0.5f)
    {
        return;
    }
    if (Vector3Length(object.velocity) <= 0.0001f)
    {
        return;
    }

    const float radiusCubed = radius * radius * radius;
    const Vector3 acceleration = Vector3Scale(object.position, -kEarthMu / radiusCubed);
    object.velocity = Vector3Add(object.velocity, Vector3Scale(acceleration, deltaTime));
    object.position = Vector3Add(object.position, Vector3Scale(object.velocity, deltaTime));
}

AvoidancePlan BuildAvoidancePlanWithEvaluator(const RiskReport &report,
                                              const std::vector<OrbitObject> &objects,
                                              int targetIndex,
                                              const std::function<RiskReport(const std::vector<OrbitObject> &)> &evaluateRisk)
{
    AvoidancePlan plan;
    plan.beforeDistance = report.distance;

    if (targetIndex < 0 || targetIndex >= static_cast<int>(objects.size()))
    {
        return plan;
    }

    const int threatIndex = OtherRiskIndex(report, targetIndex);
    if (!IsDebrisThreat(objects, threatIndex))
    {
        plan.action = "Avoidance burn is reserved for debris threats.";
        return plan;
    }

    if (report.level == RiskLevel::Low)
    {
        plan.action = "Current user satellite path is acceptable. No avoidance action is needed.";
        return plan;
    }

    struct Candidate
    {
        std::string action;
        float radiusDelta;
        float inclinationDelta;
        float speedScale;
    };

    const std::vector<Candidate> candidates = {
        {"Raise orbit radius by 25 km", 25.0f, 0.0f, 1.0f},
        {"Lower orbit radius by 25 km", -25.0f, 0.0f, 1.0f},
        {"Increase inclination by 12 deg", 0.0f, 12.0f, 1.0f},
        {"Decrease inclination by 12 deg", 0.0f, -12.0f, 1.0f},
        {"Slow angular speed by 15 percent", 0.0f, 0.0f, 0.85f},
        {"Speed angular rate by 15 percent", 0.0f, 0.0f, 1.15f},
    };

    RiskReport bestReport = report;
    OrbitObject bestObject = objects[targetIndex];
    std::string bestAction = "No better avoidance candidate found.";

    for (const Candidate &candidate : candidates)
    {
        std::vector<OrbitObject> testObjects = objects;
        OrbitObject &testObject = testObjects[targetIndex];
        testObject.orbitRadius = ClampFloat(testObject.orbitRadius + candidate.radiusDelta, 85.0f, 330.0f);
        testObject.inclinationDeg = ClampFloat(testObject.inclinationDeg + candidate.inclinationDelta, -75.0f, 75.0f);
        testObject.angularSpeed = ClampFloat(testObject.angularSpeed * candidate.speedScale, -1.20f, 1.20f);
        RefreshObjectPosition(testObject);

        const RiskReport candidateReport = evaluateRisk(testObjects);
        if (candidateReport.distance > bestReport.distance)
        {
            bestReport = candidateReport;
            bestObject = testObject;
            bestAction = candidate.action;
        }
    }

    plan.available = bestReport.distance > report.distance + 1.0f;
    plan.objectIndex = targetIndex;
    plan.afterDistance = bestReport.distance;
    plan.afterLevel = bestReport.level;
    plan.action = bestAction;
    plan.proposedObject = bestObject;

    if (!plan.available)
    {
        plan.action = "No safe quick adjustment found. Reduce speed or launch into a wider orbit.";
    }

    return plan;
}
} // namespace

const char *RiskLevelText(RiskLevel level)
{
    switch (level)
    {
    case RiskLevel::High:
        return "HIGH";
    case RiskLevel::Medium:
        return "MEDIUM";
    case RiskLevel::Low:
    default:
        return "LOW";
    }
}

RiskReport AnalyzeRisk(const std::vector<OrbitObject> &objects)
{
    return AnalyzeRisk(objects, true, -1);
}

RiskReport AnalyzeRisk(const std::vector<OrbitObject> &objects, bool showDemoObjects, int selectedObjectIndex)
{
    RiskReport report;
    report.distance = std::numeric_limits<float>::max();

    if (IsValidVisibleIndex(objects, selectedObjectIndex, showDemoObjects))
    {
        for (int j = 0; j < static_cast<int>(objects.size()); ++j)
        {
            if (j == selectedObjectIndex || !IsVisibleForRisk(objects[j], showDemoObjects))
            {
                continue;
            }

            const RiskReport pairReport = PredictPairRisk(objects, selectedObjectIndex, j);
            if (pairReport.distance < report.distance)
            {
                report = pairReport;
            }
        }

        if (report.firstIndex < 0)
        {
            FinalizeRiskReport(report);
        }
        return report;
    }

    for (int i = 0; i < static_cast<int>(objects.size()); ++i)
    {
        if (!IsVisibleForRisk(objects[i], showDemoObjects))
        {
            continue;
        }

        for (int j = i + 1; j < static_cast<int>(objects.size()); ++j)
        {
            if (!IsVisibleForRisk(objects[j], showDemoObjects))
            {
                continue;
            }

            const RiskReport pairReport = PredictPairRisk(objects, i, j);
            if (pairReport.distance < report.distance)
            {
                report = pairReport;
            }
        }
    }

    if (report.firstIndex < 0)
    {
        FinalizeRiskReport(report);
    }
    return report;
}

RiskReport PredictPairRisk(const std::vector<OrbitObject> &objects,
                           int firstIndex,
                           int secondIndex,
                           float horizonSeconds,
                           float stepSeconds)
{
    RiskReport report;
    if (firstIndex < 0 ||
        secondIndex < 0 ||
        firstIndex == secondIndex ||
        firstIndex >= static_cast<int>(objects.size()) ||
        secondIndex >= static_cast<int>(objects.size()))
    {
        FinalizeRiskReport(report);
        return report;
    }

    report.firstIndex = firstIndex < secondIndex ? firstIndex : secondIndex;
    report.secondIndex = firstIndex < secondIndex ? secondIndex : firstIndex;
    OrbitObject first = objects[firstIndex];
    OrbitObject second = objects[secondIndex];
    report.distance = Vector3Distance(first.position, second.position);
    report.closestApproachTime = 0.0f;
    report.predicted = true;

    for (float t = stepSeconds; t <= horizonSeconds + 0.001f; t += stepSeconds)
    {
        StepPredictedObject(first, stepSeconds);
        StepPredictedObject(second, stepSeconds);
        const float distance = Vector3Distance(first.position, second.position);
        if (distance < report.distance)
        {
            report.distance = distance;
            report.closestApproachTime = t;
        }
    }

    FinalizeRiskReport(report);
    return report;
}

RiskReport AnalyzePairRisk(const std::vector<OrbitObject> &objects, int firstIndex, int secondIndex)
{
    return PredictPairRisk(objects, firstIndex, secondIndex);
}

int FindControlledRiskObject(const RiskReport &report, const std::vector<OrbitObject> &objects)
{
    if (report.firstIndex >= 0 && report.firstIndex < static_cast<int>(objects.size()) && objects[report.firstIndex].userControlled)
    {
        return report.firstIndex;
    }
    if (report.secondIndex >= 0 && report.secondIndex < static_cast<int>(objects.size()) && objects[report.secondIndex].userControlled)
    {
        return report.secondIndex;
    }
    return -1;
}

AvoidancePlan BuildAvoidancePlan(const RiskReport &report, const std::vector<OrbitObject> &objects)
{
    return BuildAvoidancePlan(report, objects, true, -1);
}

AvoidancePlan BuildAvoidancePlan(const RiskReport &report,
                                 const std::vector<OrbitObject> &objects,
                                 bool showDemoObjects,
                                 int selectedObjectIndex)
{
    const int targetIndex = FindControlledRiskObject(report, objects);
    if (targetIndex < 0)
    {
        return AvoidancePlan{};
    }

    return BuildAvoidancePlanWithEvaluator(
        report,
        objects,
        targetIndex,
        [showDemoObjects, selectedObjectIndex](const std::vector<OrbitObject> &testObjects) {
            return AnalyzeRisk(testObjects, showDemoObjects, selectedObjectIndex);
        });
}

AvoidancePlan BuildAvoidancePlanForThreat(const std::vector<OrbitObject> &objects, int threatIndex)
{
    const int playerIndex = FindPlayerSatelliteIndex(objects);
    if (playerIndex < 0)
    {
        return AvoidancePlan{};
    }

    const RiskReport threatReport = AnalyzePairRisk(objects, playerIndex, threatIndex);
    return BuildAvoidancePlanWithEvaluator(
        threatReport,
        objects,
        playerIndex,
        [playerIndex, threatIndex](const std::vector<OrbitObject> &testObjects) {
            return AnalyzePairRisk(testObjects, playerIndex, threatIndex);
        });
}

void ApplyAvoidancePlan(std::vector<OrbitObject> &objects, const AvoidancePlan &plan)
{
    if (!plan.available || plan.objectIndex < 0 || plan.objectIndex >= static_cast<int>(objects.size()))
    {
        return;
    }

    objects[plan.objectIndex].orbitRadius = plan.proposedObject.orbitRadius;
    objects[plan.objectIndex].inclinationDeg = plan.proposedObject.inclinationDeg;
    objects[plan.objectIndex].angularSpeed = plan.proposedObject.angularSpeed;
    RefreshObjectPosition(objects[plan.objectIndex]);
}
} // namespace OrbitGuard
