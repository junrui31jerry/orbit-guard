#include "orbit_guard.h"

#include <cmath>

namespace OrbitGuard
{
void ResetShip(ShipState &ship, Vector3 position, float yaw)
{
    ship.position = position;
    ship.yaw = yaw;
    ship.speed = 0.0f;
}

void MoveShipForward(ShipState &ship, float deltaTime, float speed)
{
    ship.position.x += std::sin(ship.yaw) * speed * deltaTime;
    ship.position.z -= std::cos(ship.yaw) * speed * deltaTime;
}

void UpdateShipFromInput(ShipState &ship, float deltaTime)
{
    if (IsKeyDown(KEY_A))
    {
        ship.yaw -= 1.8f * deltaTime;
    }
    if (IsKeyDown(KEY_D))
    {
        ship.yaw += 1.8f * deltaTime;
    }
    if (IsKeyDown(KEY_W))
    {
        MoveShipForward(ship, deltaTime, 95.0f);
    }
    if (IsKeyDown(KEY_S))
    {
        MoveShipForward(ship, deltaTime, -70.0f);
    }
}

bool IsInsideBlackHoleTransferRange(const ShipState &ship, float eventHorizonRadius)
{
    return Vector3Length(ship.position) <= eventHorizonRadius;
}

void DrawShip(const ShipState &ship, Color color)
{
    const Vector3 nose = {
        ship.position.x + std::sin(ship.yaw) * 18.0f,
        ship.position.y,
        ship.position.z - std::cos(ship.yaw) * 18.0f};
    const Vector3 left = {
        ship.position.x + std::sin(ship.yaw + 2.4f) * 12.0f,
        ship.position.y,
        ship.position.z - std::cos(ship.yaw + 2.4f) * 12.0f};
    const Vector3 right = {
        ship.position.x + std::sin(ship.yaw - 2.4f) * 12.0f,
        ship.position.y,
        ship.position.z - std::cos(ship.yaw - 2.4f) * 12.0f};
    DrawTriangle3D(nose, left, right, color);
    DrawLine3D(left, right, Fade(WHITE, 0.75f));
}

void DrawSolarSystemMode(const ShipState &ship)
{
    DrawSolarSystemBackground();
    DrawModeTitle("Solar System Simulator", "Fly near planets to preview future planet introductions.");
    BeginMode3D(Camera3D{{0.0f, 320.0f, 520.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, 45.0f, CAMERA_PERSPECTIVE});
    DrawGrid(30, 24.0f);
    DrawSphere({0.0f, 0.0f, 0.0f}, 34.0f, GOLD);
    DrawSphere({110.0f, 0.0f, 0.0f}, 10.0f, SKYBLUE);
    DrawSphere({185.0f, 0.0f, 45.0f}, 14.0f, BLUE);
    DrawSphere({275.0f, 0.0f, -70.0f}, 12.0f, ORANGE);
    DrawShip(ship, YELLOW);
    EndMode3D();
    DrawText("W/S fly   A/D turn   Esc menu", 32, 672, 18, Fade(RAYWHITE, 0.78f));
}

void DrawBlackHoleMode(const ShipState &ship, bool transferred)
{
    DrawStarField();
    DrawModeTitle("Black Hole World Simulator", transferred ? "Transfer complete: unknown world preview." : "Fly toward the event horizon.");
    BeginMode3D(Camera3D{{0.0f, 190.0f, 440.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, 45.0f, CAMERA_PERSPECTIVE});
    DrawGrid(24, 24.0f);
    if (transferred)
    {
        DrawSphere({0.0f, 0.0f, 0.0f}, 72.0f, Color{80, 180, 210, 255});
        DrawSphereWires({0.0f, 0.0f, 0.0f}, 110.0f, 24, 12, Fade(SKYBLUE, 0.5f));
    }
    else
    {
        DrawSphere({0.0f, 0.0f, 0.0f}, 74.0f, BLACK);
        DrawSphereWires({0.0f, 0.0f, 0.0f}, 118.0f, 32, 16, Fade(VIOLET, 0.76f));
    }
    DrawShip(ship, transferred ? SKYBLUE : YELLOW);
    EndMode3D();
    DrawText(transferred ? "Unknown world preview   Esc menu" : "W/S fly   A/D turn   Enter event horizon to transfer   Esc menu",
             32, 672, 18, Fade(RAYWHITE, 0.78f));
}
} // namespace OrbitGuard
