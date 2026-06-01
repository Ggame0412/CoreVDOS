// player/player_camera.cpp
#include "player_camera.h"
#include "../core/math_utils.h"
#include <cmath>

void PlayerCamera::init(PlayerMove* p) {
    pm             = p;
    cam.position   = pm->getCameraPos();
    cam.target     = {cam.position.x, cam.position.y, cam.position.z + 1.0f};
    cam.up         = {0, 1, 0};
    cam.fovy       = 70.0f;
    cam.projection = CAMERA_PERSPECTIVE;
}

Camera3D makeOverheadCam(Vector3 playerPos) {
    Camera3D cam  = {};
    float    dist = 20.0f;
    cam.position  = {playerPos.x + dist, playerPos.y + dist, playerPos.z + dist};
    cam.target    = playerPos;
    cam.up        = {0.0f, 1.0f, 0.0f};
    cam.fovy      = 15.0f;
    cam.projection = CAMERA_ORTHOGRAPHIC;
    return cam;
}

Vector3 PlayerCamera::getForward() const {
    return {
        cosf(pitch) * sinf(yaw),
        sinf(pitch),
        cosf(pitch) * cosf(yaw)
    };
}

Vector3 PlayerCamera::getRight() const {
    return { cosf(yaw), 0.0f, sinf(yaw) };
}

void PlayerCamera::update(float dt) {
    // Единственный источник истины для блокировки движения: IsCursorHidden().
    // ImGui::WantCaptureKeyboard НЕ используем — он может зависнуть в true
    // после закрытия консоли и намертво заблокировать ходьбу.
    if (!IsCursorHidden()) {
        pm->forwardMove = 0;
        pm->sideMove    = 0;
        pm->jumpPressed = false;
        cam.position    = pm->getCameraPos();
        cam.target      = vAdd(cam.position, getForward());
        return;
    }

    // Mouse-look
    Vector2 mouse = GetMouseDelta();
    yaw   -= mouse.x * sensitivity;
    pitch -= mouse.y * sensitivity;
    pitch  = std::clamp(pitch, -1.5f, 1.5f);

    // Обновить направления в PlayerMove
    pm->forward = getForward();
    pm->right   = getRight();

    // WASD ввод
    pm->forwardMove = 0;
    pm->sideMove    = 0;
    if (IsKeyDown(KEY_W)) pm->forwardMove =  movevars.speed;
    if (IsKeyDown(KEY_S)) pm->forwardMove = -movevars.speed;
    if (IsKeyDown(KEY_A)) pm->sideMove    = -movevars.speed;
    if (IsKeyDown(KEY_D)) pm->sideMove    =  movevars.speed;
    pm->jumpPressed = IsKeyDown(KEY_SPACE);

    // Обновить камеру
    cam.position = pm->getCameraPos();
    cam.target   = vAdd(cam.position, getForward());
}
