#pragma once
// world/dual_contouring.h
// Dual Contouring — генерация треугольного меша из SVO.
//
// РИСКИ (из ARCHITECTURE.md):
//   R1 — T-junctions: граничные рёбра читают из СОСЕДНЕГО чанка через SVO.
//        Вершины на границе выравниваются по данным обоих чанков → нет щелей.
//   R2 — Краш при передаче меша: двойная буферизация.
//        pendingMesh строится в воркере.
//        activeMesh неизменен во время physWorld->update() и рендера.
//        Обмен — только в SafeSwapPoint (между update и рендером).

#ifndef SVO_NO_RAYLIB
  #include "raylib.h"
#else
  struct Vector3 { float x, y, z; };
#endif

#include "svo.h"
#include <vector>
#include <array>
#include <atomic>
#include <mutex>
#include <cmath>
#include <algorithm>

// ================================================================
//  Меш чанка
// ================================================================

struct DCVertex {
    Vector3 pos;    // рендер-координаты (через FloatingOrigin)
    Vector3 normal;
};

struct DCMesh {
    std::vector<DCVertex>   vertices;
    std::vector<uint32_t>   indices;
    bool                    valid = false;

    void clear() { vertices.clear(); indices.clear(); valid = false; }

    bool empty() const { return indices.empty(); }
};

// ================================================================
//  Двойной буфер меша (R2)
//
//  Воркер пишет в pending.
//  Рендер/физика читают из active.
//  safeSwap() вызывается ТОЛЬКО в SafeSwapPoint —
//  между physWorld->update() и BeginDrawing().
// ================================================================

struct ChunkMeshBuffer {
    DCMesh active;   // читается рендером и физикой — НЕ трогать из воркера
    DCMesh pending;  // пишется воркером — НЕ читать из рендера

    std::atomic<bool> swapReady{false};  // воркер выставил, main читает

    // Вызывать только в SafeSwapPoint (main-тред, после physWorld->update())
    bool safeSwap() {
        if (!swapReady.load(std::memory_order_acquire)) return false;
        std::swap(active, pending);
        pending.clear();
        swapReady.store(false, std::memory_order_release);
        return true;
    }

    // Воркер вызывает после завершения генерации
    void submitPending() {
        pending.valid = true;
        swapReady.store(true, std::memory_order_release);
    }
};

// ================================================================
//  Вспомогательные типы DC
// ================================================================

// Позиция вершины DC внутри ячейки (QEF минимум)
struct DCVert {
    float x, y, z;   // в локальных координатах чанка [0, CHUNK_SIZE]
    float nx, ny, nz; // нормаль (усреднённая по рёбрам ячейки)
    bool  valid = false;
};

// Индексы 12 рёбер куба
static constexpr int EDGE_TABLE[12][2] = {
    {0,1},{1,3},{2,3},{0,2},  // нижняя грань
    {4,5},{5,7},{6,7},{4,6},  // верхняя грань
    {0,4},{1,5},{2,6},{3,7}   // вертикальные
};

// 8 угловых смещений ячейки
static constexpr int CORNER_OFFSETS[8][3] = {
    {0,0,0},{1,0,0},{0,1,0},{1,1,0},
    {0,0,1},{1,0,1},{0,1,1},{1,1,1}
};

// ================================================================
//  Запрос плотности с поддержкой соседних чанков (R1)
//
//  При запросе координат за пределами чанка [0, CHUNK_SIZE)
//  функция обращается к SVO напрямую через мировые координаты.
//  Это гарантирует что вершины на границе чанков вычисляются
//  из одних и тех же данных с обеих сторон → нет T-junctions.
// ================================================================

static float sampleDensity(const SVO& svo,
                            ChunkKey   ck,
                            int lx, int ly, int lz)
{
    // Внутри чанка — быстрый путь через локальные координаты
    if (lx >= 0 && lx < CHUNK_SIZE &&
        ly >= 0 && ly < CHUNK_SIZE &&
        lz >= 0 && lz < CHUNK_SIZE)
    {
        const Chunk* chunk = svo.getChunk(ck);
        if (!chunk) return -1.0f;
        // Читаем через WorldPos чтобы не дублировать логику навигации
        WorldPos wp = {
            ck.cx * (int64_t)CHUNK_SIZE + lx,
            ck.cy * (int64_t)CHUNK_SIZE + ly,
            ck.cz * (int64_t)CHUNK_SIZE + lz
        };
        return svo.getDensity(wp);
    }

    // ---- R1: за границей — читаем из SVO по мировым координатам ----
    // Это тот же путь что и соседний чанк использует для своих вершин.
    // Результат детерминирован с обеих сторон границы.
    WorldPos wp = {
        ck.cx * (int64_t)CHUNK_SIZE + lx,
        ck.cy * (int64_t)CHUNK_SIZE + ly,
        ck.cz * (int64_t)CHUNK_SIZE + lz
    };
    return svo.getDensity(wp);
}

// ================================================================
//  QEF (Quadratic Error Function) — минимайзер
//
//  Упрощённая версия: усредняем точки пересечений рёбер.
//  Полный SVD-QEF сложнее, но для нашего масштаба достаточно.
//  Острые углы (manifold DC) — следующая итерация.
// ================================================================

static DCVert solveQEF(const std::array<float,3>(&intersections)[12],
                       const std::array<float,3>(&normals)[12],
                       int intersectionCount)
{
    DCVert v{};
    if (intersectionCount == 0) return v;

    float sx = 0, sy = 0, sz = 0;
    float nx = 0, ny = 0, nz = 0;

    for (int i = 0; i < 12; i++) {
        if (normals[i][0] == 0 && normals[i][1] == 0 && normals[i][2] == 0) continue;
        sx += intersections[i][0];
        sy += intersections[i][1];
        sz += intersections[i][2];
        nx += normals[i][0];
        ny += normals[i][1];
        nz += normals[i][2];
    }

    float inv = 1.0f / intersectionCount;
    v.x = sx * inv;
    v.y = sy * inv;
    v.z = sz * inv;

    float nl = sqrtf(nx*nx + ny*ny + nz*nz);
    if (nl > 1e-6f) { v.nx = nx/nl; v.ny = ny/nl; v.nz = nz/nl; }
    else            { v.nx = 0;     v.ny = 1;     v.nz = 0; }

    v.valid = true;
    return v;
}

// ================================================================
//  Градиент плотности (аппроксимация нормали через конечные разности)
// ================================================================

static std::array<float,3> gradientAt(const SVO& svo, ChunkKey ck,
                                       int lx, int ly, int lz)
{
    float dx = sampleDensity(svo, ck, lx+1,ly,lz) - sampleDensity(svo, ck, lx-1,ly,lz);
    float dy = sampleDensity(svo, ck, lx,ly+1,lz) - sampleDensity(svo, ck, lx,ly-1,lz);
    float dz = sampleDensity(svo, ck, lx,ly,lz+1) - sampleDensity(svo, ck, lx,ly,lz-1);
    float len = sqrtf(dx*dx + dy*dy + dz*dz);
    if (len < 1e-6f) return {0,1,0};
    return {dx/len, dy/len, dz/len};
}

// ================================================================
//  generateChunkMesh — основная функция DC
//
//  Алгоритм:
//    1. Для каждой ячейки (lx,ly,lz) размером 1³ в чанке:
//       a. Читаем плотность в 8 углах (с выходом за границу → R1)
//       b. Если ячейка полностью solid или полностью empty — пропуск
//       c. Находим пересечения изоповерхности на активных рёбрах
//       d. QEF → позиция вершины DC
//    2. Для каждого активного ребра (смена знака вдоль оси):
//       Соединяем вершины 4 ячеек вокруг ребра → 2 треугольника
//
//  Параметр chunkWorldBase — мировые координаты угла чанка (int64).
//  В рендер переводим через g_origin.toRenderF().
// ================================================================

inline void generateChunkMesh(const SVO& svo,
                               ChunkKey   ck,
                               DCMesh&    out)
{
    out.clear();

    // Нужно +1 по каждой оси для проверки рёбер на границе
    constexpr int N = CHUNK_SIZE + 1;

    // Индексная сетка вершин DC: -1 = нет вершины
    // Размер (N)³ — покрывает ячейки [0..CHUNK_SIZE-1] и их границы
    std::vector<int32_t> vertIndex(N * N * N, -1);

    auto vidx = [&](int x, int y, int z) -> int32_t& {
        return vertIndex[x + y*N + z*N*N];
    };

    // Шаг 1: Для каждой ячейки найти вершину DC
    for (int lz = 0; lz < CHUNK_SIZE; lz++)
    for (int ly = 0; ly < CHUNK_SIZE; ly++)
    for (int lx = 0; lx < CHUNK_SIZE; lx++)
    {
        // Плотности в 8 углах ячейки
        float d[8];
        for (int c = 0; c < 8; c++) {
            d[c] = sampleDensity(svo, ck,
                lx + CORNER_OFFSETS[c][0],
                ly + CORNER_OFFSETS[c][1],
                lz + CORNER_OFFSETS[c][2]);
        }

        // Ячейка однородна — пропуск
        bool anyPos = false, anyNeg = false;
        for (int c = 0; c < 8; c++) {
            if (d[c] > 0) anyPos = true;
            else          anyNeg = true;
        }
        if (!anyPos || !anyNeg) continue;

        // Найти пересечения активных рёбер
        std::array<float,3> intersections[12]{};
        std::array<float,3> normals[12]{};
        int iCount = 0;

        for (int e = 0; e < 12; e++) {
            int c0 = EDGE_TABLE[e][0], c1 = EDGE_TABLE[e][1];
            float d0 = d[c0], d1 = d[c1];
            if ((d0 > 0) == (d1 > 0)) continue; // нет смены знака

            // Линейная интерполяция вдоль ребра
            float t  = d0 / (d0 - d1);
            float ix = lx + CORNER_OFFSETS[c0][0] + t*(CORNER_OFFSETS[c1][0] - CORNER_OFFSETS[c0][0]);
            float iy = ly + CORNER_OFFSETS[c0][1] + t*(CORNER_OFFSETS[c1][1] - CORNER_OFFSETS[c0][1]);
            float iz = lz + CORNER_OFFSETS[c0][2] + t*(CORNER_OFFSETS[c1][2] - CORNER_OFFSETS[c0][2]);

            intersections[e] = {ix, iy, iz};

            // Нормаль через градиент в точке пересечения
            int ix0 = (int)roundf(ix), iy0 = (int)roundf(iy), iz0 = (int)roundf(iz);
            normals[e] = gradientAt(svo, ck, ix0, iy0, iz0);
            iCount++;
        }

        if (iCount == 0) continue;

        DCVert vert = solveQEF(intersections, normals, iCount);
        if (!vert.valid) continue;

        // Запомнить вершину
        uint32_t vIdx = (uint32_t)out.vertices.size();
        vidx(lx, ly, lz) = (int32_t)vIdx;

        // Перевод в рендер-координаты через FloatingOrigin
        int64_t wx = ck.cx * (int64_t)CHUNK_SIZE + (int64_t)vert.x;
        int64_t wy = ck.cy * (int64_t)CHUNK_SIZE + (int64_t)vert.y;
        int64_t wz = ck.cz * (int64_t)CHUNK_SIZE + (int64_t)vert.z;

        DCVertex v;
        v.pos    = g_origin.toRenderF(wx, wy, wz);
        v.normal = {vert.nx, vert.ny, vert.nz};
        out.vertices.push_back(v);
    }

    // Шаг 2: Для каждого активного ребра по X/Y/Z — выдать 2 треугольника
    // Ребро активно если есть смена знака. Треугольники строятся из
    // 4 ячеек, разделяющих это ребро.

    // Ребро вдоль X (y-z плоскость)
    for (int lz = 1; lz < CHUNK_SIZE; lz++)
    for (int ly = 1; ly < CHUNK_SIZE; ly++)
    for (int lx = 0; lx < CHUNK_SIZE; lx++)
    {
        float d0 = sampleDensity(svo, ck, lx,   ly, lz);
        float d1 = sampleDensity(svo, ck, lx+1, ly, lz);
        if ((d0 > 0) == (d1 > 0)) continue;

        int32_t i0 = vidx(lx, ly-1, lz-1);
        int32_t i1 = vidx(lx, ly,   lz-1);
        int32_t i2 = vidx(lx, ly-1, lz  );
        int32_t i3 = vidx(lx, ly,   lz  );
        if (i0 < 0 || i1 < 0 || i2 < 0 || i3 < 0) continue;

        bool flip = d0 < 0; // ориентация по нормали
        if (!flip) {
            out.indices.push_back(i0); out.indices.push_back(i1); out.indices.push_back(i2);
            out.indices.push_back(i1); out.indices.push_back(i3); out.indices.push_back(i2);
        } else {
            out.indices.push_back(i2); out.indices.push_back(i1); out.indices.push_back(i0);
            out.indices.push_back(i2); out.indices.push_back(i3); out.indices.push_back(i1);
        }
    }

    // Ребро вдоль Y
    for (int lz = 1; lz < CHUNK_SIZE; lz++)
    for (int ly = 0; ly < CHUNK_SIZE; ly++)
    for (int lx = 1; lx < CHUNK_SIZE; lx++)
    {
        float d0 = sampleDensity(svo, ck, lx, ly,   lz);
        float d1 = sampleDensity(svo, ck, lx, ly+1, lz);
        if ((d0 > 0) == (d1 > 0)) continue;

        int32_t i0 = vidx(lx-1, ly, lz-1);
        int32_t i1 = vidx(lx,   ly, lz-1);
        int32_t i2 = vidx(lx-1, ly, lz  );
        int32_t i3 = vidx(lx,   ly, lz  );
        if (i0 < 0 || i1 < 0 || i2 < 0 || i3 < 0) continue;

        bool flip = d0 > 0;
        if (!flip) {
            out.indices.push_back(i0); out.indices.push_back(i1); out.indices.push_back(i2);
            out.indices.push_back(i1); out.indices.push_back(i3); out.indices.push_back(i2);
        } else {
            out.indices.push_back(i2); out.indices.push_back(i1); out.indices.push_back(i0);
            out.indices.push_back(i2); out.indices.push_back(i3); out.indices.push_back(i1);
        }
    }

    // Ребро вдоль Z
    for (int lz = 0; lz < CHUNK_SIZE; lz++)
    for (int ly = 1; ly < CHUNK_SIZE; ly++)
    for (int lx = 1; lx < CHUNK_SIZE; lx++)
    {
        float d0 = sampleDensity(svo, ck, lx, ly, lz  );
        float d1 = sampleDensity(svo, ck, lx, ly, lz+1);
        if ((d0 > 0) == (d1 > 0)) continue;

        int32_t i0 = vidx(lx-1, ly-1, lz);
        int32_t i1 = vidx(lx,   ly-1, lz);
        int32_t i2 = vidx(lx-1, ly,   lz);
        int32_t i3 = vidx(lx,   ly,   lz);
        if (i0 < 0 || i1 < 0 || i2 < 0 || i3 < 0) continue;

        bool flip = d0 < 0;
        if (!flip) {
            out.indices.push_back(i0); out.indices.push_back(i1); out.indices.push_back(i2);
            out.indices.push_back(i1); out.indices.push_back(i3); out.indices.push_back(i2);
        } else {
            out.indices.push_back(i2); out.indices.push_back(i1); out.indices.push_back(i0);
            out.indices.push_back(i2); out.indices.push_back(i3); out.indices.push_back(i1);
        }
    }

    out.valid = !out.indices.empty();
}