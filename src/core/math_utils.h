#pragma once
// core/math_utils.h
// Единственный источник Vector3-математики в движке.
// Нигде больше не дублировать vAdd, vSub и пр.

#include "raylib.h"
#include <cmath>

// ----------------------------------------------------------------
//  Базовые операции
// ----------------------------------------------------------------
inline Vector3 vAdd(Vector3 a, Vector3 b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
inline Vector3 vSub(Vector3 a, Vector3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
inline Vector3 vScale(Vector3 v, float s) { return {v.x*s,   v.y*s,   v.z*s  }; }
inline Vector3 vNeg(Vector3 v)            { return {-v.x,    -v.y,    -v.z   }; }

// Multiply-add: a + b*t
inline Vector3 vMA(Vector3 a, Vector3 b, float t) {
    return {a.x + b.x*t, a.y + b.y*t, a.z + b.z*t};
}

// ----------------------------------------------------------------
//  Алгебра
// ----------------------------------------------------------------
inline float   vDot(Vector3 a, Vector3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline float   vLen(Vector3 v)            { return sqrtf(vDot(v, v)); }
inline float   vLen2(Vector3 v)           { return vDot(v, v); }

inline Vector3 vNorm(Vector3 v) {
    float l = vLen(v);
    if (l < 1e-6f) return {0,0,0};
    return vScale(v, 1.0f / l);
}

inline Vector3 vCross(Vector3 a, Vector3 b) {
    return {
        a.y*b.z - a.z*b.y,
        a.z*b.x - a.x*b.z,
        a.x*b.y - a.y*b.x
    };
}

// ----------------------------------------------------------------
//  Утилиты
// ----------------------------------------------------------------
inline bool vIsZero(Vector3 v, float eps = 1e-6f) { return vLen2(v) < eps*eps; }

// Raylib <-> ReactPhysics3D конвертация
#include "reactphysics3d/reactphysics3d.h"
namespace rp3d = reactphysics3d;

inline rp3d::Vector3 toRP(Vector3 v)   { return {v.x, v.y, v.z}; }
inline Vector3       toRL(rp3d::Vector3 v) { return {v.x, v.y, v.z}; }
