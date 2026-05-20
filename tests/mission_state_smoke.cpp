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

    MissionState mission;
    ResetMissionState(mission);

    ok = Expect(mission.currentStep == MissionStep::ConfigureOrbit, "reset starts at ConfigureOrbit") && ok;
    ok = Expect(!mission.hasLaunchedUserSatellite, "reset clears launched flag") && ok;
    ok = Expect(!mission.hasReviewedRisk, "reset clears reviewed risk flag") && ok;
    ok = Expect(!mission.hasAppliedAvoidance, "reset clears avoidance flag") && ok;
    ok = Expect(!mission.hasSavedMissionReport, "reset clears saved report flag") && ok;

    RiskReport report;
    report.level = RiskLevel::Low;
    AvoidancePlan plan;
    std::vector<OrbitObject> objects;

    UpdateMissionState(mission, report, plan, objects);
    ok = Expect(mission.currentStep == MissionStep::ConfigureOrbit, "empty object list keeps ConfigureOrbit current") && ok;
    ok = Expect(mission.nextActionText == "Next: Adjust orbit settings, then press L to launch.",
                "empty object list prompts launch setup") &&
         ok;

    OrbitObject userSatellite;
    userSatellite.name = "UserSat-1";
    userSatellite.userControlled = true;
    objects.push_back(userSatellite);

    MarkMissionLaunched(mission);
    UpdateMissionState(mission, report, plan, objects);

    ok = Expect(mission.hasLaunchedUserSatellite, "launch flag remains set after update") && ok;
    ok = Expect(mission.hasReviewedRisk, "risk review is marked after launched update") && ok;
    ok = Expect(mission.currentStep == MissionStep::SaveMissionReport, "low risk advances to SaveMissionReport") && ok;
    ok = Expect(MissionStepCompleted(mission, MissionStep::ApplyAvoidance, RiskLevel::Low),
                "low risk completes ApplyAvoidance step") &&
         ok;

    report.level = RiskLevel::High;
    plan.available = true;
    MarkMissionLaunched(mission);
    UpdateMissionState(mission, report, plan, objects);

    ok = Expect(mission.currentStep == MissionStep::ApplyAvoidance, "high risk advances to ApplyAvoidance") && ok;
    ok = Expect(mission.nextActionText == "Next: Press A to apply the suggested avoidance action.",
                "available avoidance prompts A key action") &&
         ok;

    MarkMissionAvoidanceApplied(mission);
    UpdateMissionState(mission, report, plan, objects);

    ok = Expect(mission.hasAppliedAvoidance, "avoidance flag is set") && ok;
    ok = Expect(mission.currentStep == MissionStep::SaveMissionReport, "applied avoidance advances to SaveMissionReport") && ok;

    MarkMissionReportSaved(mission);
    ok = Expect(mission.hasSavedMissionReport, "report saved flag is set") && ok;
    ok = Expect(mission.currentStep == MissionStep::Complete, "saved report completes mission") && ok;
    ok = Expect(mission.nextActionText == "Mission complete: report saved to orbit_guard_report.txt.",
                "complete mission prompt is shown") &&
         ok;

    return ok ? 0 : 1;
}
