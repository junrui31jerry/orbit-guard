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

    GameMode earthMode = GameMode::EarthSpace;
    bool shouldClose = false;
    ApplyEscapeAndWindowClose(earthMode, shouldClose, false, true, true);
    ok = Expect(earthMode == GameMode::MainMenu, "ESC close signal in simulator returns to menu") && ok;
    ok = Expect(!shouldClose, "ESC close signal in simulator does not close the app") && ok;

    GameMode solarMode = GameMode::SolarSystem;
    shouldClose = false;
    ApplyEscapeAndWindowClose(solarMode, shouldClose, true, true, false);
    ok = Expect(solarMode == GameMode::MainMenu, "normal ESC press outside menu returns to menu") && ok;
    ok = Expect(!shouldClose, "normal ESC press outside menu does not close the app") && ok;

    GameMode heldEscapeMode = GameMode::EarthSpace;
    shouldClose = false;
    ApplyEscapeAndWindowClose(heldEscapeMode, shouldClose, false, true, false);
    ok = Expect(heldEscapeMode == GameMode::MainMenu, "held ESC outside menu returns to menu") && ok;
    ok = Expect(!shouldClose, "held ESC outside menu does not close the app") && ok;

    GameMode closeButtonMode = GameMode::EarthSpace;
    shouldClose = false;
    ApplyEscapeAndWindowClose(closeButtonMode, shouldClose, false, false, true);
    ok = Expect(shouldClose, "window close button still closes the app") && ok;

    GameMode menuMode = GameMode::MainMenu;
    shouldClose = false;
    ApplyEscapeAndWindowClose(menuMode, shouldClose, true, true, false);
    ok = Expect(shouldClose, "ESC on main menu still quits") && ok;

    const Rectangle buttonBounds = BackToMenuButtonBounds();
    ok = Expect(buttonBounds.x <= 20.0f && buttonBounds.y <= 20.0f,
                "back-to-menu button is anchored in the top-left corner") &&
         ok;
    ok = Expect(IsPointInBackToMenuButton({buttonBounds.x + buttonBounds.width * 0.5f,
                                           buttonBounds.y + buttonBounds.height * 0.5f}),
                "back-to-menu button accepts clicks inside its bounds") &&
         ok;
    ok = Expect(!IsPointInBackToMenuButton({buttonBounds.x + buttonBounds.width + 20.0f,
                                            buttonBounds.y + buttonBounds.height + 20.0f}),
                "back-to-menu button rejects clicks outside its bounds") &&
         ok;

    return ok ? 0 : 1;
}
