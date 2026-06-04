cmake_minimum_required(VERSION 3.16)
project(Engine)



set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==============================================================================
# ---- Определение путей к библиотекам в зависимости от платформы ----
# ==============================================================================

# По умолчанию ищем в корне соответствующих папок (для Linux/Android)
set(FLECS_LIB_PATH "${CMAKE_SOURCE_DIR}/third-party/flecs/linux-build/libflecs_static.a")
set(RP3D_LIB_PATH  "${CMAKE_SOURCE_DIR}/third-party/rp3d/build/libreactphysics3d.a")

# Если собираем под Windows (через llvm-mingw или нативно)
if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Путь к Raylib Windows
    set(RAYLIB_LIB_PATH "${CMAKE_SOURCE_DIR}/third-party/raylib/lib/libraylibdll.a")
    set(RAYLIB_INC_PATH "${CMAKE_SOURCE_DIR}/third-party/raylib/src")
    
    # ТОЧНЫЙ ПУТЬ К СТАТИЧЕСКОМУ FLECS (исправлено):
    set(FLECS_LIB_PATH "${CMAKE_SOURCE_DIR}/third-party/flecs/win-build/libflecs_static.a")
    
    # Путь к RP3D под Windows
    set(RP3D_LIB_PATH "${CMAKE_SOURCE_DIR}/third-party/rp3d/win-build/libreactphysics3d.a")

else()
    # Для Linux/Android используем системный find_package или предполагаем, 
    # что они собраны прямо в корне своих папок в third-party
    set(RAYLIB_LIB_PATH "raylib") # Если ставишь через менеджер пакетов Termux/Linux
    # Если Raylib тоже лежит собранный в third-party, укажи путь к .a для Linux аналогично
endif()

# ==============================================================================
# ---- Импорт сторонних библиотек (Таргеты) ----
# ==============================================================================

# ---- raylib ----
if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_library(raylib STATIC IMPORTED)
    set_target_properties(raylib PROPERTIES
        IMPORTED_LOCATION "${RAYLIB_LIB_PATH}"
        INTERFACE_INCLUDE_DIRECTORIES "${RAYLIB_INC_PATH}"
    )
else()
    # На Linux/Android проще найти установленный в систему raylib
    find_package(raylib REQUIRED)
endif()

# ---- flecs ----
add_library(flecs_static STATIC IMPORTED)
set_target_properties(flecs_static PROPERTIES
    IMPORTED_LOCATION "${FLECS_LIB_PATH}"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/third-party/flecs/include"
)

# ---- ReactPhysics3D ----
add_library(rp3d STATIC IMPORTED)
set_target_properties(rp3d PROPERTIES
    IMPORTED_LOCATION "${RP3D_LIB_PATH}"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/third-party/rp3d/include"
)

file(GLOB IMGUI_SOURCES "${CMAKE_SOURCE_DIR}/third-party/imgui/*.cpp")
add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC "${CMAKE_SOURCE_DIR}/third-party/imgui")

# ---- rlImGui (Сборка из исходников) ----
# Предполагается структура: third-party/rlImGui/rlImGui.cpp и rlImGui.h
add_library(rlImGui STATIC 
    "${CMAKE_SOURCE_DIR}/third-party/rlImGui/rlImGui.cpp"
)
target_include_directories(rlImGui PUBLIC "${CMAKE_SOURCE_DIR}/third-party/rlImGui")
target_link_libraries(rlImGui PRIVATE raylib imgui)

# ==============================================================================
# ---- Исходники и создание исполняемого файла ----
# ==============================================================================

add_executable(Engine
    main.cpp
    src/physics/physics.cpp
    src/player/player_camera.cpp
)

# ---- Заголовки проекта ----
target_include_directories(Engine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third-party/flecs/include
    ${CMAKE_SOURCE_DIR}/third-party/rp3d/include
    ${CMAKE_SOURCE_DIR}/third-party/imgui
    ${CMAKE_SOURCE_DIR}/third-party/rlImGui
    
)

# ---- Линковка основных либ ----
target_link_libraries(Engine PRIVATE
    raylib
    flecs_static
    rp3d
    imgui
    rlImGui
)

# ==============================================================================
# ---- Системные и платформенные зависимости ----
# ==============================================================================

if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Важно: Для MinGW/LLVM-MinGW порядок линковки системных либ имеет значение!
    target_link_libraries(Engine PRIVATE winmm gdi32 user32 shell32 ws2_32 dbghelp)
    
    # Статическая сборка, чтобы .exe запускался на чистой винде без LLVM/MinGW рантайм-либ
    target_link_options(Engine PRIVATE -static -static-libgcc -static-libstdc++)
    
elseif(APPLE)
    target_link_libraries(Engine PRIVATE
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
elseif(UNIX AND NOT APPLE)
    # Сюда попадут и Linux, и Android (Termux)
    target_link_libraries(Engine PRIVATE
        GL m pthread dl rt X11
    )
endif()

# ---- Дебаг режим ----
option(PM_DEBUG "Enable physics debug" OFF)
if(PM_DEBUG)
    target_compile_definitions(Engine PRIVATE PM_DEBUG_ENABLED)
endif()
