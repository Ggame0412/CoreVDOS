#pragma once
// world/cube.h
// Struct Cube — данные одного блока мира.
// ВРЕМЕННЫЙ: заменится на SVO/VoxelGrid в Фазе 2.

#include "raylib.h"
#include "reactphysics3d/reactphysics3d.h"

namespace rp3d = reactphysics3d;

struct Cube {
    Vector3 pos;
    Vector3 size  = {1.0f, 1.0f, 1.0f};
    Color   color = GREEN;
    bool    alive = true;
    float   rotY  = 0.0f;

    // RP3D — nullptr если физика не инициализирована
    rp3d::RigidBody* body     = nullptr;
    rp3d::Collider*  collider = nullptr;
};
