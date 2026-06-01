// physics/physics.cpp
#include "physics.h"
#include <cmath>
#include <algorithm>
#include <vector>

MoveVars movevars;

// ----------------------------------------------------------------
//  Вспомогательная: vMA с другим порядком аргументов (legacy совместимость)
// ----------------------------------------------------------------
static inline Vector3 _vMA(Vector3 a, float s, Vector3 b) {
    return {a.x + s*b.x, a.y + s*b.y, a.z + s*b.z};
}

// ================================================================
//  PM_Init / PM_Destroy
// ================================================================

void PM_Init(PlayerMove& pm, rp3d::PhysicsCommon* common, rp3d::PhysicsWorld* world) {
    pm.common = common;
    pm.world  = world;

    rp3d::Transform t;
    t.setPosition(toRP(pm.origin));
    pm.body = world->createRigidBody(t);
    pm.body->setType(rp3d::BodyType::KINEMATIC);
    pm.body->setIsAllowedToSleep(false);

    float capHeight = pm.height - 2.0f * pm.radius;
    if (capHeight < 0.01f) capHeight = 0.01f;
    pm.shape    = common->createCapsuleShape(pm.radius, capHeight);
    pm.collider = pm.body->addCollider(pm.shape, rp3d::Transform::identity());
    pm.collider->getMaterial().setFrictionCoefficient(0.0f);
    pm.collider->getMaterial().setBounciness(0.0f);
}

void PM_Destroy(PlayerMove& pm) {
    if (pm.body) pm.world->destroyRigidBody(pm.body);
    pm.body = nullptr;
}

// ================================================================
//  setShape — изменить капсулу (присесть и т.п.)
// ================================================================

void PlayerMove::setShape(float newHeight, float newRadius) {
    height = newHeight;
    radius = newRadius;

    if (collider) body->removeCollider(collider);

    float capHeight = height - 2.0f * radius;
    if (capHeight < 0.01f) capHeight = 0.01f;
    shape    = common->createCapsuleShape(radius, capHeight);
    collider = body->addCollider(shape, rp3d::Transform::identity());
    collider->getMaterial().setFrictionCoefficient(0.0f);
    collider->getMaterial().setBounciness(0.0f);
}

// ================================================================
//  PM_SweepCapsule — честный шаговый sweep через RP3D
// ================================================================

static PMSweepResult PM_SweepCapsule(PlayerMove& pm, Vector3 start, Vector3 end) {
    PMSweepResult result{};
    result.endPos = end;

    Vector3 move    = vSub(end, start);
    float   moveLen = vLen(move);
    if (moveLen < 0.00001f) { result.endPos = start; return result; }

    struct OverlapCallback : public rp3d::CollisionCallback {
        struct Contact { Vector3 normal; float depth; };
        std::vector<Contact>  contacts;
        rp3d::RigidBody*      selfBody;
        explicit OverlapCallback(rp3d::RigidBody* b) : selfBody(b) {}

        void onContact(const CallbackData& data) override {
            for (rp3d::uint32 p = 0; p < data.getNbContactPairs(); ++p) {
                auto pair = data.getContactPair(p);
                if (pair.getBody1() == selfBody && pair.getBody2() == selfBody) continue;
                for (rp3d::uint32 i = 0; i < pair.getNbContactPoints(); ++i) {
                    auto cp = pair.getContactPoint(i);
                    Vector3 n = toRL(cp.getWorldNormal());
                    if (pair.getBody2() == selfBody) n = vScale(n, -1.0f);
                    contacts.push_back({n, cp.getPenetrationDepth()});
                }
            }
        }
    };

    constexpr int MAX_STEPS = 8;
    int numSteps = (int)ceilf(moveLen / (pm.radius * 0.5f));
    numSteps = std::clamp(numSteps, 1, MAX_STEPS);

    rp3d::Transform savedTransform = pm.body->getTransform();
    Vector3 safePos = start;

    for (int step = 1; step <= numSteps; ++step) {
        float   t       = (float)step / (float)numSteps;
        Vector3 testPos = vAdd(start, vScale(move, t));

        rp3d::Transform tf;
        tf.setPosition(toRP(testPos));
        pm.body->setTransform(tf);

        OverlapCallback cb(pm.body);
        pm.world->testCollision(pm.body, cb);

        if (cb.contacts.empty()) {
            safePos = testPos;
            continue;
        }

        // Нормаль с максимальной глубиной
        Vector3 bestNormal = {0, 1, 0};
        float   bestDepth  = 0.0f;
        for (auto& c : cb.contacts) {
            if (c.depth > bestDepth) {
                bestDepth  = c.depth;
                bestNormal = vNorm(c.normal);
            }
        }

        result.hit      = true;
        result.normal   = bestNormal;
        result.fraction = (float)(step - 1) / (float)numSteps;
        result.endPos   = safePos;

        pm.body->setTransform(savedTransform);
        return result;
    }

    pm.body->setTransform(savedTransform);
    result.endPos = end;
    return result;
}

// ================================================================
//  PM_GroundTrace
// ================================================================

bool PM_GroundTrace(PlayerMove& pm, Vector3& outNormal) {
    if (pm.velocity.y > 0.5f) return false;

    Vector3 end = vAdd(pm.origin, {0, -0.12f, 0});
    PMSweepResult trace = PM_SweepCapsule(pm, pm.origin, end);

    if (trace.hit && trace.normal.y >= PM_MIN_STEP_NORMAL) {
        outNormal = trace.normal;
        return true;
    }
    return false;
}

// ================================================================
//  PM_ClipVelocity
// ================================================================

Vector3 PM_ClipVelocity(Vector3 in, Vector3 normal, float overbounce) {
    float backoff = vDot(in, normal) * overbounce;
    Vector3 out = {
        in.x - normal.x * backoff,
        in.y - normal.y * backoff,
        in.z - normal.z * backoff
    };
    if (fabsf(out.x) < PM_STOP_EPSILON) out.x = 0;
    if (fabsf(out.y) < PM_STOP_EPSILON) out.y = 0;
    if (fabsf(out.z) < PM_STOP_EPSILON) out.z = 0;
    return out;
}

// ================================================================
//  PM_SlideMove
// ================================================================

int PM_SlideMove(PlayerMove& pm, float dt) {
    int     blocked    = 0;
    int     numplanes  = 0;
    Vector3 planes[PM_MAX_CLIP_PLANES];
    Vector3 primal     = pm.velocity;
    Vector3 original   = pm.velocity;
    float   time_left  = dt;

    for (int bump = 0; bump < PM_NUMBUMPS; bump++) {
        if (vLen(pm.velocity) < 0.0001f) break;

        Vector3 end = _vMA(pm.origin, time_left, pm.velocity);
        PMSweepResult trace = PM_SweepCapsule(pm, pm.origin, end);

        if (trace.hit) {
            pm.origin = trace.endPos;

            if      (trace.normal.y >= PM_MIN_STEP_NORMAL) blocked |= BLOCKED_FLOOR;
            else if (trace.normal.y == 0.0f)               blocked |= BLOCKED_STEP;
            else                                            blocked |= BLOCKED_OTHER;

            time_left -= time_left * trace.fraction;

            if (numplanes < PM_MAX_CLIP_PLANES)
                planes[numplanes++] = trace.normal;

            int i, j;
            for (i = 0; i < numplanes; i++) {
                Vector3 clipped = PM_ClipVelocity(original, planes[i], 1.01f);
                for (j = 0; j < numplanes; j++) {
                    if (j != i && vDot(clipped, planes[j]) < 0) break;
                }
                if (j == numplanes) { pm.velocity = clipped; break; }
            }

            if (i == numplanes) {
                if (numplanes == 2) {
                    Vector3 crease = vNorm(vCross(planes[0], planes[1]));
                    pm.velocity    = vScale(crease, vDot(crease, pm.velocity));
                } else {
                    pm.velocity = {0,0,0};
                    break;
                }
            }

            if (vDot(pm.velocity, primal) <= 0) {
                pm.velocity = {0,0,0};
                break;
            }
        } else {
            pm.origin = end;
            break;
        }
    }

    pm.lastBlocked = blocked;
    return blocked;
}

// ================================================================
//  PM_StepSlideMove
// ================================================================

int PM_StepSlideMove(PlayerMove& pm, float dt) {
    Vector3 origOrigin = pm.origin;
    Vector3 origVel    = pm.velocity;

    int blocked = PM_SlideMove(pm, dt);
    if (!blocked || !pm.onGround) return blocked;

    float stepsize = movevars.stepheight;

    Vector3 downOrigin = pm.origin;
    Vector3 downVel    = pm.velocity;

    pm.origin   = origOrigin;
    pm.velocity = origVel;

    // Попытка подняться на ступеньку
    PMSweepResult upTrace = PM_SweepCapsule(pm, pm.origin, vAdd(pm.origin, {0, stepsize, 0}));
    pm.origin = upTrace.endPos;

    pm.velocity.y = 0;
    PM_SlideMove(pm, dt);

    PMSweepResult downTrace = PM_SweepCapsule(pm, pm.origin, vSub(pm.origin, {0, stepsize, 0}));
    pm.origin = downTrace.endPos;

    // Выбрать путь с большим горизонтальным смещением
    auto hDist2 = [&](Vector3 a) {
        float dx = a.x - origOrigin.x, dz = a.z - origOrigin.z;
        return dx*dx + dz*dz;
    };

    if (hDist2(downOrigin) >= hDist2(pm.origin) ||
        (downTrace.hit && downTrace.normal.y < PM_MIN_STEP_NORMAL))
    {
        pm.origin   = downOrigin;
        pm.velocity = downVel;
    } else {
        pm.velocity.y = downVel.y;
    }

    return blocked;
}

// ================================================================
//  PM_Friction
// ================================================================

void PM_Friction(PlayerMove& pm, float dt) {
    if (!pm.onGround) return;

    float speed = vLen(pm.velocity);
    if (speed < 0.0001f) { pm.velocity.x = pm.velocity.z = 0; return; }

    float control  = speed < movevars.stopspeed ? movevars.stopspeed : speed;
    float newspeed = speed - control * movevars.friction * dt;
    if (newspeed < 0) newspeed = 0;

    pm.velocity = vScale(pm.velocity, newspeed / speed);
}

// ================================================================
//  PM_Accelerate / PM_AirAccelerate
// ================================================================

void PM_Accelerate(PlayerMove& pm, Vector3 wishdir, float wishspeed, float dt) {
    float cur = vDot(pm.velocity, wishdir);
    float add = wishspeed - cur;
    if (add <= 0) return;
    float accel = movevars.accelerate * dt * wishspeed;
    if (accel > add) accel = add;
    pm.velocity = _vMA(pm.velocity, accel, wishdir);
}

void PM_AirAccelerate(PlayerMove& pm, Vector3 wishdir, float wishspeed, float dt) {
    float wishspd = std::min(wishspeed, 1.0f); // ограничение для strafe-jump
    float cur = vDot(pm.velocity, wishspd > 0 ? wishdir : Vector3{0,0,0});
    float add = wishspd - cur;
    if (add <= 0) return;
    float accel = movevars.airaccelerate * wishspeed * dt;
    if (accel > add) accel = add;
    pm.velocity = _vMA(pm.velocity, accel, wishdir);
}

// ================================================================
//  PM_PlayerMove — главная функция, вызывать каждый кадр
// ================================================================

void PM_PlayerMove(PlayerMove& pm, float dt) {
    if (dt > 0.05f) dt = 0.05f; // защита от фризов

    // Направления (из камеры, Y=0)
    Vector3 fwd   = vNorm({pm.forward.x,   0, pm.forward.z});
    Vector3 right = vNorm({pm.right.x,     0, pm.right.z});

    // Желаемая скорость
    Vector3 wishvel   = _vMA(vScale(fwd, pm.forwardMove), pm.sideMove, right);
    float   wishspeed = vLen(wishvel);
    Vector3 wishdir   = wishspeed > 0.0001f ? vScale(wishvel, 1.0f / wishspeed) : Vector3{0,0,0};
    if (wishspeed > movevars.maxspeed) wishspeed = movevars.maxspeed;

    // Ground check
    Vector3 groundNormal;
    pm.onGround = PM_GroundTrace(pm, groundNormal);
    if (pm.onGround) pm.lastNormal = groundNormal;

    // Прыжок
    if (pm.jumpPressed && pm.onGround && !pm.jumpHeld) {
        pm.velocity.y = movevars.jumpspeed;
        pm.onGround   = false;
        pm.jumpHeld   = true;
    }
    if (!pm.jumpPressed) pm.jumpHeld = false;

    // Трение
    PM_Friction(pm, dt);

    // Ускорение + гравитация
    if (pm.onGround) {
        pm.velocity.y = 0;
        PM_Accelerate(pm, wishdir, wishspeed, dt);
    } else {
        pm.velocity.y -= movevars.gravity * dt;
        PM_AirAccelerate(pm, wishdir, wishspeed, dt);
    }

    // Движение
    PM_StepSlideMove(pm, dt);

    // Post-move ground check
    pm.onGround = PM_GroundTrace(pm, groundNormal);
    if (pm.onGround) {
        pm.lastNormal = groundNormal;
        if (pm.velocity.y < 0) pm.velocity.y = 0;
    }

    // Аварийный пол (не дать провалиться)
    if (pm.origin.y < pm.height * 0.5f) {
        pm.origin.y   = pm.height * 0.5f;
        pm.velocity.y = 0;
        pm.onGround   = true;
    }

    // Синхронизировать RP3D transform
    rp3d::Transform t;
    t.setPosition(toRP(pm.origin));
    pm.body->setTransform(t);

    // Обновить PMState
    pm.state = pm.onGround ? PMState::GROUNDED : PMState::AIRBORNE;
}
