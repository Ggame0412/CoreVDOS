// tests/test_svo.cpp
// Юнит-тесты SVO. Запуск: скомпилировать и запустить как обычный .exe
// Зависимости: только svo.h (raylib нужен для Vector3 — mock ниже)

// Говорим svo.h не включать raylib, используем внутренний mock
#define SVO_NO_RAYLIB

#include "../src/world/svo.h"
#include <iostream>
#include <cassert>
#include <cmath>

// ----------------------------------------------------------------
//  Мини-фреймворк
// ----------------------------------------------------------------
static int  s_pass = 0, s_fail = 0;
static const char* s_currentSuite = "";

#define SUITE(name) s_currentSuite = name; std::cout << "\n[" << name << "]\n"

#define CHECK(expr) do { \
    if (expr) { s_pass++; std::cout << "  PASS  " << #expr << "\n"; } \
    else      { s_fail++; std::cerr << "  FAIL  " << #expr \
                    << "  (" << __FILE__ << ":" << __LINE__ << ")\n"; } \
} while(0)

#define CHECK_NEAR(a, b, eps) CHECK(std::fabs((a)-(b)) < (eps))

// ================================================================
//  Тесты ChunkKey и WorldPos
// ================================================================
void test_chunk_key() {
    SUITE("ChunkKey / WorldPos");

    // Положительные координаты
    WorldPos wp1 = {16, 0, 0};
    ChunkKey k1  = wp1.toChunkKey();
    CHECK(k1.cx == 1 && k1.cy == 0 && k1.cz == 0);
    CHECK(wp1.localX() == 0 && wp1.localY() == 0 && wp1.localZ() == 0);

    WorldPos wp2 = {15, 0, 0};
    ChunkKey k2  = wp2.toChunkKey();
    CHECK(k2.cx == 0 && k2.cy == 0 && k2.cz == 0);
    CHECK(wp2.localX() == 15);

    WorldPos wp3 = {17, 5, 33};
    ChunkKey k3  = wp3.toChunkKey();
    CHECK(k3.cx == 1 && k3.cy == 0 && k3.cz == 2);
    CHECK(wp3.localX() == 1 && wp3.localY() == 5 && wp3.localZ() == 1);

    // Отрицательные координаты — floor-деление
    WorldPos wpNeg = {-1, -1, -1};
    ChunkKey kNeg  = wpNeg.toChunkKey();
    CHECK(kNeg.cx == -1 && kNeg.cy == -1 && kNeg.cz == -1);
    CHECK(wpNeg.localX() == CHUNK_SIZE - 1);

    WorldPos wpNeg2 = {-16, -16, -16};
    ChunkKey kNeg2  = wpNeg2.toChunkKey();
    CHECK(kNeg2.cx == -1 && kNeg2.cy == -1 && kNeg2.cz == -1);
    CHECK(wpNeg2.localX() == 0);

    WorldPos wpNeg3 = {-17, 0, 0};
    ChunkKey kNeg3  = wpNeg3.toChunkKey();
    CHECK(kNeg3.cx == -2);
    CHECK(wpNeg3.localX() == CHUNK_SIZE - 1);
}

// ================================================================
//  Тесты ChunkKeyHash (нет коллизий у соседних чанков)
// ================================================================
void test_chunk_hash() {
    SUITE("ChunkKeyHash");

    ChunkKeyHash h;
    ChunkKey a = {0,0,0}, b = {1,0,0}, c = {0,1,0}, d = {0,0,1};
    CHECK(h(a) != h(b));
    CHECK(h(a) != h(c));
    CHECK(h(a) != h(d));
    CHECK(h(b) != h(c));

    // Равные ключи — одинаковый хэш
    ChunkKey a2 = {0,0,0};
    CHECK(h(a) == h(a2));

    // Отрицательные
    ChunkKey neg = {-1, -1, -1};
    CHECK(h(neg) != h(a));
    CHECK(h({-1,0,0}) != h({1,0,0}));
}

// ================================================================
//  Тесты OctreeNode (subdivide / tryMerge)
// ================================================================
void test_octree_node() {
    SUITE("OctreeNode subdivide/merge");

    OctreeNode root;
    CHECK(root.leaf == true);
    CHECK(root.homogeneous == true);
    CHECK(root.homogSolid == false);

    // Subdivide
    root.subdivide();
    CHECK(root.leaf == false);
    for (int i = 0; i < 8; i++) CHECK(root.children[i] != nullptr);
    CHECK(root.children[0]->depth == 1);

    // Все дети одинаковые — merge
    bool merged = root.tryMerge();
    CHECK(merged == true);
    CHECK(root.leaf == true);

    // Разные дети — нельзя merge
    root.subdivide();
    root.children[0]->homogeneous = true;
    root.children[0]->homogSolid  = true;   // один твёрдый
    root.children[1]->homogeneous = true;
    root.children[1]->homogSolid  = false;  // остальные воздух
    bool merged2 = root.tryMerge();
    CHECK(merged2 == false);
    CHECK(root.leaf == false);
}

// ================================================================
//  Тесты SVO: чтение/запись
// ================================================================
void test_svo_read_write() {
    SUITE("SVO setDensity / getDensity");

    SVO svo;

    // По умолчанию — воздух
    CHECK(svo.getDensity({0,0,0}) < 0.0f);
    CHECK(svo.isEmpty({0,0,0}));

    // Установить твердь
    svo.setDensity({0,0,0}, 1.0f);
    CHECK(svo.isSolid({0,0,0}));
    CHECK_NEAR(svo.getDensity({0,0,0}), 1.0f, 0.001f);

    // Установить другой воксель в том же чанке
    svo.setDensity({5, 3, 7}, 0.5f);
    CHECK_NEAR(svo.getDensity({5,3,7}), 0.5f, 0.001f);
    CHECK(svo.getDensity({0,0,0}) > 0.0f); // предыдущий не изменился

    // Другой чанк
    svo.setDensity({16, 0, 0}, 1.0f);
    CHECK(svo.isSolid({16,0,0}));
    CHECK(svo.isEmpty({15,0,0})); // граница чанка

    // Отрицательные координаты
    svo.setDensity({-1, -1, -1}, 1.0f);
    CHECK(svo.isSolid({-1,-1,-1}));
    CHECK(svo.isEmpty({-2,-1,-1}));
}

// ================================================================
//  Тесты SVO: несозданные чанки
// ================================================================
void test_svo_unloaded_chunks() {
    SUITE("SVO несозданные чанки");

    SVO svo;

    // Чанк не существует → getDensity возвращает воздух
    CHECK(svo.getDensity({1000, 1000, 1000}) < 0.0f);
    CHECK(svo.getHermite({500, 500, 500}) == nullptr);
    CHECK(!svo.hasChunk({999, 999, 999}));

    // После записи чанк создаётся
    svo.setDensity({1000, 1000, 1000}, 1.0f);
    ChunkKey key = WorldPos{1000,1000,1000}.toChunkKey();
    CHECK(svo.hasChunk(key));
    CHECK(svo.chunkCount() == 1);
}

// ================================================================
//  Тесты SVO: dirty-флаги
// ================================================================
void test_svo_dirty() {
    SUITE("SVO dirty флаги");

    SVO svo;

    // Изначально нет грязных чанков
    auto dirty = svo.getDirtyChunks();
    CHECK(dirty.empty());

    // После записи — чанк грязный
    svo.setDensity({0,0,0}, 1.0f);
    dirty = svo.getDirtyChunks();
    CHECK(dirty.size() == 1);

    // Второй чанк
    svo.setDensity({16,0,0}, 1.0f);
    dirty = svo.getDirtyChunks();
    CHECK(dirty.size() == 2);

    // Сбросить флаг вручную
    ChunkKey k = WorldPos{0,0,0}.toChunkKey();
    svo.getChunkMut(k)->meshDirty = false;
    dirty = svo.getDirtyChunks();
    CHECK(dirty.size() == 1);
}

// ================================================================
//  Тесты CSG кистей
// ================================================================
void test_svo_brushes() {
    SUITE("CSG Brushes");

    SVO svo;

    // applySphere — внутри радиуса становится твердью (weight > 0)
    svo.applySphere({0,0,0}, 3.0f, 2.0f); // weight=2: из -1.0 в +1.0 (воздух → твердь)
    CHECK(svo.isSolid({0,0,0}));
    CHECK(svo.isSolid({1,0,0}));
    // dist=2, radius=3, weight=2: influence=0.33 → delta=0.67 → -1+0.67=-0.33 = воздух (SDF градиент)
    CHECK(!svo.isSolid({2,0,0})); // граница сферы плавная, не бинарная
    // За радиусом — воздух
    CHECK(svo.isEmpty({5,0,0}));

    // applySphere — вычитание (взрыв) weight < 0
    // Сначала заполним куб
    svo.applyCube({10,0,10}, 5, 5, 5, 2.0f); // weight=2: -1.0+2.0=1.0 гарантированно
    CHECK(svo.isSolid({10,0,10}));
    // Взрыв в центре
    svo.applySphere({10,0,10}, 3.0f, -2.0f); // -2 гарантирует < 0 даже если было 1.0
    CHECK(svo.isEmpty({10,0,10})); // центр взрыва — воздух
    CHECK(svo.isSolid({14,0,10}));  // дальше — твердь осталась

    // applyCylinder
    SVO svo2;
    svo2.applyCylinder({0,0,0}, 2.0f, 4, 2.0f); // weight=2: гарантированно solid
    CHECK(svo2.isSolid({0,0,0}));
    CHECK(svo2.isSolid({0,4,0}));
    CHECK(svo2.isEmpty({0,5,0})); // выше высоты
    CHECK(svo2.isEmpty({3,0,0})); // за радиусом
}

// ================================================================
//  Тесты Floating Origin
// ================================================================
void test_floating_origin() {
    SUITE("FloatingOrigin");

    FloatingOrigin orig;
    orig.x = orig.y = orig.z = 0;

    // Близко к нулю — сдвига нет
    WorldPos near = {10, 5, 10};
    CHECK(!orig.needsShift(near));

    // Далеко — нужен сдвиг
    WorldPos far = {CHUNK_SHIFT_THRESHOLD + 10, 0, 0};
    CHECK(orig.needsShift(far));

    // После shiftTo — больше не нужен (выровнено по чанку)
    orig.shiftTo(far);
    CHECK(!orig.needsShift(far));

    // toRenderF вблизи origin — float точный
    // После сдвига игрок в {74,0,0}, origin в {64,0,0}
    Vector3 rp = orig.toRenderF(far.x, far.y, far.z);
    CHECK_NEAR(rp.x, (float)(CHUNK_SHIFT_THRESHOLD + 10 - 64) * VOXEL_SCALE, 0.001f);
    CHECK_NEAR(rp.y, 0.0f, 0.001f);
    CHECK_NEAR(rp.z, 0.0f, 0.001f);

    // Убеждаемся что origin выровнен по CHUNK_SIZE
    CHECK(orig.x % CHUNK_SIZE == 0);
    CHECK(orig.y % CHUNK_SIZE == 0);
    CHECK(orig.z % CHUNK_SIZE == 0);
}

// ================================================================
//  Тесты: граничные случаи
// ================================================================
void test_edge_cases() {
    SUITE("Edge cases");

    SVO svo;

    // Запись прямо на границе двух чанков
    svo.setDensity({15, 0, 0}, 1.0f);  // последний воксель чанка 0
    svo.setDensity({16, 0, 0}, 1.0f);  // первый воксель чанка 1

    ChunkKey k0 = WorldPos{15,0,0}.toChunkKey();
    ChunkKey k1 = WorldPos{16,0,0}.toChunkKey();
    CHECK(k0.cx != k1.cx);  // разные чанки
    CHECK(svo.isSolid({15,0,0}));
    CHECK(svo.isSolid({16,0,0}));
    CHECK(svo.isEmpty({14,0,0}));
    CHECK(svo.isEmpty({17,0,0}));

    // Запись на границе отрицательного и положительного чанка
    svo.setDensity({-1, 0, 0}, 1.0f);
    svo.setDensity({0, 0, 0}, 1.0f);
    ChunkKey kNeg = WorldPos{-1,0,0}.toChunkKey();
    ChunkKey kPos = WorldPos{0,0,0}.toChunkKey();
    CHECK(kNeg.cx == -1 && kPos.cx == 0);
    CHECK(svo.isSolid({-1,0,0}));
    CHECK(svo.isSolid({0,0,0}));

    // Очень большие координаты (далеко от нуля — int64 не переполнится)
    WorldPos huge = {1000000000LL, 500000000LL, -999999999LL};
    svo.setDensity(huge, 1.0f);
    CHECK(svo.isSolid(huge));
    CHECK(svo.isEmpty({huge.x + 1, huge.y, huge.z}));
}

// ================================================================
//  main
// ================================================================
int main() {
    std::cout << "=== SVO Unit Tests ===\n";

    test_chunk_key();
    test_chunk_hash();
    test_octree_node();
    test_svo_read_write();
    test_svo_unloaded_chunks();
    test_svo_dirty();
    test_svo_brushes();
    test_floating_origin();
    test_edge_cases();

    std::cout << "\n======================\n";
    std::cout << "PASS: " << s_pass << "\n";
    std::cout << "FAIL: " << s_fail << "\n";

    if (s_fail == 0)
        std::cout << "\n✅ ВСЕ ТЕСТЫ ПРОШЛИ\n";
    else
        std::cout << "\n❌ ЕСТЬ ПРОВАЛЫ\n";

    return s_fail > 0 ? 1 : 0;
}
