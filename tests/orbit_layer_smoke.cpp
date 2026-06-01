#include "orbit_guard.h"

#include <iostream>

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
    ok = Expect(ClassifyOrbitLayer(100.0f) == OrbitLayer::LEO, "100 is LEO") && ok;
    ok = Expect(ClassifyOrbitLayer(165.0f) == OrbitLayer::MEO, "165 is MEO") && ok;
    ok = Expect(ClassifyOrbitLayer(260.0f) == OrbitLayer::GEO, "260 is GEO") && ok;
    ok = Expect(ClassifyOrbitLayer(70.0f) == OrbitLayer::BelowOperational, "70 is below operational") && ok;
    ok = Expect(ClassifyOrbitLayer(350.0f) == OrbitLayer::BeyondOperational, "350 is beyond operational") && ok;
    ok = Expect(std::string(OrbitLayerText(OrbitLayer::MEO)) == "MEO", "MEO label") && ok;
    ok = Expect(IsOrbitLayerMatch(OrbitLayer::MEO, 146.0f), "MEO lower bound matches") && ok;
    ok = Expect(IsOrbitLayerMatch(OrbitLayer::MEO, 230.0f), "MEO upper bound matches") && ok;
    ok = Expect(!IsOrbitLayerMatch(OrbitLayer::MEO, 231.0f), "MEO rejects GEO radius") && ok;
    return ok ? 0 : 1;
}
