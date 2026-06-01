#pragma once
// renderer/renderer.h
// Абстракция рендера. Текущая реализация: RaylibRenderer.
// Будущие: OpenGLRenderer, VulkanRenderer — меняется только реализация.

#include "raylib.h"

class IRenderer {
public:
    virtual ~IRenderer() = default;

    // Кадр
    virtual void beginFrame(Camera3D& cam, Color clearColor = SKYBLUE) = 0;
    virtual void endFrame()   = 0;

    // 3D
    virtual void drawCube(Vector3 pos, Vector3 size, Color color, float rotY = 0.0f) = 0;
    virtual void drawCubeWires(Vector3 pos, Vector3 size, Color color, float rotY = 0.0f) = 0;
    virtual void drawGrid(int slices, float spacing) = 0;
    virtual void drawSphere(Vector3 pos, float radius, Color color) = 0;
    virtual void drawLine3D(Vector3 from, Vector3 to, Color color) = 0;

    // 2D HUD (вызывать между beginFrame/endFrame, снаружи BeginMode3D)
    virtual void drawText(const char* text, int x, int y, int size, Color color) = 0;
    virtual void drawCircle(int x, int y, float radius, Color color) = 0;
};
