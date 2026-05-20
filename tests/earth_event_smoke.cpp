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
    EarthMissionState mission;
    LaunchSettings settings;
    settings.orbitRadius = 165.0f;

    UpdateEarthMissionBeforeLaunch(mission, settings);
    ok = Expect(mission.targetLayer == OrbitLayer::MEO, "default target is MEO") && ok;
    ok = Expect(mission.deploymentComplete == false, "mission is incomplete before launch") && ok;

    std::vector<OrbitObject> objects;
    objects.push_back(CreatePlayerSatellite(settings));
    UpdateEarthMissionAfterLaunch(mission, objects);
    ok = Expect(mission.playerSatDeployed, "PlayerSat deployment is tracked") && ok;
    ok = Expect(mission.deploymentComplete, "MEO deployment completes default mission") && ok;

    ImmediateEventState event;
    StartCollisionWarning(event, 0, "Debris-A17");
    ok = Expect(event.type == ImmediateEventType::CollisionWarning, "collision event type") && ok;
    ok = Expect(event.phase == ImmediateEventPhase::WaitingForPlayer, "collision waits for player") && ok;
    ok = Expect(event.action.find("Press A") != std::string::npos, "collision action mentions A") && ok;

    UnknownScanState scan;
    StartUnknownObjectEvent(event, scan, 1);
    ok = Expect(event.type == ImmediateEventType::UnknownObject, "unknown event type") && ok;
    ok = Expect(scan.objectActive, "unknown object is active") && ok;
    ok = Expect(scan.objectIndex == 1, "unknown index is stored") && ok;

    BeginUnknownScan(event, scan);
    ok = Expect(scan.scanning, "scan starts") && ok;
    UpdateUnknownScan(event, scan, 2.0f);
    ok = Expect(scan.identified, "scan finishes after enough time") && ok;
    ok = Expect(event.phase == ImmediateEventPhase::Resolved, "event resolves after scan") && ok;
    return ok ? 0 : 1;
}
