#pragma once
// physics/physics.h
// Quake-style PlayerMove физика на базе ReactPhysics3D.

#include "raylib.h"

//reset macros 
#undef RED
#undef MAGENTA
#undef GREEN
#undef BLUE
#undef YELLOW
#undef WHITE
#undef BLACK
//that's all for now

#include "../core/math_utils.h"
#include "reactphysics3d/reactphysics3d.h"

// ----------------------------------------------------------------
//  Константы движения (переопределяются через MoveVars в рантайме)
// ----------------------------------------------------------------
constexpr float PM_GRAVITY         = 15.0f;
constexpr float PM_MAXSPEED        = 7.0f;
constexpr float PM_SPEED           = 5.0f;
constexpr float PM_ACCELERATE      = 1.0f;
constexpr float PM_AIRACCELERATE   = 1.0f;
constexpr float PM_FRICTION        = 6.0f;
constexpr float PM_STOPSPEED       = 3.0f;
constexpr float PM_JUMPSPEED       = 8.0f;
constexpr float PM_STEPHEIGHT      = 0.5f;
constexpr float PM_STOP_EPSILON    = 0.1f;
constexpr int   PM_MAX_CLIP_PLANES = 5;
constexpr int   PM_NUMBUMPS        = 4;
constexpr float PM_MIN_STEP_NORMAL = 0.7f;

// Флаги результата SlideMove
constexpr int BLOCKED_FLOOR = 1;
constexpr int BLOCKED_STEP  = 2;
constexpr int BLOCKED_OTHER = 4;

// ----------------------------------------------------------------
//  Состояния игрока
// ----------------------------------------------------------------
enum class PMState {
    GROUNDED,
    AIRBORNE
};

// ----------------------------------------------------------------
//  Результат sweep теста
// ----------------------------------------------------------------
struct PMSweepResult {
    bool    hit      = false;
    float   fraction = 1.0f;
    Vector3 normal   = {0, 1, 0};
    Vector3 endPos   = {};
};

// ----------------------------------------------------------------
//  PlayerMove — состояние физики игрока
// ----------------------------------------------------------------
struct PlayerMove {
    Vector3 origin   = {0, 1.8f, 0};   // центр капсулы
    Vector3 velocity = {};

    float height = 1.8f;
    float radius = 0.4f;

    PMState state    = PMState::AIRBORNE;
    bool    onGround = false;
    bool    jumpHeld = false;

    // Ввод (устанавливает PlayerCamera каждый кадр)
    float   forwardMove = 0.0f;
    float   sideMove    = 0.0f;
    bool    jumpPressed = false;

    // Направления (из камеры, Y=0, нормализованы)
    Vector3 forward = {0, 0, 1};
    Vector3 right   = {1, 0, 0};

    // RP3D объекты
    rp3d::PhysicsWorld*  world    = nullptr;
    rp3d::PhysicsCommon* common   = nullptr;
    rp3d::RigidBody*     body     = nullptr;
    rp3d::CapsuleShape*  shape    = nullptr;
    rp3d::Collider*      collider = nullptr;

    // Дебаг
    int     lastBlocked = 0;
    Vector3 lastNormal  = {};

    // Высота глаз: чуть ниже верха капсулы
    float   eyeHeight()    const { return height - height * 0.1f; }
    Vector3 getCameraPos() const {
        return {
            origin.x,
            origin.y - height * 0.5f + eyeHeight(),
            origin.z
        };
    }

    // Изменить размер капсулы (например, присесть)
    void setShape(float newHeight, float newRadius);
};

// ----------------------------------------------------------------
//  MoveVars — параметры движения, меняются из Lua в рантайме
// ----------------------------------------------------------------
struct MoveVars {
    float gravity       = PM_GRAVITY;
    float stopspeed     = PM_STOPSPEED;
    float maxspeed      = PM_MAXSPEED;
    float speed         = PM_SPEED;
    float accelerate    = PM_ACCELERATE;
    float airaccelerate = PM_AIRACCELERATE;
    float friction      = PM_FRICTION;
    float jumpspeed     = PM_JUMPSPEED;
    float stepheight    = PM_STEPHEIGHT;
};

extern MoveVars movevars; // глобальный экземпляр, один на мир

// ----------------------------------------------------------------
//  API
// ----------------------------------------------------------------
void    PM_Init(PlayerMove& pm, rp3d::PhysicsCommon* common, rp3d::PhysicsWorld* world);
void    PM_Destroy(PlayerMove& pm);

bool    PM_GroundTrace(PlayerMove& pm, Vector3& outNormal);
Vector3 PM_ClipVelocity(Vector3 in, Vector3 normal, float overbounce);
int     PM_SlideMove(PlayerMove& pm, float dt);
int     PM_StepSlideMove(PlayerMove& pm, float dt);
void    PM_Friction(PlayerMove& pm, float dt);
void    PM_Accelerate(PlayerMove& pm, Vector3 wishdir, float wishspeed, float dt);
void    PM_AirAccelerate(PlayerMove& pm, Vector3 wishdir, float wishspeed, float dt);
void    PM_PlayerMove(PlayerMove& pm, float dt);
