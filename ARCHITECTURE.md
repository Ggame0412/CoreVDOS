# ARCHITECTURE.md — CoreVDOS / Real Life 2
> **Живой документ.** Обновляется по мере реализации каждого модуля.  
> Статусы: ✅ реализовано | 🔧 частично | 📋 запланировано | ❌ не начато

---

## 🛠 ЧАСТЬ 1: ГРАММАТИКА И СИНТАКСИС VDOSL

### Алфавит и токены

| Токен | Пример | Описание |
|-------|--------|---------|
| `IDENT` | `object`, `cube`, `green` | Идентификатор |
| `STRING` | `"path/to/file"` | Строка в двойных кавычках |
| `NUMBER` | `1.5`, `-3`, `0` | Целое или дробное, может быть отрицательным |
| `SECTION` | `@shape`, `@color` | Начинается с `@` |
| `LBRACE/RBRACE` | `{` `}` | Блок |
| `COLON` | `:` | Разделитель |
| `EQUAL` | `=` | Присваивание именованного параметра |
| `END` | — | Конец потока |

Комментарии: `// однострочный` ✅  
Комментарии: `/* многострочный */` 📋

### Объявление объекта ✅

```dsl
object ИмяОбъекта {
    @shape  cube 1 1 1
    @color  green
    @tranc  0 0.5 0
    @rot    0 45 0
    @physic static
}
```

### Наследование прототипов 📋

```dsl
entity Zombie : Enemy {
    // Копирует все компоненты Enemy, перезаписывает явно указанные
    @color red
}
```

Правило: дочерний объект наследует все секции родителя. Секции с одинаковым именем — перезаписываются.

### Секции

| Секция | Параметры | Описание | Статус |
|--------|-----------|---------|--------|
| `@shape` | `cube/sphere/capsule [w h d]` | Форма | ✅ |
| `@color` | `colorName` или `r g b` | Цвет | ✅ |
| `@texture` | `"path"` | Путь к текстуре | ✅ |
| `@tranc` | `x y z` | Локальное смещение | ✅ |
| `@rot` | `x y z` | Локальный поворот (градусы) | ✅ |
| `@physic` | `static/dynamic/kinematic` | Тип тела | 🔧 |
| `@script` | `"path/to/script.lua"` | Lua-скрипт поведения | 📋 |
| `@net` | `networked/local/remote` | Сетевой тег | 📋 |

### Типы данных

```dsl
// Число
radius = 1.5

// Строка
name = "My Object"

// Массив (для vec3)
position = [1.0, 2.5, -3.0]

// Булево
active = true
```

---

## 🧩 ЧАСТЬ 2: РЕЕСТР КОМПОНЕНТОВ («АТОМОВ»)

> Каждый атом = C++ компонент Flecs (данные) + система (логика).  
> Синглтоны — один на `flecs::world`. Компоненты — на каждой сущности.

---

### CPosition (Компонент) ✅
- **Файл:** `src/ecs/components.h`
- **Описание:** Мировая позиция сущности.
- **Поля:** `x: float`, `y: float`, `z: float`
- **DSL:** `@tranc x y z`
- **События:** нет

---

### CRotation (Компонент) ✅
- **Файл:** `src/ecs/components.h`
- **Описание:** Поворот в радианах (Euler XYZ).
- **Поля:** `x: float`, `y: float`, `z: float`
- **DSL:** `@rot x y z`

---

### CRenderable (Компонент) 🔧
- **Файл:** `src/ecs/components.h`
- **Описание:** Временный рендер-компонент (DrawCube). Заменится на VoxelGrid/RenderChunk.
- **Поля:** `color: Color`, `w/h/d: float`
- **DSL:** `@shape`, `@color`

---

### CPhysicsBody (Компонент) 🔧
- **Файл:** `src/ecs/components.h`
- **Описание:** RP3D rigidbody на сущности.
- **Поля:** `body: rp3d::RigidBody*`, `collider: rp3d::Collider*`
- **DSL:** `@physic static/dynamic/kinematic`
- **Ограничение:** `physCommon` должен быть залочен мьютексом при создании из воркер-треда.

---

### VoxelGrid (Компонент/Синглтон) 📋
- **Файл:** `src/world/voxel_grid.h` (не создан)
- **Описание:** Sparse Voxel Octree (SVO) — хранит Hermite-данные чанка.
  - Нода = `density: float` (знак: `< 0` воздух, `> 0` твердь) + `edge_intersection: vec3` + `edge_normal: vec3`
  - Глубина дерева: 10 уровней (1024³ вокселей макс)
  - Лист = чанк максимальной детализации
- **DSL:** `@density`, `@brush`
- **События:** `"chunk_dirty"` — нода помечена для перегенерации DC

---

### RenderChunk (Система) 📋
- **Файл:** `src/world/render_chunk.h`
- **Описание:** Следит за `VoxelGrid`. При флаге `dirty` асинхронно запускает Dual Contouring, генерирует `std::vector<Vector3> verts` + `indices`, обновляет `rp3d::TriangleMesh`.
- **Зависит от:** VoxelGrid
- **Выход:** треугольный меш для рендера и физики

---

### PhysicsBody (Компонент) 📋 → см. CPhysicsBody
Полная реализация через RP3D bridge:
- **Поля DSL:** `mass: float`, `drag: float`, `angular_drag: float`, `gravity: bool`, `kinematic: bool`
- **События:** нет (физика не генерирует события напрямую — через VolumeTrigger)

---

### VolumeTrigger (Компонент) 📋
- **Файл:** `src/physics/volume_trigger.h`
- **Описание:** Невидимый AABB/Сфера. C++ определяет enter/exit/stay, шлёт в Event Bus.
- **Поля:** `shape: string ("box"/"sphere")`, `size: vec3`, `offset: vec3`
- **DSL:** `@trigger box 2 2 2`
- **События:** `"trigger_enter"`, `"trigger_exit"`, `"trigger_stay"` → в Event Bus

---

### FluidVolume (Компонент) 📋
- **Файл:** `src/physics/fluid_volume.h`
- **Описание:** Архимедова сила. Итерирует все тела внутри объёма. Для рагдоллов — применяет к каждому звену независимо.
- **Поля:** `density: float`, `viscosity: float`, `size: vec3`
- **DSL:** `@fluid density=1.0 viscosity=0.5`

---

### PhysicsJoint (Компонент) 📋
- **Файл:** `src/physics/physics_joint.h`
- **Описание:** RP3D HingeJoint / BallJoint / SliderJoint.
- **Поля:** `type: string ("hinge"/"ball"/"slider")`, `anchor: vec3`, `axis: vec3`, `limits: vec2`
- **DSL:** `@joint type=hinge anchor=[0,1,0] axis=[0,0,1]`

---

### KinematicMover (Компонент) 📋
- **Файл:** `src/physics/kinematic_mover.h`
- **Описание:** C++ интерполятор. Lua задаёт цель, C++ двигает каждый кадр.
- **Поля:** `target: vec3`, `duration: float`, `elapsed: float`, `easing: string`
- **DSL:** `@move target=[5,0,0] duration=2.0 easing=linear`
- **Lua:** `entity:moveTo(vec3, seconds)`

---

### RaycastTarget (Компонент) 📋
- **Файл:** `src/physics/raycast_target.h`
- **Описание:** Хитбокс для интерактивных объектов. C++ определяет наведение прицела.
- **Поля:** `size: vec3`, `offset: vec3`
- **DSL:** `@interactive size=[1,1,1]`
- **События:** `"hover"`, `"click"` → в Event Bus

---

### NavigationMesh (Синглтон) 📋
- **Файл:** `src/ai/navigation_mesh.h`
- **Описание:** Глобальный граф путей (A* по DC мешу). Lua вызывает `entity:pathTo(vec3)`.
- **Зависит от:** RenderChunk (перестраивается при изменении ландшафта)

---

### SkeletalAnimator (Компонент) 📋
- **Файл:** `src/animation/skeletal_animator.h`
- **Описание:** cgltf загрузка + SIMD skinning на CPU / Compute Shader на GPU.
- **Форматы:** `.iqm` (Raylib), `.gltf` (cgltf)
- **Поля:** `model_path: string`, `current_anim: string`, `speed: float`, `loop: bool`
- **DSL:** `@model "assets/models/zombie.gltf" anim=walk`

---

### StateMachine (Компонент) 📋
- **Файл:** `src/ai/state_machine.h`
- **Описание:** Конечный автомат для ИИ. Микросекундное переключение состояний.
- **Поля:** `current_state: string`, `transitions: map<string, Condition>`

---

### InputActionMap (Синглтон) 📋
- **Файл:** `src/ui/input_action_map.h`
- **Описание:** Абстракция ввода. Переводит KEY_W → `MoveForward`, тач-стик → то же.
- **Поддержка:** PC клавиатура/мышь, Android touchscreen (Termux)
- **DSL:** `@input MoveForward=[KEY_W, GAMEPAD_LEFT_Y+]`

---

### ScreenSpaceCanvas (Компонент) 📋
- **Файл:** `src/ui/canvas.h`
- **Описание:** Ортографический рендер поверх 3D. Панели, инвентарь, меню.
- **Поля:** `anchor: string ("top_left"/"center"/...)`, `size: vec2`

---

### CameraViewport (Синглтон) 📋
- **Файл:** `src/renderer/camera_viewport.h`
- **Описание:** FOV, матрица проекции, постэффекты (искажение под водой, motion blur).
- **Поля:** `fov: float (60-120)`, `near_plane: float`, `far_plane: float`, `post_effect: string`

---

### DecalProjector (Компонент) 📋
- **Файл:** `src/renderer/decal_projector.h`
- **Описание:** Наложение текстур поверх геометрии (пулевые отверстия, кровь). Без новых 3D моделей.
- **Поля:** `texture: string`, `size: float`, `lifetime: float (-1 = вечно)`

---

### SkyAtmosphere (Синглтон) 📋
- **Файл:** `src/renderer/sky_atmosphere.h`
- **Описание:** Скайбокс без записи в Depth Buffer. Процедурная смена дня/ночи.
- **Поля:** `time_of_day: float (0-24)`, `horizon_color: vec4`, `zenith_color: vec4`, `sun_direction: vec3`

---

### ParticleEmitter (Компонент) 📋
- **Файл:** `src/renderer/particle_emitter.h`
- **Описание:** GPU instancing тысяч частиц. Lua передаёт только стартовые параметры.
- **Поля:** `texture: string`, `emit_rate: float`, `velocity: vec3`, `gravity: float`, `lifetime: float`

---

### UniversalLight (Компонент) ✅ (спека) 📋 (реализация)
- **Файл:** `src/renderer/universal_light.h`
- **Описание:** Point/Spot/Directional свет с поддержкой запекания теней.
- **Поля:**
  - `type: string ("point"/"spot"/"directional")`
  - `color: vec4 [r,g,b,a]` (0.0–1.0)
  - `intensity: float` (0.0–1000.0)
  - `radius: float` (метры)
  - `cone_angle: float` (для `spot`, градусы)
- **События:** нет

---

### LevelOfDetail (Система) 📋
- **Файл:** `src/renderer/lod_system.h`
- **Описание:** Frustum culling по AABB нод Octree + замена детального меша на упрощённый при удалении.
- **Порог:** настраивается через `lod_distance: float` на чанке

---

### AudioEmitter (Компонент) 📋
- **Файл:** `src/audio/audio_emitter.h`
- **Описание:** 3D источник звука через SoLoud. Затухание в пространстве, эффект Доплера.
- **Поля:** `sound_path: string`, `volume: float (0-1)`, `pitch: float`, `loop: bool`, `radius: float`
- **DSL:** `@sound "assets/audio/footstep.ogg" loop=false`

---

### TrackerSynthesizer (Компонент) 📋
- **Файл:** `src/audio/tracker_synth.h`
- **Описание:** SoLoud::Openmpt — управление каналами .xm/.mod файла из Lua.
- **Поля:** `track_path: string`, `bpm: float`, `muted_channels: int[]`
- **DSL:** `@music "assets/music/battle.xm"`
- **Lua:** `synth:unmuteChannel(4)`, `synth:setBPM(160)`
- **Примечание:** SoLoud + OpenMPT совместимы через `SoLoud::Openmpt` класс.

---

### AudioListener (Компонент) 📋
- **Файл:** `src/audio/audio_listener.h`
- **Описание:** "Уши" движка. Обычно вешается на камеру. Поддержка split-screen: несколько слушателей.
- **Поля:** `volume_multiplier: float`

---

### NetworkReplicator (Компонент-тег) 📋
- **Файл:** `src/net/network_replicator.h`
- **Описание:** Пометить сущность для авторепликации. Snapshot Interpolation для Transform + PhysicsBody.
- **Теги:** `TNetworked`, `TLocal`, `TRemote` (в `src/ecs/components.h`)
- **Закон Stateless Lua:** игровой стейт нельзя хранить в локальных Lua переменных — только в Flecs компонентах.

---

### DeferredTimer (Компонент) 📋
- **Файл:** `src/net/deferred_timer.h`
- **Описание:** ECS-таймер. Мигрирует к новому хосту при Host Migration (не теряет время).
- **Поля:** `duration: float`, `elapsed: float`, `event_name: string`, `event_data: string`
- **Lua:** `timers.after(3.0, function() entity:destroy() end)`

---

## 🧠 ЧАСТЬ 3: SVO & DENSITY API (Octree + Dual Contouring)

**Статус: 📋 Запланировано (Фаза 2)**

### Структура Hermite-данных

Каждая нода Octree хранит:
```cpp
struct HermiteNode {
    float   density;              // < 0 = воздух, > 0 = твердь
    Vector3 edge_intersection;    // точка пересечения ребра изоповерхностью
    Vector3 edge_normal;          // нормаль в точке пересечения
};
```

### Операции CSG (кисти)

```lua
-- Взрыв/пещера — вычитание сферы
octree.applyBrush(BRUSH_SPHERE, center, {radius=2.0, weight=-1.0})

-- Постройка стены — добавление куба
octree.applyBrush(BRUSH_CUBE, center, {size=[2,3,1], weight=1.0})

-- Шахта — цилиндр
octree.applyBrush(BRUSH_CYLINDER, center, {radius=1.0, height=5.0, weight=-1.0})

-- Применить изменения (запускает DC асинхронно)
octree.applyChanges()
```

### Правило Dirty-нод

1. `applyBrush()` → помечает затронутые ноды флагом `dirty`
2. `applyChanges()` → запускает Dual Contouring только для `dirty` нод в воркер-треде
3. DC генерирует меш → `rp3d::TriangleMesh` обновляется (с мьютексом на `physCommon`)
4. Флаг `dirty` снимается

### Thread Safety

```cpp
// Создание/удаление форм RP3D из воркер-треда:
{
    std::lock_guard<std::mutex> lock(g_physics_common_mutex);
    chunkMesh = physCommon.createTriangleMesh(vertexArray);
}
// physWorld->update() — параллельно без блокировок (изолированы по мирам)
```

---

## 📜 ЧАСТЬ 4: LUA API

**Статус: 📋 Запланировано (Фаза 3, после Octree)**

> Биндинги через Sol3. Макрос `REGISTER_FIELD` для автобиндинга полей Flecs компонентов.

Полная спецификация: см. **API.md**

---

## 🌐 ЧАСТЬ 5: СЕТЬ И СЕРИАЛИЗАЦИЯ

**Статус: 📋 Запланировано**

### Теги сущностей

| Тег | Поведение |
|-----|-----------|
| `[Networked]` | C++ реплицирует Transform + PhysicsBody автоматически |
| `[Local]` | Только на своей машине. Не реплицируется. |
| `[Remote]` | Принадлежит другому игроку. Snapshot Interpolation. |

### Закон Stateless Lua

> **Запрещено** хранить игровой стейт в локальных Lua-переменных.  
> Любая переменная, которая должна пережить Host Migration, **обязана** быть полем Flecs-компонента.

### P2P схема

- **Listen-Server**: хост = авторитетная физика + Lua
- **Host Migration**: стейт ECS сериализуется → передаётся новому хосту
- **NAT Punchthrough**: STUN серверы. Fallback: TURN реле.
- **Транспорт**: `libdatachannels` / WebRTC или Steam Networking API

---

## 🗂 ЧАСТЬ 6: СТАНДАРТ ИМПОРТА РЕСУРСОВ

### Структура папок

```
assets/
├── textures/     — .png (UI: 9-patch, мир: тайлы)
├── audio/        — .wav, .ogg (звуки)
├── music/        — .xm, .mod (трекерная музыка)
├── models/       — .iqm (Raylib скелет), .gltf (cgltf)
├── scripts/      — .lua (поведение объектов)
└── maps/         — .vdosl (сцены DSL)
```

### Форматы

| Тип | Форматы | Библиотека |
|-----|---------|-----------|
| Текстуры | `.png` | Raylib |
| Звуки | `.wav`, `.ogg` | SoLoud |
| Музыка | `.xm`, `.mod` | SoLoud::Openmpt |
| Модели | `.iqm` | Raylib |
| Модели (анимация) | `.gltf` | cgltf |
| Сцены | `.vdosl` | VDOSL Parser |

---

## 🛠 ЧАСТЬ 7: ПРИМЕРЫ ЭТАЛОННЫХ ОБЪЕКТОВ

### Пример 1: Кнопка, открывающая дверь ✅ (DSL) 📋 (Lua)

```dsl
// button.vdosl
object Button {
    @shape  cube 0.3 0.1 0.3
    @color  red
    @tranc  0 0.05 0
    @physic static
    @interactive size=[0.4, 0.4, 0.4]
    @script "scripts/button.lua"
}

object Door {
    @shape  cube 1 2 0.1
    @color  brown
    @physic kinematic
    @script "scripts/door.lua"
}
```

```lua
-- scripts/button.lua
entity:on("click", function(user)
    events.emit("door_open", { door_id = "main_door" })
end)

-- scripts/door.lua
events.on("door_open", function(data)
    if data.door_id == entity:getName() then
        entity:moveTo(entity:getPos() + vec3(0, 2.5, 0), 1.5)
    end
end)
```

### Пример 2: Взрыв гранаты (CSG деформация) 📋

```lua
-- Взрыв: вычесть сферу из ландшафта
local pos = entity:getPos()
octree.applyBrush(BRUSH_SPHERE, pos, { radius = 2.5, weight = -1.0 })
octree.applyChanges()  -- DC перегенерирует меш асинхронно

-- Волна урона
local targets = world.findInRadius(pos, 5.0)
for _, target in ipairs(targets) do
    target:emit("damage", { amount = 80, type = "explosion" })
end
```

### Пример 3: Турель с трекерным звуком 📋

```lua
-- Кручение (KinematicMover)
entity:on("update", function(dt)
    local target = world.findByTag("enemy")[1]
    if target then
        entity:setRot(lookAt(entity:getPos(), target:getPos()))
        
        -- Raycast скан
        local hit = world.raycast(entity:getPos(), entity:getForward(), 30)
        if hit.entity and hit.entity:hasTag("enemy") then
            hit.entity:emit("damage", { amount = 10 })
            -- Включить ударные в .xm треке
            synth:unmuteChannel(4)
        end
    end
end)
```

---

---

### MaterialProperties (Компонент) 📋
- **Файл:** `src/renderer/material_properties.h`
- **Описание:** До 8 текстурных слоёв на объект. Слои передаются на GPU по отдельным слотам — без склейки на CPU. Lua может анимировать UV-координаты (лава, вода, голограмма).
- **Поля:**
  - `albedo: string` — базовый цвет (слот 0)
  - `scratches: string` — царапины/повреждения (слот 1)
  - `ao: string` — запечённые тени (ambient occlusion, слот 2)
  - `roughness: string` — шероховатость (слот 3)
  - `emission: string` — свечение (слот 4)
  - `layers[8]: TextureLayer` — произвольные слои
  - `uv_scroll: vec2` — скорость прокрутки UV (анимация)
  - `uv_scale: vec2` — масштаб тайлинга
- **Поля TextureLayer:**
  - `path: string` — путь к текстуре
  - `blend_mode: string ("multiply"/"add"/"overlay")`
  - `uv_offset: vec2`
  - `uv_scroll: vec2`
- **DSL:**
  ```dsl
  @material
    albedo   = "assets/textures/rock_base.png"
    scratches = "assets/textures/rock_scratches.png"
    ao       = "assets/textures/rock_ao.png"
    uv_scale = [2.0, 2.0]
  ```
- **Lua:**
  ```lua
  -- Анимация воды
  entity:on("update", function(dt)
      local mat = entity:getComponent(MaterialProperties)
      mat.uv_scroll = mat.uv_scroll + vec2(0.01, 0.005) * dt
  end)
  ```
- **Зависит от:** ShaderInstance (слоты подаются в шейдер через юниформы)
- **События:** нет

---

### ShaderInstance (Компонент) 📋
- **Файл:** `src/renderer/shader_instance.h`
- **Описание:** Управляет кастомным GLSL-шейдером. Хранит ID скомпилированного шейдера и карту юниформов. Lua меняет параметры в рантайме — визуал объекта меняется без перекомпиляции.
- **Поля:**
  - `shader_path_vert: string` — путь к `.vert`
  - `shader_path_frag: string` — путь к `.frag`
  - `shader_id: int` — ID после компиляции (Raylib `Shader.id`)
  - `uniforms: map<string, UniformValue>` — параметры шейдера
- **Поля UniformValue:**
  - `type: string ("float"/"vec2"/"vec3"/"vec4"/"int"/"texture")`
  - `value: variant<float, vec2, vec3, vec4, int, string>`
- **DSL:**
  ```dsl
  @shader
    vert = "assets/shaders/hologram.vert"
    frag = "assets/shaders/hologram.frag"
    time        = float 0.0      // автоматически обновляется движком
    scan_color  = vec4 0 1 0 0.8
    glitch_amp  = float 0.05
  ```
- **Lua:**
  ```lua
  -- Эффект голограммы
  local s = entity:getComponent(ShaderInstance)
  s:setUniform("scan_color", vec4(0, 1, 0.5, 0.9))
  s:setUniform("glitch_amp", 0.1)

  -- Подсветка кисти редактора на ландшафте
  brush_shader:setUniform("brush_pos",   world.getBrushPos())
  brush_shader:setUniform("brush_radius", 2.5)

  -- Искажение под водой
  water_shader:setUniform("time", world.getTime())
  ```
- **Системное юниформ `time`:** движок автоматически обновляет каждый кадр если поле называется `time`.
- **Зависит от:** MaterialProperties (текстурные слоты), IRenderer
- **Примечания:** Только один ShaderInstance на сущность. Если не задан — используется дефолтный шейдер рендерера.
- **События:** нет

---

## ⚠️ ЧАСТЬ 8: РЕЕСТР АРХИТЕКТУРНЫХ РИСКОВ

> Обязательно читать перед реализацией каждой фазы. Каждый риск имеет статус и митигацию.

---

### R1 — T-junctions в DC мешах на стыках чанков 🔴
- **Фаза:** 2 (Dual Contouring)
- **Проблема:** На стыках чанков разной LOD-глубины DC генерирует Т-образные зазоры. RP3D застревает в них капсулой/колёсами.
- **Митигация:** Реализовать переходные ячейки (Transvoxel-style) для DC на границах чанков. Альтернатива — принудительно выравнивать вершины граничных рёбер соседних нод.
- **Статус:** 📋 не реализовано

---

### R2 — Передача TriangleMesh из воркера в физику 🟣
- **Фаза:** 2
- **Проблема:** DC строит меш в воркер-треде. RP3D использует `TriangleMesh` в main-треде для broad-phase. Удаление буфера вершин пока физика его читает → crash.
- **Митигация:** Двойная буферизация мешей. Пока `meshA` в физике — `meshB` строится в фоне. В безопасной точке кадра (после `physWorld->update()`) меши меняются местами атомарно.
  ```cpp
  // Безопасная точка обмена:
  // [конец physWorld->update()] → swap(activeMesh, pendingMesh) → [начало следующего update()]
  std::atomic<ChunkMesh*> activeMesh;
  ChunkMesh*              pendingMesh; // строится в воркере
  ```
- **Статус:** 📋 не реализовано

---

### R3 — Flecs structural change из параллельных тредов 🟣
- **Фаза:** 1–2
- **Проблема:** Вызов `entity.set<T>()`, `entity.add<Tag>()`, `entity.remove<T>()` из воркер-тредов (DC, сетевой тред) во время `ecs.progress()` → ассерт Flecs или порча памяти.
- **Правило:** Любые структурные изменения ECS из не-main-треда — **только через Command Queue**:
  ```cpp
  // В воркере:
  ecs.defer_begin();
  entity.set<CPosition>({x, y, z});
  ecs.defer_end();  // применяется в main-треде в безопасной точке
  ```
- **Статус:** ⚠️ нет защиты — добавить при первом использовании воркеров

---

### R4 — Zombie-коллбэки Lua после удаления C++ сущности 🟡
- **Фаза:** 3 (Sol3 биндинги)
- **Проблема:** `entity:on("click", function() end)` создаёт жёсткую ссылку Sol3. При `entity.destruct()` Lua-замыкание живёт, ловит события, обращается к удалённой памяти → Segfault.
- **Митигация:**
  1. Вместо анонимных функций — именованные события: `timers.after(3.0, entity, "OnExplode")`
  2. `sol::environment` с weak_ref на сущность. Перед вызовом — проверка `entity.is_alive()`.
  3. При `entity.destruct()` — явная отписка всех коллбэков через Entity Lifecycle Hook.
- **Статус:** 📋 проектировать с нуля правильно

---

### R5 — Dangling Pointer в CPhysicsBody 🔴
- **Фаза:** 1–2
- **Проблема:** `CPhysicsBody` хранит сырые `rp3d::RigidBody*`. Если сущность удалена без явного `physWorld->destroyRigidBody()` — висячий указатель. Рендерер / физика читает удалённую память.
- **Митигация:** Система `PhysicsCleanupSystem` в Flecs — реагирует на `OnRemove(CPhysicsBody)`:
  ```cpp
  ecs.observer<CPhysicsBody>()
     .event(flecs::OnRemove)
     .each([&](flecs::entity e, CPhysicsBody& pb) {
         if (pb.body) physWorld->destroyRigidBody(pb.body);
         pb.body = pb.collider = nullptr;
     });
  ```
- **Статус:** 📋 добавить при интеграции Flecs + RP3D

---

### R6 — Недетерминированность RP3D при Host Migration 🔴
- **Фаза:** 7
- **Проблема:** RP3D зависит от порядка итераций и погрешностей float. При передаче стейта новому хосту накопленные импульсы/контакты джоинтов расходятся → игроков телепортирует, машины улетают.
- **Митигация:**
  1. При миграции — заморозить физику на `0.5s`.
  2. Передать только чистые `position` + `velocity` (без накопленных импульсов и лямбд-мультипликаторов).
  3. На новом хосте — запустить симуляцию «с чистого листа». Snapshot Interpolation сгладит рывок у клиентов.
- **Статус:** 📋 проектировать с нуля правильно

---

### R7 — RCE через незащищённый LuaJIT 🟣
- **Фаза:** 3
- **Проблема:** `os.execute()`, `io.open()`, `require('ffi')` в кастомном Lua-скрипте → удаление файлов, кража данных, запуск вирусов.
- **Митигация:** Жёсткий sandbox при создании `lua_State`:
  ```cpp
  // Запрещено намертво:
  lua["os"]      = sol::nil;
  lua["io"]      = sol::nil;
  lua["debug"]   = sol::nil;
  lua["package"] = sol::nil;
  lua["ffi"]     = sol::nil;
  lua["require"] = sol::nil;  // только белый список
  // Разрешено только через зарегистрированный C++ API
  ```
- **Статус:** 📋 обязательно с первого дня Фазы 3

---

### R8 — Фальсификация мира при Host Migration 🟣
- **Фаза:** 7
- **Проблема:** Злоумышленник-новый-хост подменяет пакет сериализации ECS → прописывает себе ресурсы, ломает карту для всех.
- **Митигация:**
  1. Хэш стейта ECS перед миграцией. Все клиенты сравнивают хэши.
  2. Голосование: новый хост принимается большинством. Пакет с расходящимся хэшем — отвергается.
  3. Критические переменные (HP, ресурсы) — на стороне хоста реплицируются с авторитетной проверкой диапазона.
- **Статус:** 📋 спроектировать при реализации Фазы 7

---

### R9 — Десинхронизация через недетерминированный Lua 🔴
- **Фаза:** 3+7
- **Проблема:** `math.random()` на двух клиентах даёт разные результаты → стена цела на сервере, разрушена у клиента → читеры могут эксплуатировать намеренно.
- **Митигация:**
  1. Запретить стандартный `math.random`. Заменить на `world.random(seed)` — детерминированный PRNG с синхронизированным seed от хоста.
  2. Все рандомные события (разлёт осколков, крит) — вычисляет только хост, рассылает результат.
- **Статус:** 📋 проектировать при Фазе 3

---

### R10 — Желейная физика рагдоллов в флюиде 🟡
- **Фаза:** 5
- **Проблема:** `FluidVolume` с `addForce` к центру масс рагдолла → неестественное «желе». При наличии `PhysicsJoint` внутри флюида — нестабильность сочленений.
- **Митигация:** `FluidSystem` итерирует каждое звено рагдолла независимо, вычисляет погружение (`submergedVolume`) и применяет силу Архимеда к каждому `RigidBody`-звену отдельно.
- **Статус:** 📋 проектировать при Фазе 5

---

## 📊 Статус реализации

| Фаза | Описание | Статус |
|------|---------|--------|
| **Фаза 0** | Raylib + RP3D + ImGui, базовый прототип | ✅ |
| **Фаза 1** | Рефакторинг: модули, IRenderer, ECS компоненты | ✅ |
| **Фаза 2** | SVO Octree + Dual Contouring + TriangleMesh физика | 📋 |
| **Фаза 3** | LuaJIT + Sol3 + Lua API биндинги | 📋 |
| **Фаза 4** | Атомы рендера (SkyAtmosphere, Particles, LOD, MaterialProperties, ShaderInstance) | 📋 |
| **Фаза 5** | Атомы физики (FluidVolume, Joints, NavMesh) | 📋 |
| **Фаза 6** | Аудио (SoLoud + OpenMPT трекер) | 📋 |
| **Фаза 7** | P2P сеть + Host Migration | 📋 |

## 🗺 Таблица рисков

| ID | Компонент | Тип | Тяжесть | Фаза |
|----|-----------|-----|---------|------|
| R1 | DC T-junctions | Алгоритм/Баг | 🔴 Высокая | 2 |
| R2 | TriangleMesh передача | Crash/Race | 🟣 Критическая | 2 |
| R3 | Flecs structural change из треда | Crash/Race | 🟣 Критическая | 1–2 |
| R4 | Zombie Lua коллбэки | Memory/Segfault | 🟡 Средняя | 3 |
| R5 | Dangling Pointer CPhysicsBody | Crash | 🔴 Высокая | 1–2 |
| R6 | RP3D недетерминированность | Геймплей/Crash | 🔴 Высокая | 7 |
| R7 | RCE через Lua | Безопасность | 🟣 Критическая | 3 |
| R8 | Фальсификация Host Migration | Безопасность | 🟣 Критическая | 7 |
| R9 | Десинхронизация math.random | Геймплей/Читы | 🔴 Высокая | 3+7 |
| R10 | Желе рагдоллов в флюиде | Физика/Геймплей | 🟡 Средняя | 5 |
