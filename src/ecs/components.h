#pragma once
// ecs/components.h
// Базовые Flecs компоненты (атомы данных).
// Каждый компонент — плоская структура без методов.
// Логика живёт в системах (systems), не здесь.

#include "raylib.h"
#include "reactphysics3d/reactphysics3d.h"

namespace rp3d = reactphysics3d;

// ----------------------------------------------------------------
//  Трансформ
// ----------------------------------------------------------------
struct CPosition  { float x = 0, y = 0, z = 0; };
struct CRotation  { float x = 0, y = 0, z = 0; };  // Euler в радианах
struct CScale     { float x = 1, y = 1, z = 1; };

// ----------------------------------------------------------------
//  Рендер (ВРЕМЕННЫЙ — до перехода на VoxelGrid + DC меш)
// ----------------------------------------------------------------
struct CRenderable {
    Color color = WHITE;
    float w = 1, h = 1, d = 1;   // размер куба
};

// ----------------------------------------------------------------
//  Физика
// ----------------------------------------------------------------
struct CPhysicsBody {
    rp3d::RigidBody* body     = nullptr;
    rp3d::Collider*  collider = nullptr;
};

// ----------------------------------------------------------------
//  Сеть (теги — данных нет, только наличие/отсутствие)
// ----------------------------------------------------------------
struct TNetworked {};  // C++ ядро реплицирует Transform + PhysicsBody
struct TLocal {};      // только на своей машине
struct TRemote {};     // принадлежит другому игроку, snapshot interpolation

// ----------------------------------------------------------------
//  ECS регистрация — вызвать один раз при инициализации
// ----------------------------------------------------------------
#include "flecs.h"

inline void RegisterCoreComponents(flecs::world& ecs) {
    ecs.component<CPosition>()  .member<float>("x").member<float>("y").member<float>("z");
    ecs.component<CRotation>()  .member<float>("x").member<float>("y").member<float>("z");
    ecs.component<CScale>()     .member<float>("x").member<float>("y").member<float>("z");
    ecs.component<CRenderable>();
    ecs.component<CPhysicsBody>();
    ecs.component<TNetworked>();
    ecs.component<TLocal>();
    ecs.component<TRemote>();
}
