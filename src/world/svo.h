#pragma once
// world/svo.h
// Sparse Voxel Octree (SVO) для бесконечного мира.
//
// АРХИТЕКТУРА:
//   Мировые координаты  — int64 (вокселей). Точные, бесконечные.
//   Рендер-координаты   — float (всегда вблизи нуля, Floating Origin).
//   Чанк               — локальный Octree глубиной CHUNK_DEPTH (16³ вокселей).
//   ChunkMap           — std::unordered_map<ChunkKey, OctreeNode*>.
//
// FLOATING ORIGIN (R: нет дрожания float):
//   renderPos = vec3(worldPos - g_originOffset)
//   При смещении игрока на ±CHUNK_SHIFT_THRESHOLD — originOffset обновляется.
//
// РИСКИ (см. ARCHITECTURE.md):
//   R1  — T-junctions на стыках LOD → Transvoxel переходные ячейки (Фаза 2.3)
//   R2  — двойная буферизация мешей при DC (Фаза 2.2)
//   R3  — Flecs defer при structural change из воркеров

#include <cstdint>
#include <cstring>
#include <cmath>
#include <memory>
#include <array>
#include <unordered_map>
#include <functional>
#include <cassert>

// Vector3: тесты определяют SVO_NO_RAYLIB перед include
#ifndef SVO_NO_RAYLIB
  #include "raylib.h"
#else
  struct Vector3 { float x, y, z; };
#endif

// ================================================================
//  Константы
// ================================================================

// Размер чанка в вокселях по каждой оси (2^CHUNK_DEPTH)
constexpr int     CHUNK_DEPTH       = 4;               // 2^4 = 16 вокселей
constexpr int     CHUNK_SIZE        = 1 << CHUNK_DEPTH; // 16
constexpr int     CHUNK_SIZE_SQ     = CHUNK_SIZE * CHUNK_SIZE;
constexpr int     CHUNK_SIZE_CUBE   = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;

// Порог сдвига Floating Origin (в вокселях)
constexpr int64_t CHUNK_SHIFT_THRESHOLD = CHUNK_SIZE * 4; // 64 вокселя

// Масштаб: 1 воксель = VOXEL_SCALE единиц рендера
constexpr float   VOXEL_SCALE       = 1.0f;

// ================================================================
//  Hermite-данные в вершине ноды SVO
// ================================================================

struct HermiteData {
    float   density = -1.0f;        // < 0 = воздух, > 0 = твердь, 0 = поверхность
    float   nx = 0, ny = 1, nz = 0; // нормаль в точке пересечения ребра
    float   ex = 0, ey = 0, ez = 0; // точка пересечения ребра (локальные координаты)
    bool    dirty = false;           // нужна перегенерация DC

    bool isSolid() const   { return density > 0.0f; }
    bool isEmpty() const   { return density < 0.0f; }
    bool isSurface() const { return density == 0.0f; }
};

// ================================================================
//  Нода Octree
// ================================================================
//
//  Дочерние ноды индексируются по октантам:
//
//       Y
//       |
//    6--7
//   /| /|    октант = (x<<0) | (y<<1) | (z<<2)
//  4--5 |     0 = (-x,-y,-z)   4 = (-x,-y,+z)
//  | 2-|3     1 = (+x,-y,-z)   5 = (+x,-y,+z)
//  |/  |/     2 = (-x,+y,-z)   6 = (-x,+y,+z)
//  0--1       3 = (+x,+y,-z)   7 = (+x,+y,+z)
//             ---> X

struct OctreeNode {
    // Hermite-данные этой ноды (актуальны только для листьев)
    HermiteData data;

    // Дочерние узлы. nullptr = пустая ветвь (воздух или твердь целиком)
    std::array<std::unique_ptr<OctreeNode>, 8> children;

    // Метаданные
    uint8_t depth = 0;   // глубина в дереве (0 = корень чанка)
    bool    leaf  = true; // нет детей

    // Всё поддерево однородно? (оптимизация)
    bool homogeneous = true;
    bool homogSolid  = false; // если homogeneous: true=твердь, false=воздух

    OctreeNode() = default;

    bool hasChild(int oct) const { return children[oct] != nullptr; }

    // Раздробить узел — создать 8 дочерних
    void subdivide() {
        assert(leaf);
        leaf = false;
        for (int i = 0; i < 8; i++) {
            children[i] = std::make_unique<OctreeNode>();
            children[i]->depth = depth + 1;
            // Наследуем плотность родителя
            children[i]->data.density = data.density;
            children[i]->homogeneous = homogeneous;
            children[i]->homogSolid  = homogSolid;
        }
    }

    // Попытаться свернуть — если все дети однородны и одинаковы
    bool tryMerge() {
        if (leaf) return false;
        for (int i = 0; i < 8; i++) {
            if (!children[i] || !children[i]->homogeneous) return false;
            if (i > 0 && children[i]->homogSolid != children[0]->homogSolid) return false;
        }
        // Все дети одинаковые — свернуть
        bool solid = children[0]->homogSolid;
        for (int i = 0; i < 8; i++) children[i].reset();
        leaf = true;
        homogeneous = true;
        homogSolid  = solid;
        data.density = solid ? 1.0f : -1.0f;
        return true;
    }
};

// ================================================================
//  Ключ чанка (мировые координаты в единицах чанков)
// ================================================================

struct ChunkKey {
    int64_t cx, cy, cz;

    bool operator==(const ChunkKey& o) const {
        return cx == o.cx && cy == o.cy && cz == o.cz;
    }
};

struct ChunkKeyHash {
    size_t operator()(const ChunkKey& k) const {
        // Хэш по трём int64 через XOR-rotate
        size_t h = 0;
        auto mix = [&](int64_t v) {
            h ^= std::hash<int64_t>{}(v) + 0x9e3779b9 + (h << 6) + (h >> 2);
        };
        mix(k.cx); mix(k.cy); mix(k.cz);
        return h;
    }
};

// ================================================================
//  Мировые координаты и Floating Origin
// ================================================================

// Мировая позиция в вокселях (int64 — нет переполнения, нет дрожания)
struct WorldPos {
    int64_t x, y, z;

    ChunkKey toChunkKey() const {
        // floor-деление (правильно для отрицательных тоже)
        auto fd = [](int64_t v, int64_t d) -> int64_t {
            return v / d - (v % d != 0 && (v ^ d) < 0 ? 1 : 0);
        };
        return { fd(x, CHUNK_SIZE), fd(y, CHUNK_SIZE), fd(z, CHUNK_SIZE) };
    }

    // Локальные координаты внутри чанка [0, CHUNK_SIZE)
    int localX() const { int v = (int)(x % CHUNK_SIZE); return v < 0 ? v + CHUNK_SIZE : v; }
    int localY() const { int v = (int)(y % CHUNK_SIZE); return v < 0 ? v + CHUNK_SIZE : v; }
    int localZ() const { int v = (int)(z % CHUNK_SIZE); return v < 0 ? v + CHUNK_SIZE : v; }
};

// Смещение рендер-системы координат относительно мировых
// Обновляется при OriginShift. renderPos = worldPos - originOffset
struct FloatingOrigin {
    int64_t x = 0, y = 0, z = 0;

    // Перевод мировых координат в рендер-float
    Vector3 toRender(WorldPos wp) const {
        return {
            (float)((wp.x - x) * (int64_t)(VOXEL_SCALE * 1000) / 1000.0f),
            (float)((wp.y - y) * (int64_t)(VOXEL_SCALE * 1000) / 1000.0f),
            (float)((wp.z - z) * (int64_t)(VOXEL_SCALE * 1000) / 1000.0f)
        };
    }

    // Быстрая версия для рендера (float арифметика вблизи нуля — точная)
    Vector3 toRenderF(int64_t wx, int64_t wy, int64_t wz) const {
        return {
            (float)(wx - x) * VOXEL_SCALE,
            (float)(wy - y) * VOXEL_SCALE,
            (float)(wz - z) * VOXEL_SCALE
        };
    }

    // Нужен ли сдвиг? Вызывать каждый кадр с позицией игрока
    bool needsShift(WorldPos playerPos) const {
        return std::abs(playerPos.x - x) > CHUNK_SHIFT_THRESHOLD ||
               std::abs(playerPos.y - y) > CHUNK_SHIFT_THRESHOLD ||
               std::abs(playerPos.z - z) > CHUNK_SHIFT_THRESHOLD;
    }

    // Сдвинуть origin к позиции игрока (выровнять по границе чанка)
    void shiftTo(WorldPos playerPos) {
        // Выравниваем на границу чанка для стабильности
        x = (playerPos.x / CHUNK_SIZE) * CHUNK_SIZE;
        y = (playerPos.y / CHUNK_SIZE) * CHUNK_SIZE;
        z = (playerPos.z / CHUNK_SIZE) * CHUNK_SIZE;
    }
};

// Глобальный экземпляр (один на мир)
inline FloatingOrigin g_origin;

// ================================================================
//  Чанк — локальный Octree + метаданные
// ================================================================

enum class ChunkState : uint8_t {
    EMPTY,          // ещё не загружен/генерирован
    GENERATING,     // идёт генерация в воркере
    READY,          // данные готовы, меш не построен
    MESHING,        // идёт DC в воркере
    ACTIVE,         // меш готов, рендерится
    DIRTY,          // данные изменены, нужна перегенерация
};

struct Chunk {
    ChunkKey   key;
    ChunkState state = ChunkState::EMPTY;

    // Корень локального Octree (покрывает CHUNK_SIZE³ вокселей)
    std::unique_ptr<OctreeNode> root;

    // Флаг — нужна перегенерация DC меша
    bool meshDirty = false;

    // Счётчик изменённых нод (для приоритизации DC)
    int  dirtyCount = 0;

    Chunk() : root(std::make_unique<OctreeNode>()) {
        root->depth        = 0;
        root->homogeneous  = true;
        root->homogSolid   = false;
        root->data.density = -1.0f; // воздух по умолчанию
    }
};

// ================================================================
//  SVO — основной класс. Хранит все чанки мира.
// ================================================================

class SVO {
public:
    // --- Вставка / изменение ---

    // Установить плотность в мировой позиции
    void setDensity(WorldPos wp, float density) {
        Chunk& chunk = getOrCreateChunk(wp.toChunkKey());
        setInChunk(chunk, wp.localX(), wp.localY(), wp.localZ(), density);
        chunk.meshDirty = true;
        chunk.dirtyCount++;
    }

    // Установить Hermite-данные полностью
    void setHermite(WorldPos wp, const HermiteData& data) {
        Chunk& chunk = getOrCreateChunk(wp.toChunkKey());
        HermiteData& node = getOrCreateLeaf(chunk, wp.localX(), wp.localY(), wp.localZ());
        node = data;
        node.dirty = true;
        chunk.meshDirty = true;
        chunk.dirtyCount++;
    }

    // --- Чтение ---

    // Получить плотность (возвращает -1 если чанк не загружен)
    float getDensity(WorldPos wp) const {
        const Chunk* chunk = getChunk(wp.toChunkKey());
        if (!chunk) return -1.0f;
        return getDensityInChunk(*chunk, wp.localX(), wp.localY(), wp.localZ());
    }

    // Получить Hermite-данные (nullptr если нет)
    const HermiteData* getHermite(WorldPos wp) const {
        const Chunk* chunk = getChunk(wp.toChunkKey());
        if (!chunk) return nullptr;
        return getLeaf(*chunk, wp.localX(), wp.localY(), wp.localZ());
    }

    bool isSolid(WorldPos wp) const { return getDensity(wp) > 0.0f; }
    bool isEmpty(WorldPos wp) const { return getDensity(wp) < 0.0f; }

    // --- CSG кисти ---

    // Сфера (взрыв, пещера)
    // applySphere:
    //   weight > 0 — добавить материал (постройка, terraforming)
    //   weight < 0 — убрать материал (взрыв, копание)
    //   Плавный градиент на краях для smooth DC меша.
    void applySphere(WorldPos center, float radiusVoxels, float weight) {
        int r = (int)ceilf(radiusVoxels);
        for (int dz = -r; dz <= r; dz++)
        for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++) {
            float dist = sqrtf((float)(dx*dx + dy*dy + dz*dz));
            if (dist > radiusVoxels) continue;

            WorldPos wp = { center.x + dx, center.y + dy, center.z + dz };
            float cur = getDensity(wp);

            // SDF: 1.0 в центре, 0.0 на поверхности, убывает к краю
            float influence = 1.0f - (dist / radiusVoxels);
            // weight > 0 = добавить: clamp(cur + influence, -1, 1)
            // weight < 0 = убрать:  clamp(cur + weight*influence, -1, 1)
            float delta      = weight * influence;
            float newDensity = std::clamp(cur + delta, -1.0f, 1.0f);
            setDensity(wp, newDensity);
        }
    }

    // Куб (постройка стен, зданий)
    void applyCube(WorldPos center, int hw, int hh, int hd, float weight) {
        for (int dz = -hd; dz <= hd; dz++)
        for (int dy = -hh; dy <= hh; dy++)
        for (int dx = -hw; dx <= hw; dx++) {
            WorldPos wp = { center.x + dx, center.y + dy, center.z + dz };
            float cur = getDensity(wp);
            float newDensity = std::clamp(cur + weight, -1.0f, 1.0f);
            setDensity(wp, newDensity);
        }
    }

    // Цилиндр (шахты, колонны)
    void applyCylinder(WorldPos center, float radiusVoxels, int halfHeight, float weight) {
        int r = (int)ceilf(radiusVoxels);
        for (int dy = -halfHeight; dy <= halfHeight; dy++)
        for (int dz = -r; dz <= r; dz++)
        for (int dx = -r; dx <= r; dx++) {
            float dist = sqrtf((float)(dx*dx + dz*dz));
            if (dist > radiusVoxels) continue;
            WorldPos wp = { center.x + dx, center.y + dy, center.z + dz };
            float cur = getDensity(wp);
            float newDensity = std::clamp(cur + weight, -1.0f, 1.0f);
            setDensity(wp, newDensity);
        }
    }

    // --- Чанки ---

    Chunk& getOrCreateChunk(ChunkKey key) {
        auto it = chunks.find(key);
        if (it != chunks.end()) return it->second;
        auto [ins, ok] = chunks.emplace(key, Chunk{});
        ins->second.key = key;
        return ins->second;
    }

    const Chunk* getChunk(ChunkKey key) const {
        auto it = chunks.find(key);
        return it != chunks.end() ? &it->second : nullptr;
    }

    Chunk* getChunkMut(ChunkKey key) {
        auto it = chunks.find(key);
        return it != chunks.end() ? &it->second : nullptr;
    }

    bool hasChunk(ChunkKey key) const { return chunks.count(key) > 0; }

    // Обход всех загруженных чанков
    void forEachChunk(std::function<void(ChunkKey, Chunk&)> fn) {
        for (auto& [key, chunk] : chunks) fn(key, chunk);
    }

    void forEachChunkConst(std::function<void(ChunkKey, const Chunk&)> fn) const {
        for (auto& [key, chunk] : chunks) fn(key, chunk);
    }

    // Выгрузить чанк (вне зоны загрузки)
    void unloadChunk(ChunkKey key) { chunks.erase(key); }

    // Количество загруженных чанков
    size_t chunkCount() const { return chunks.size(); }

    // Собрать список чанков с dirty мешами
    std::vector<ChunkKey> getDirtyChunks() const {
        std::vector<ChunkKey> result;
        for (auto& [key, chunk] : chunks) {
            if (chunk.meshDirty) result.push_back(key);
        }
        return result;
    }

    // Floating Origin — обновить при необходимости
    // Вызывать каждый кадр с мировой позицией игрока
    bool updateOrigin(WorldPos playerWorldPos) {
        if (!g_origin.needsShift(playerWorldPos)) return false;
        g_origin.shiftTo(playerWorldPos);
        // При сдвиге origin — все рендер-координаты пересчитываются автоматически
        // через g_origin.toRenderF() — никакого дрожания float
        return true;
    }

private:
    std::unordered_map<ChunkKey, Chunk, ChunkKeyHash> chunks;

    // ---- Внутренние методы навигации по дереву ----

    // Получить или создать лист на глубине CHUNK_DEPTH для локальных координат
    HermiteData& getOrCreateLeaf(Chunk& chunk, int lx, int ly, int lz) {
        OctreeNode* node = chunk.root.get();
        for (int d = 0; d < CHUNK_DEPTH; d++) {
            if (node->leaf) node->subdivide();
            int bit  = CHUNK_DEPTH - 1 - d;
            int oct  = ((lx >> bit) & 1) |
                       (((ly >> bit) & 1) << 1) |
                       (((lz >> bit) & 1) << 2);
            node = node->children[oct].get();
        }
        node->homogeneous = false;
        return node->data;
    }

    // Прочитать лист (без создания)
    const HermiteData* getLeaf(const Chunk& chunk, int lx, int ly, int lz) const {
        const OctreeNode* node = chunk.root.get();
        for (int d = 0; d < CHUNK_DEPTH; d++) {
            if (node->leaf) return &node->data; // однородный узел
            int bit = CHUNK_DEPTH - 1 - d;
            int oct = ((lx >> bit) & 1) |
                      (((ly >> bit) & 1) << 1) |
                      (((lz >> bit) & 1) << 2);
            if (!node->children[oct]) return &node->data; // нет дочернего — берём родителя
            node = node->children[oct].get();
        }
        return &node->data;
    }

    void setInChunk(Chunk& chunk, int lx, int ly, int lz, float density) {
        HermiteData& d = getOrCreateLeaf(chunk, lx, ly, lz);
        d.density = density;
        d.dirty   = true;
        // Попробовать свернуть дерево снизу вверх (экономия памяти)
        // (полный merge pass — отдельная функция compactChunk)
    }

    float getDensityInChunk(const Chunk& chunk, int lx, int ly, int lz) const {
        const HermiteData* d = getLeaf(chunk, lx, ly, lz);
        return d ? d->density : -1.0f;
    }
};

// ================================================================
//  Глобальный экземпляр SVO (один на игровой мир)
// ================================================================
inline SVO g_svo;
