#pragma once
// ui/hud.h
// HUD: кроссхейр и дебаг-оверлей.
// Вызывать между BeginDrawing() и EndDrawing(), снаружи BeginMode3D.

#include "raylib.h"
#include "../physics/physics.h"

// ----------------------------------------------------------------
//  Кроссхейр — точка в центре экрана (только когда консоль закрыта)
// ----------------------------------------------------------------
inline void HUD_DrawCrosshair() {
    int cx = GetScreenWidth()  / 2;
    int cy = GetScreenHeight() / 2;
    DrawCircle(cx, cy, 3, DARKGRAY);
}

// ----------------------------------------------------------------
//  Дебаг-оверлей состояния игрока
//  Читать ПОСЛЕ PM_PlayerMove — тогда onGround финальный.
// ----------------------------------------------------------------
inline void HUD_DrawPlayerDebug(const PlayerMove& pm) {
    const char* stateStr = pm.onGround ? "GROUNDED" : "AIRBORNE";
    Color       stateCol = pm.onGround ? GREEN : RED;

    DrawText(stateStr,                              10, 10, 20, stateCol);
    DrawText(TextFormat("vel Y:   %.2f", pm.velocity.y), 10, 35, 20, WHITE);
    DrawText(TextFormat("origin Y: %.2f", pm.origin.y),  10, 60, 20, WHITE);
    DrawText(TextFormat("speed:   %.2f",
        sqrtf(pm.velocity.x*pm.velocity.x + pm.velocity.z*pm.velocity.z)),
        10, 85, 20, WHITE);
    DrawText(TextFormat("blocked: %d", pm.lastBlocked), 10, 110, 20, LIGHTGRAY);
}

// ----------------------------------------------------------------
//  FPS счётчик
// ----------------------------------------------------------------
inline void HUD_DrawFPS(int x, int y) {
    DrawFPS(x, y);
}
