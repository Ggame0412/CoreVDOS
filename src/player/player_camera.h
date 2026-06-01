#pragma once
// player/player_camera.h
// Перволичная камера с mouse-look. Читает ввод и пишет в PlayerMove.

#include "raylib.h"
#include "../physics/physics.h"

struct PlayerCamera {
    Camera3D    cam;
    float       yaw         = 0.0f;
    float       pitch       = 0.0f;
    float       sensitivity = 0.003f;
    PlayerMove* pm          = nullptr;

    void    init(PlayerMove* p);

    // Вызывать каждый кадр. Если курсор видим — движение заморожено.
    void    update(float dt);

    Vector3 getForward() const;
    Vector3 getRight()   const;
};

// Overhead (F2) камера — изометрический вид сверху
Camera3D makeOverheadCam(Vector3 playerPos);
