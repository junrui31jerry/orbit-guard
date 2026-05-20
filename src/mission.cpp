#include "orbit_guard.h"

namespace OrbitGuard
{
namespace
{
const char *kConfigurePrompt = "Next: Adjust orbit settings, then press L to launch.";
const char *kReviewRiskPrompt = "Next: Review the closest-pair risk level.";
const char *kApplyAvoidancePrompt = "Next: Press A to apply the suggested avoidance action.";
const char *kNoAvoidancePrompt = "Next: Adjust orbit settings or wait for a safer avoidance option.";
const char *kSaveReportPrompt = "Next: Press S to save the mission risk report.";
const char *kSaveAvoidedReportPrompt = "Next: Review the avoidance result, then press S to save the report.";
const char *kCompletePrompt = "Mission complete: report saved to orbit_guard_report.txt.";

bool HasUserControlledSatellite(const std::vector<OrbitObject> &objects)
{
    for (const OrbitObject &object : objects)
    {
        if (object.userControlled)
        {
            return true;
        }
    }

    return false;
}
} // namespace

void ResetMissionState(MissionState &mission)
{
    mission.hasLaunchedUserSatellite = false;
    mission.hasReviewedRisk = false;
    mission.hasAppliedAvoidance = false;
    mission.hasSavedMissionReport = false;
    mission.currentStep = MissionStep::ConfigureOrbit;
    mission.nextActionText = kConfigurePrompt;
}

void MarkMissionLaunched(MissionState &mission)
{
    mission.hasLaunchedUserSatellite = true;
    mission.hasReviewedRisk = false;
    mission.hasAppliedAvoidance = false;
    mission.hasSavedMissionReport = false;
    mission.currentStep = MissionStep::ReviewRisk;
    mission.nextActionText = kReviewRiskPrompt;
}

void MarkMissionAvoidanceApplied(MissionState &mission)
{
    mission.hasAppliedAvoidance = true;
    mission.hasSavedMissionReport = false;
    mission.currentStep = MissionStep::SaveMissionReport;
    mission.nextActionText = kSaveAvoidedReportPrompt;
}

void MarkMissionReportSaved(MissionState &mission)
{
    mission.hasSavedMissionReport = true;
    mission.currentStep = MissionStep::Complete;
    mission.nextActionText = kCompletePrompt;
}

void UpdateMissionState(MissionState &mission,
                        const RiskReport &report,
                        const AvoidancePlan &avoidancePlan,
                        const std::vector<OrbitObject> &objects)
{
    if (!HasUserControlledSatellite(objects))
    {
        ResetMissionState(mission);
        return;
    }

    mission.hasLaunchedUserSatellite = true;
    mission.hasReviewedRisk = true;

    if (mission.hasSavedMissionReport)
    {
        mission.currentStep = MissionStep::Complete;
        mission.nextActionText = kCompletePrompt;
        return;
    }

    const bool needsAvoidance = report.level == RiskLevel::Medium || report.level == RiskLevel::High;
    if (needsAvoidance && !mission.hasAppliedAvoidance)
    {
        mission.currentStep = MissionStep::ApplyAvoidance;
        mission.nextActionText = avoidancePlan.available ? kApplyAvoidancePrompt : kNoAvoidancePrompt;
        return;
    }

    mission.currentStep = MissionStep::SaveMissionReport;
    mission.nextActionText = mission.hasAppliedAvoidance ? kSaveAvoidedReportPrompt : kSaveReportPrompt;
}

bool MissionStepCompleted(const MissionState &mission, MissionStep step, RiskLevel currentRiskLevel)
{
    switch (step)
    {
    case MissionStep::ConfigureOrbit:
    case MissionStep::LaunchSatellite:
        return mission.hasLaunchedUserSatellite;
    case MissionStep::ReviewRisk:
        return mission.hasReviewedRisk;
    case MissionStep::ApplyAvoidance:
        return mission.hasAppliedAvoidance || (mission.hasReviewedRisk && currentRiskLevel == RiskLevel::Low);
    case MissionStep::SaveMissionReport:
    case MissionStep::Complete:
        return mission.hasSavedMissionReport;
    default:
        return false;
    }
}

const char *MissionStepTitle(MissionStep step)
{
    switch (step)
    {
    case MissionStep::ConfigureOrbit:
        return "Configure Orbit";
    case MissionStep::LaunchSatellite:
        return "Launch Satellite";
    case MissionStep::ReviewRisk:
        return "Review Risk";
    case MissionStep::ApplyAvoidance:
        return "Apply Avoidance";
    case MissionStep::SaveMissionReport:
        return "Save Mission Report";
    case MissionStep::Complete:
        return "Complete";
    default:
        return "Mission Step";
    }
}

const char *MissionStepStatusText(const MissionState &mission, MissionStep step, RiskLevel currentRiskLevel)
{
    if (MissionStepCompleted(mission, step, currentRiskLevel))
    {
        return "Done";
    }

    if (mission.currentStep == step)
    {
        return "Current";
    }

    return "Waiting";
}
} // namespace OrbitGuard
