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
    ShipState ship;
    ship.position = {0.0f, 0.0f, 260.0f};
    ship.yaw = 0.0f;

    MoveShipForward(ship, 1.0f, 60.0f);
    ok = Expect(ship.position.z < 260.0f, "forward movement reduces z at yaw 0") && ok;

    ship.position = {0.0f, 0.0f, 120.0f};
    ok = Expect(IsInsideBlackHoleTransferRange(ship, 130.0f), "ship inside black hole range") && ok;

    ship.position = {0.0f, 0.0f, 260.0f};
    ok = Expect(!IsInsideBlackHoleTransferRange(ship, 130.0f), "ship outside black hole range") && ok;
    return ok ? 0 : 1;
}
