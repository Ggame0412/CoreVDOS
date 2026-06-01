#pragma once
// world/cube_physics.h
// Инициализация/уничтожение RP3D физики для Cube.
// ВРЕМЕННЫЙ: уйдёт когда Cube заменится на SVO + TriangleMesh.

#include "cube.h"

// Создаёт STATIC rigidbody + BoxShape. Безопасен при повторном вызове (проверяет body != nullptr).
inline void Cube_InitPhysics(Cube& cube,
                             rp3d::PhysicsCommon& physCommon,
                             rp3d::PhysicsWorld*  physWorld)
{
    if (cube.body) return; // уже инициализирован

    rp3d::Transform t;
    t.setPosition({cube.pos.x, cube.pos.y, cube.pos.z});
    t.setOrientation(rp3d::Quaternion::fromEulerAngles(
        0.0f, cube.rotY * (3.14159265f / 180.0f), 0.0f
    ));

    cube.body = physWorld->createRigidBody(t);
    cube.body->setType(rp3d::BodyType::STATIC);

    rp3d::BoxShape* shape = physCommon.createBoxShape(
        {cube.size.x * 0.5f, cube.size.y * 0.5f, cube.size.z * 0.5f}
    );
    cube.collider = cube.body->addCollider(shape, rp3d::Transform::identity());
}

// Уничтожает rigidbody, зануляет указатели.
inline void Cube_DestroyPhysics(Cube& cube, rp3d::PhysicsWorld* physWorld)
{
    if (!cube.body) return;
    physWorld->destroyRigidBody(cube.body); // RP3D сам удаляет коллайдер и shape
    cube.body     = nullptr;
    cube.collider = nullptr;
}
